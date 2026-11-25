/*
 * bl_ota.c
 *
 *  Created on: 03-Jan-2026
 *      Author: arunrawat
 */


#include "flash_layout.h"
#include "main.h"
#include "flash_operations.h"
#include "bl_ota.h"
#include "app_header.h"
#include "string.h"
#include "bl_image.h"
#include "bl_errors.h"

#define CRC32_POLY 0xEDB88320

void bl_crc_init(uint32_t *crc)
{
	*crc = 0xFFFFFFFF;
}

void bl_crc_update(uint32_t *crc, uint8_t *data, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		*crc ^= data[i];
		for (uint32_t j = 0; j < 8; j++)
		{
			if (*crc & 1)
				*crc = (*crc >> 1) ^ CRC32_POLY;
			else
				*crc >>= 1;
		}
	}
}

uint32_t bl_crc_finalize(uint32_t crc)
{
	return crc ^ 0xFFFFFFFF;
}


/* Fix A: check the whole OTA slot in place, before anything is erased.
   Part 5: same check as every other image area (codes -10 to -13, see bl_image.h). */
int bl_ota_check_slot(void)
{
    return image_check(OTA_START_ADDR);
}

int bl_flash_erase_app (void)
{
	HAL_FLASH_Unlock();
	int err = flash_erase_app();
	HAL_FLASH_Lock();
	return err;
}



int bl_ota_begin(bl_ota_ctx_t *ctx, uint32_t image_size, uint32_t expected_crc)
{
	if (bl_flash_erase_app() != BL_OK)
		return BL_ERR_APP_ERASE;              // -8: the app area may be partly erased
	ctx->write_addr     = APP_START_ADDR;
	ctx->total_received = 0;
	ctx->image_size     = image_size;
	ctx->expected_crc   = expected_crc;
	bl_crc_init(&ctx->running_crc);
	return BL_OK;
}



int bl_flash_write_app_header(uint32_t magic, uint32_t size, uint32_t crc, uint32_t version){
	app_header_t hdr;
	int err = BL_OK;

	hdr.ota_flag = 0;          // retired, always 0
	hdr.magic    = APP_MAGIC;
	hdr.size     = size;
	hdr.crc      = crc;
	hdr.version  = version;
	memset(hdr.reserved, 0xFF, sizeof(hdr.reserved));

	HAL_FLASH_Unlock();
	if (flash_erase_header() != BL_OK)                                        // erases all of S1
		err = BL_ERR_HDR_WRITE;
	else if (flash_write_flashword(APP_HEADER_ADDR, (const uint8_t *)&hdr) != 0)
		err = BL_ERR_HDR_WRITE;
	HAL_FLASH_Lock();
	return err;
}

int bl_ota_write_chunk(bl_ota_ctx_t *ctx, uint8_t *data, uint32_t len)
{
	if ((ctx->total_received + len) > ctx->image_size)
		return -1;

	/* Update CRC ONLY on real data bytes */
	bl_crc_update(&ctx->running_crc, data, len);

	/* Write data to flash, handling last unaligned word safely */
	HAL_FLASH_Unlock();
	//uint32_t i = 0;

	if (flash_write_bytes(ctx->write_addr, data, len) != 0)
	    {
	        HAL_FLASH_Lock();
	        return -1;
	    }

	ctx->write_addr += (len + FLASH_WORD_BYTES - 1) & ~(FLASH_WORD_BYTES - 1);

	HAL_FLASH_Lock();
	ctx->total_received += len;
	return 0;
}

int bl_ota_finalize(bl_ota_ctx_t *ctx, ota_image_hdr_t *hdr)
{
	if (ctx->total_received != ctx->image_size)
		return -1;

	uint32_t final_crc = bl_crc_finalize(ctx->running_crc);

	if (final_crc != ctx->expected_crc)
	{
		/* OTA failed → stay in bootloader */
		return -2;
	}

	/* Commit application */
	if (bl_flash_write_app_header(hdr->magic, ctx->image_size, final_crc, hdr->version) != BL_OK)
		return BL_ERR_HDR_WRITE;              // -9: the copy was good, the header could not be written
	return 0;
}

int bl_ota_run(bl_ota_ctx_t *ctx, ota_stream_t *stream)
{
    ota_image_hdr_t hdr;

    /* Read OTA header */
    if (stream->read((uint8_t *)&hdr, sizeof(hdr)) != 0)
        return -1;

    /* Validate header */
    if (hdr.magic != APP_MAGIC)
        return -2;

    if (hdr.image_size == 0 || hdr.image_size > APP_MAX_SIZE)
        return -3;

    /* The whole slot must be good before bl_ota_begin() erases the application */
    int slot_err = bl_ota_check_slot();
    if (slot_err != 0)
        return slot_err;

    /* Calculate total OTA image size */
    uint32_t total_size =
        sizeof(ota_image_hdr_t) + hdr.image_size;

    /* Inform stream about bounds */
    if (stream->set_total_size)
        stream->set_total_size(total_size);

    /* Start OTA: erases the application area */
    if (bl_ota_begin(ctx, hdr.image_size, hdr.crc) != BL_OK)
        return BL_ERR_APP_ERASE;

    /* Receive firmware payload */
    uint32_t remaining = hdr.image_size;
    uint8_t buf[256];

    while (remaining)
    {
        uint32_t len = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;

        if (stream->read(buf, len) != 0)
            return -4;

        if (bl_ota_write_chunk(ctx, buf, len) != 0)
            return -5;

        remaining -= len;
    }

    /* Finalize OTA: CRC check of the copied bytes, then the new header */
    int fin = bl_ota_finalize(ctx, &hdr);
    if (fin == BL_ERR_HDR_WRITE)
        return BL_ERR_HDR_WRITE;              // -9
    if (fin != 0)
        return -6;                            // copy check failed

    return 0;
}


/* ---- Slot reader: the install always reads from the OTA slot ------------- */
/* Moved here from main.c 11-25. bl_ota_run() pulls bytes through these. */

static uint32_t slot_cursor = 0;                         // next byte to read, from OTA_START_ADDR
static uint32_t slot_total  = sizeof(ota_image_hdr_t);   // how far the reader may go

static void slot_set_total_size(uint32_t size)
{
    slot_total = size;                                   // header + payload, set by bl_ota_run()
}

static int slot_read(uint8_t *buf, uint32_t len)
{
    if (slot_cursor + len > slot_total)
        return -1;                                       // never read past the image

    memcpy(buf, (const uint8_t *)(OTA_START_ADDR + slot_cursor), len);   // flash is memory-mapped
    slot_cursor += len;
    return 0;
}

int bl_ota_install_from_slot(void)
{
    ota_stream_t stream =
    {
        .read           = slot_read,
        .set_total_size = slot_set_total_size
    };
    bl_ota_ctx_t ctx;

    slot_cursor = 0;                          // start at the slot header, every time
    slot_total  = sizeof(ota_image_hdr_t);    // only the header may be read until its size is known

    return bl_ota_run(&ctx, &stream);
}
