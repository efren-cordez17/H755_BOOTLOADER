/*
 * flash_operations.h
 *
 *  Created on: 03-Jan-2026
 *      Author: arunrawat
 */

#ifndef INC_FLASH_OPERATIONS_H_
#define INC_FLASH_OPERATIONS_H_

uint32_t flash_read_ota_flag(void);
uint32_t flash_erase_app (void);
uint32_t flash_erase_ota (void);      // F411: re-enabled, erases the internal OTA slot
uint32_t flash_erase_header (void);
int flash_write_flashword(uint32_t addr, const uint8_t *data32);
int flash_write_bytes(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif /* INC_FLASH_OPERATIONS_H_ */
