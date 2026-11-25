/*
 * app_header.h
 *
 *  Created on: 20-Dec-2025
 *      Author: jefje
 *
 *  2026-11-25: 32-byte image header (magic, version, size, CRC32, HW revision).
 *  Shared with APP TO FLASH: keep both copies identical.
 */

#ifndef INC_APP_HEADER_H_
#define INC_APP_HEADER_H_

#include <stdint.h>
#include <stddef.h>          // offsetof
#include "flash_layout.h"

#define APP_MAGIC 	0xABCDEFAB

/* App header: one copy at APP_HEADER_ADDR (S1). Describes the installed app. */
typedef struct
{
    uint32_t ota_flag;      // always written 0, no longer read
    uint32_t magic;
    uint32_t size;          // application size in bytes
    uint32_t crc;           // CRC32 of application
    uint32_t version;
    uint32_t reserved[3];   // pads the header to 32 bytes = one H7 flash word
} app_header_t;

/* Image header: at the start of every image (bank 2 S7 receive, bank 2 S6 backup, OTA slot) */
typedef struct
{
    uint32_t magic;         // 0x00  APP_MAGIC
    uint32_t version;       // 0x04  firmware version (plain integer for now)
    uint32_t image_size;    // 0x08  payload bytes only, 1..IMG_MAX_PAYLOAD
    uint32_t crc;           // 0x0C  CRC32 of the payload only (zlib.crc32)
    uint32_t hw_rev;        // 0x10  must equal BOARD_HW_REV
    uint32_t reserved[3];   // 0x14  0xFFFFFFFF, pads to one flash word
} ota_image_hdr_t;


/* ---- Compile-time checks: the C layout must match the packaging script ---- */
_Static_assert(sizeof(app_header_t)    == FLASH_WORD_BYTES, "app_header_t must be one flash word");
_Static_assert(sizeof(ota_image_hdr_t) == IMG_HDR_SIZE,     "ota_image_hdr_t must be 32 bytes");
_Static_assert(offsetof(ota_image_hdr_t, version)    == 0x04, "version must be at offset 0x04");
_Static_assert(offsetof(ota_image_hdr_t, image_size) == 0x08, "image_size must be at offset 0x08");
_Static_assert(offsetof(ota_image_hdr_t, crc)        == 0x0C, "crc must be at offset 0x0C");
_Static_assert(offsetof(ota_image_hdr_t, hw_rev)     == 0x10, "hw_rev must be at offset 0x10");

#endif /* INC_APP_HEADER_H_ */
