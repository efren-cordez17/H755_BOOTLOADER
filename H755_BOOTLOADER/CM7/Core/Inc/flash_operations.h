/*
 * flash_operations.h
 *
 *  Created on: 03-Jan-2026
 *      Author: arunrawat
 */

#ifndef INC_FLASH_OPERATIONS_H_
#define INC_FLASH_OPERATIONS_H_

#include <stdint.h>

/* Erase: the caller unlocks flash first (HAL_FLASH_Unlock) and locks it after.
   Each returns BL_OK (0), BL_ERR_ERASE (-30) or BL_ERR_ERASE_REFUSED (-32). */
int flash_erase_sectors(uint32_t bank, uint32_t first, uint32_t count);
int flash_erase_header(void);   // bank 1 S1     (app header + flag records)
int flash_erase_app(void);      // bank 1 S2-S3  (application)
int flash_erase_ota(void);      // bank 1 S4-S5  (OTA slot)
int flash_erase_backup(void);   // bank 2 S6     (backup)
int flash_erase_rx(void);       // bank 2 S7     (receive area)

/* 1 = allowed, 0 = refused. Pure check, touches no hardware. */
int flash_erase_allowed(uint32_t bank, uint32_t first, uint32_t count);

/* Write: addr must be a multiple of 32. Return 0 or -1. */
int flash_write_flashword(uint32_t addr, const uint8_t *data32);
int flash_write_bytes(uint32_t addr, const uint8_t *buf, uint32_t len);

/* HAL_FLASH_GetError() saved at the last failed erase or write. */
uint32_t flash_last_hal_error(void);

#endif /* INC_FLASH_OPERATIONS_H_ */
