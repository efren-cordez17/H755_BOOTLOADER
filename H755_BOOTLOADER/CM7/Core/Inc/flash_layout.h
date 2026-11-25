/*
 * flash_layout.h
 *
 *  Created on: 16-Dec-2025
 *      Author: epc
 *
 *  2026-11-25: dual-bank OTA map for the STM32H755.
 *  Shared with APP to flash: keep both copies identical.
 */

#ifndef INC_FLASH_LAYOUT_H_
#define INC_FLASH_LAYOUT_H_

/*
 * STM32H755ZI: 2 MB flash = 2 banks x 8 sectors x 128 KB (0x20000).
 * Address = bank base + sector x 0x20000.
 *
 * Bank 1 (base 0x08000000)
 *   S0     0x08000000  bootloader                          never erased by firmware
 *   S1     0x08020000  app header (32 B) + boot flag records from 0x08020020
 *   S2-S3  0x08040000  application (running)
 *   S4-S5  0x08080000  OTA slot           (32 B image header + payload)
 *   S6     0x080C0000  spare: not used by OTA (the erase guard refuses it)
 *   S7     0x080E0000  ArQ configuration                   never erased by firmware
 * Bank 2 (base 0x08100000)
 *   S0-S5  0x08100000  not used by OTA (the CM4 project is linked here)
 *   S6     0x081C0000  backup of the app  (32 B image header + payload)
 *   S7     0x081E0000  receive area       (32 B image header + payload), written by the app*
 * FLASH_SECTOR_x and FLASH_BANK_x come from the HAL (include main.h first).
 * Do not use them in #if lines or in the _Static_assert checks below.
 */


/* ---- Bank 1 regions (existing) ------------------------------------ */
#define BL_START_ADDR       0x08000000   // S0    : bootloader
#define APP_HEADER_ADDR     0x08020000   // S1    : app header + boot flag records
#define APP_START_ADDR      0x08040000   // S2-S3 : application
#define OTA_START_ADDR      0x08080000   // S4-S5 : OTA slot

#define APP_HEADER_SECTOR   FLASH_SECTOR_1
#define APP_FIRST_SECTOR    FLASH_SECTOR_2
#define APP_NB_SECTORS      2
#define OTA_FIRST_SECTOR    FLASH_SECTOR_4
#define OTA_NB_SECTORS      2

#define HDR_BANK            FLASH_BANK_1
#define APP_BANK            FLASH_BANK_1
#define OTA_BANK            FLASH_BANK_1


/* ---- Receive area: bank 2, sector 7 (new) -------------------------- */
#define RX_START_ADDR       0x081E0000
#define RX_BANK             FLASH_BANK_2
#define RX_SECTOR           FLASH_SECTOR_7   // sector numbers restart at 0 in each bank
#define RX_NB_SECTORS       1


/* ---- Backup area: bank 2, sector 6 (moved from bank 1 S6, 2026-11-25) -- */
#define BKP_START_ADDR      0x081C0000
#define BKP_BANK            FLASH_BANK_2
#define BKP_SECTOR          FLASH_SECTOR_6   // sector numbers restart at 0 in each bank
#define BKP_NB_SECTORS      1



/* ---- Boot flag records: rest of S1 after the header (new) ---------- */
#define FLAG_REC_SIZE       32                                   // one flash word
#define FLAG_AREA_START     (APP_HEADER_ADDR + FLAG_REC_SIZE)    // 0x08020020
#define FLAG_AREA_END       0x08040000                           // first address after S1
#define FLAG_REC_COUNT      ((FLAG_AREA_END - FLAG_AREA_START) / FLAG_REC_SIZE)   // 4095


/* ---- Image size limits (new) --------------------------------------- */
#define IMG_HDR_SIZE        32                                   // one flash word
#define IMG_MAX_TOTAL       (128 * 1024)                         // one sector: bank 2 S6 (backup) and S7 (receive)
#define IMG_MAX_PAYLOAD     (IMG_MAX_TOTAL - IMG_HDR_SIZE)       // 131,040 bytes

#define APP_MAX_SIZE        IMG_MAX_PAYLOAD   // was (256*1024 - 16)
#define OTA_MAX_SIZE        IMG_MAX_PAYLOAD   // was (256*1024 - 16); not used yet


/* ---- Protected sectors: bank 1, never erased (new) ----------------- */
#define BL_SECTOR           FLASH_SECTOR_0   // bootloader
#define ARQ_CFG_SECTOR      FLASH_SECTOR_7   // ArQ configuration
#define ARQ_CFG_ADDR        0x080E0000


#define FLASH_WORD_BYTES    32           // H7 programs 256 bits at a time

/* ---- Compile-time checks (plain numbers only) ---------------------- */
_Static_assert(RX_START_ADDR  == 0x08100000 + 7 * 0x20000, "RX_START_ADDR must be bank 2 sector 7");
_Static_assert(BKP_START_ADDR == 0x08100000 + 6 * 0x20000, "BKP_START_ADDR must be bank 2 sector 6");
_Static_assert(BKP_START_ADDR + IMG_MAX_TOTAL <= RX_START_ADDR, "backup area must end before the receive area");
_Static_assert(ARQ_CFG_ADDR   == 0x08000000 + 7 * 0x20000, "ARQ_CFG_ADDR must be bank 1 sector 7");
_Static_assert(FLAG_AREA_START == 0x08020020, "flag records start right after the 32-byte header");
_Static_assert(FLAG_REC_COUNT == 4095, "S1 holds 4095 flag records after the header");
_Static_assert(IMG_HDR_SIZE == FLASH_WORD_BYTES, "image header must be exactly one flash word");
_Static_assert(IMG_MAX_PAYLOAD == 131040, "128 KB minus the 32-byte header");

#endif /* INC_FLASH_LAYOUT_H_ */
