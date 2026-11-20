/*
 * flash_layout.h
 *
 *  Created on: 16-Dec-2025
 *      Author: controllerstech
 */

#ifndef INC_FLASH_LAYOUT_H_
#define INC_FLASH_LAYOUT_H_

/* F411RE: 512 KB = sectors 0-3 (16 KB), 4 (64 KB), 5-7 (128 KB) */

/* H755 bank 1: eight sectors of 128 KB each */
#define BL_START_ADDR       0x08000000   // Sector 0   : 128 KB bootloader
#define APP_HEADER_ADDR     0x08020000   // Sector 1   : 128 KB app header + OTA flag
#define APP_START_ADDR      0x08040000   // Sector 2-3 : 256 KB application
#define APP_MAX_SIZE        (256*1024 - 16)
#define OTA_START_ADDR      0x08080000   // Sector 4-5 : 256 KB OTA download slot
#define OTA_MAX_SIZE        (256*1024 - 16)
/* Sector 6 (0x080C0000) spare. Sector 7 (0x080E0000) ArQ configuration: never erase. */

#define APP_HEADER_SECTOR   FLASH_SECTOR_1
#define APP_FIRST_SECTOR    FLASH_SECTOR_2
#define APP_NB_SECTORS      2
#define OTA_FIRST_SECTOR    FLASH_SECTOR_4
#define OTA_NB_SECTORS      2

#define FLASH_WORD_BYTES    32           // H7 programs 256 bits at a time
//#define FLASH_WORD_BYTES    (FLASH_NB_32BITWORD_IN_FLASHWORD * 4U)

#endif /* INC_FLASH_LAYOUT_H_ */
