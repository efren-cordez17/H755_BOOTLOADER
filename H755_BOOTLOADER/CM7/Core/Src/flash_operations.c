/*
 * flash_operations.c
 *
 *  Created on: 03-Jan-2026
 *      Author: arunrawat
 */

#include "main.h"
#include "flash_operations.h"
#include "flash_layout.h"
#include "string.h"

static void flash_clear_errors(void)
{
    __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
}

static uint32_t flash_erase_sectors(uint32_t first, uint32_t count)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t error = 0;

    flash_clear_errors();
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = FLASH_BANK_1;
    erase.Sector       = first;
    erase.NbSectors    = count;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASHEx_Erase(&erase, &error);
    return error;                              // 0xFFFFFFFF = success
}

uint32_t flash_erase_app(void)    { return flash_erase_sectors(APP_FIRST_SECTOR, APP_NB_SECTORS); }
uint32_t flash_erase_ota(void)    { return flash_erase_sectors(OTA_FIRST_SECTOR, OTA_NB_SECTORS); }
uint32_t flash_erase_header(void) { return flash_erase_sectors(APP_HEADER_SECTOR, 1); }

uint32_t flash_read_ota_flag(void)
{
    return *(__IO uint32_t *)APP_HEADER_ADDR;
}

/* addr must be a multiple of 32. Writes exactly 32 bytes. */
int flash_write_flashword(uint32_t addr, const uint8_t *data32)
{
    uint32_t word[8];
    memcpy(word, data32, FLASH_WORD_BYTES);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr, (uint32_t)word) != HAL_OK)
        return -1;
    return 0;
}

/* addr must be a multiple of 32. A final partial block is padded with 0xFF. */
int flash_write_bytes(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t block[FLASH_WORD_BYTES];
    uint32_t i = 0;

    while (i < len)
    {
        uint32_t n = (len - i >= FLASH_WORD_BYTES) ? FLASH_WORD_BYTES : (len - i);
        memset(block, 0xFF, sizeof(block));
        memcpy(block, buf + i, n);
        if (flash_write_flashword(addr, block) != 0)
            return -1;
        addr += FLASH_WORD_BYTES;
        i += n;
    }
    return 0;
}


/*
void flash_write_word(uint32_t addr, uint32_t data)          // unchanged
{
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
}

void flash_write_bytes(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t aligned_addr = addr & ~3;
    uint32_t padding = addr - aligned_addr;
    uint32_t temp_word;
    uint32_t i = 0;

    // Handle unaligned start
    if (padding)
    {
        uint32_t n = 4 - padding;
        if (n > len) n = len;      // F411: fix, never copy more bytes than the caller passed
        temp_word = 0xFFFFFFFF;
        memcpy(((uint8_t*)&temp_word) + padding, buf, n);
        flash_write_word(aligned_addr, temp_word);
        aligned_addr += 4;
        i += n;
    }

    // Write full words
    for (; i + 4 <= len; i += 4)
    {
        memcpy(&temp_word, buf + i, 4);
        flash_write_word(aligned_addr, temp_word);
        aligned_addr += 4;
    }

    // Last partial word
    if (i < len)
    {
        temp_word = 0xFFFFFFFF;
        memcpy(&temp_word, buf + i, len - i);
        flash_write_word(aligned_addr, temp_word);
    }
}
*/
