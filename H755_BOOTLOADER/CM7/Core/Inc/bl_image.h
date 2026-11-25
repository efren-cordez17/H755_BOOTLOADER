/*
 * bl_image.h
 *
 *  2026-11-25: one image check for every image area, and one copy helper
 *  that moves an image into the backup or the OTA slot.
 */

#ifndef INC_BL_IMAGE_H_
#define INC_BL_IMAGE_H_

#include <stdint.h>
#include "app_header.h"          // ota_image_hdr_t (and flash_layout.h)

/* A flash area that holds one image: where it starts, and which sectors an erase must cover. */
typedef struct
{
    uint32_t addr;               // first byte; the 32-byte image header goes here
    uint32_t bank;               // FLASH_BANK_1 or FLASH_BANK_2
    uint32_t first_sector;       // FLASH_SECTOR_0..7, counted inside the bank
    uint32_t nb_sectors;
} flash_region_t;

extern const flash_region_t region_slot;     // bank 1 S4-S5, 0x08080000
extern const flash_region_t region_backup;   // bank 2 S6,    0x081C0000

/* Checks the image whose header starts at base (flash or RAM).
   0 = good; -10 magic, -11 size, -12 hw_rev, -13 payload CRC. Reads only, never writes. */
int image_check(uint32_t base);

/* Erases dst, writes the payload (from payload_src) and then the header (from hdr), reads the copy back.
   hdr may point into flash or RAM. expected_crc is the CRC the caller expects (flag record or S1 header).
   0 = good copy. Before anything is erased: -11 size, -14 CRC not expected, -33 source inside dst.
   After: -30/-32 erase, -31 write, -10..-13 read-back check failed. */
int copy_image(const ota_image_hdr_t *hdr, uint32_t payload_src,
               const flash_region_t *dst, uint32_t expected_crc);

/* Builds a backup image header from the S1 app header (APP_HEADER_ADDR).
   Call it only after bootloader_is_app_valid() returned 0. */
void make_backup_header(ota_image_hdr_t *out);

#endif /* INC_BL_IMAGE_H_ */
