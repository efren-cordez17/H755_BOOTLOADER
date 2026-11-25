/*
 * bl_image.c
 *
 *  Created on: Nov 26, 2026
 *      Author: jefje
 *  one image check for every image area (receive area, backup, OTA slot)
 *  and one copy helper. See bl_image.h for the return codes.
 */

#include "bl_image.h"
#include "main.h"                // HAL_FLASH_Unlock / HAL_FLASH_Lock, FLASH_SECTOR_SIZE
#include "board_cfg.h"           // BOARD_HW_REV
#include "crc32.h"
#include "flash_operations.h"
#include "bl_errors.h"

#define COPY_CHUNK_BYTES    4096U
_Static_assert(COPY_CHUNK_BYTES % FLASH_WORD_BYTES == 0, "each chunk must end on a flash word boundary");

const flash_region_t region_slot   = { OTA_START_ADDR, OTA_BANK, OTA_FIRST_SECTOR, OTA_NB_SECTORS };
const flash_region_t region_backup = { BKP_START_ADDR, BKP_BANK, BKP_SECTOR,       BKP_NB_SECTORS };

int image_check(uint32_t base)
{
    const ota_image_hdr_t *hdr = (const ota_image_hdr_t *)base;

    /* Cheapest checks first; the CRC reads the whole payload, so it goes last */
    if (hdr->magic != APP_MAGIC)
        return BL_ERR_IMG_MAGIC;                                     // -10
    if (hdr->image_size == 0 || hdr->image_size > IMG_MAX_PAYLOAD)
        return BL_ERR_IMG_SIZE;                                      // -11
    if (hdr->hw_rev != BOARD_HW_REV)
        return BL_ERR_IMG_HWREV;                                     // -12
    if (crc32((const uint8_t *)(base + IMG_HDR_SIZE), hdr->image_size) != hdr->crc)
        return BL_ERR_IMG_CRC;                                       // -13
    return BL_OK;
}

/* Payload in 4 KB chunks. Flash must be unlocked and the area erased. */
static int write_payload(uint32_t dst, uint32_t src, uint32_t size)
{
    uint32_t done = 0;

    while (done < size)
    {
        uint32_t n = size - done;
        if (n > COPY_CHUNK_BYTES)
            n = COPY_CHUNK_BYTES;
        if (flash_write_bytes(dst + done, (const uint8_t *)(src + done), n) != 0)
            return BL_ERR_COPY_WRITE;                                // -31
        done += n;
        /* Phase 14: refresh the watchdog here, once per chunk */
    }
    return BL_OK;
}

int copy_image(const ota_image_hdr_t *hdr, uint32_t payload_src,
               const flash_region_t *dst, uint32_t expected_crc)
{
    ota_image_hdr_t h = *hdr;                     // RAM copy: still valid after the erase
    uint32_t dst_end  = dst->addr + dst->nb_sectors * FLASH_SECTOR_SIZE;
    int rc;

    /* 1. Checks before touching flash: if one fails, nothing is erased */
    if (h.image_size == 0 || h.image_size > IMG_MAX_PAYLOAD)
        return BL_ERR_IMG_SIZE;                                      // -11
    if (h.crc != expected_crc)
        return BL_ERR_CRC_MISMATCH;                                  // -14: not the image the caller expects
    if (payload_src < dst_end && dst->addr < payload_src + h.image_size)
        return BL_ERR_COPY_OVERLAP;                                  // -33: the erase would destroy the source

    /* 2. Erase, write the payload, write the header last */
    HAL_FLASH_Unlock();
    rc = flash_erase_sectors(dst->bank, dst->first_sector, dst->nb_sectors);   // -30, -32
    if (rc == BL_OK)
        rc = write_payload(dst->addr + IMG_HDR_SIZE, payload_src, h.image_size);   // -31
    if (rc == BL_OK && flash_write_flashword(dst->addr, (const uint8_t *)&h) != 0)
        rc = BL_ERR_COPY_WRITE;                                      // -31
    HAL_FLASH_Lock();
    if (rc != BL_OK)
        return rc;

    /* 3. Read the copy back from flash */
    return image_check(dst->addr);                                   // -10..-13
}

void make_backup_header(ota_image_hdr_t *out)
{
    const app_header_t *app = (const app_header_t *)APP_HEADER_ADDR;

    out->magic       = app->magic;
    out->version     = app->version;
    out->image_size  = app->size;
    out->crc         = app->crc;
    out->hw_rev      = BOARD_HW_REV;             // the running app was installed on this board
    out->reserved[0] = 0xFFFFFFFF;
    out->reserved[1] = 0xFFFFFFFF;
    out->reserved[2] = 0xFFFFFFFF;
}
