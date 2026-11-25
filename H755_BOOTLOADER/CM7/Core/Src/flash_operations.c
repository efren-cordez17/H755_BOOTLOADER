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
#include "bl_errors.h"

static uint32_t last_hal_error = 0;       // HAL_FLASH_GetError() at the last failure

uint32_t flash_last_hal_error(void)
{
    return last_hal_error;
}

static void flash_clear_errors(uint32_t bank)
{
    if (bank == FLASH_BANK_2)
        __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_ALL_ERRORS_BANK2);
    else
        __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
}

/* Erase guard. Bank 1: only S1-S5 (header, app, slot); never S0 bootloader, S6 spare, S7 ArQ config.
   Bank 2: only the backup (S6) or the receive area (S7), one sector at a time. Anything else is refused. */
int flash_erase_allowed(uint32_t bank, uint32_t first, uint32_t count)
{
    if (count == 0 || first > FLASH_SECTOR_7 || count > 8U - first)
        return 0;                                          // empty range or past the end of the bank

    uint32_t last = first + count - 1;

    if (bank == FLASH_BANK_1)
        return (first != BL_SECTOR) && (last < OTA_FIRST_SECTOR + OTA_NB_SECTORS);   // S1..S5
    if (bank == FLASH_BANK_2)
        return ((first == BKP_SECTOR) && (count == BKP_NB_SECTORS)) ||
               ((first == RX_SECTOR)  && (count == RX_NB_SECTORS));

    return 0;                                              // FLASH_BANK_BOTH or an unknown value
}

int flash_erase_sectors(uint32_t bank, uint32_t first, uint32_t count)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;

    if (!flash_erase_allowed(bank, first, count))
        return BL_ERR_ERASE_REFUSED;                       // -32: nothing erased

    flash_clear_errors(bank);
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = bank;
    erase.Sector       = first;
    erase.NbSectors    = count;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        last_hal_error = HAL_FLASH_GetError();
        return BL_ERR_ERASE;                               // -30
    }
    return BL_OK;
}


int flash_erase_header(void) { return flash_erase_sectors(HDR_BANK, APP_HEADER_SECTOR, 1); }
int flash_erase_app(void)    { return flash_erase_sectors(APP_BANK, APP_FIRST_SECTOR, APP_NB_SECTORS); }
int flash_erase_ota(void)    { return flash_erase_sectors(OTA_BANK, OTA_FIRST_SECTOR, OTA_NB_SECTORS); }
int flash_erase_backup(void) { return flash_erase_sectors(BKP_BANK, BKP_SECTOR, BKP_NB_SECTORS); }
int flash_erase_rx(void)     { return flash_erase_sectors(RX_BANK, RX_SECTOR, RX_NB_SECTORS); }


/* addr must be a multiple of 32. Writes exactly 32 bytes. HAL picks the bank from addr. */
int flash_write_flashword(uint32_t addr, const uint8_t *data32)
{
    uint32_t word[8];
    memcpy(word, data32, FLASH_WORD_BYTES);
    flash_clear_errors(addr >= FLASH_BANK2_BASE ? FLASH_BANK_2 : FLASH_BANK_1);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr, (uint32_t)word) != HAL_OK)
    {
        last_hal_error = HAL_FLASH_GetError();
        return -1;
    }
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
