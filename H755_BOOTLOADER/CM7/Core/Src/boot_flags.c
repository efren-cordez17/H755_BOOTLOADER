/*
 * boot_flags.c
 *
 *  append-only boot flag log in bank 1 S1, after the app header.
 *  Reads use rec_at() only, so later hardening against half-written words has one place to go.
 */


#include "boot_flags.h"
#include "main.h"                // HAL_FLASH_Unlock / HAL_FLASH_Lock
#include "flash_layout.h"        // FLAG_AREA_START, FLAG_AREA_END, FLAG_REC_SIZE
#include "flash_operations.h"
#include "bl_errors.h"
#include <string.h>

_Static_assert(FLAG_REC_SIZE == sizeof(boot_flag_rec_t), "flag record size must match the struct");

/* The record stored at addr (flash is memory-mapped, so this is a plain read) */
static const boot_flag_rec_t *rec_at(uint32_t addr)
{
    return (const boot_flag_rec_t *)addr;
}

/* 1 if all 32 bytes read 0xFF (erased, never written) */
static int rec_is_blank(const boot_flag_rec_t *rec)
{
    const uint32_t *w = (const uint32_t *)rec;

    for (uint32_t i = 0; i < FLAG_REC_SIZE / 4U; i++)
        if (w[i] != 0xFFFFFFFFU)
            return 0;
    return 1;
}

/* 1 if marker, state and ~state agree and the state is a real one */
static int rec_is_valid(const boot_flag_rec_t *rec)
{
    return rec->marker == FLAG_REC_MARKER &&
           (rec->state ^ rec->state_inv) == 0xFFFFFFFFU &&
           rec->state >= FLAG_STATE_IMAGE_READY &&
           rec->state <= FLAG_STATE_REJECTED;
}

int flags_read_last(boot_flag_rec_t *out, uint32_t *next_free, uint32_t *count, uint32_t *bad)
{
    boot_flag_rec_t last;
    uint32_t n_valid = 0, n_bad = 0;
    uint32_t free_addr = FLAG_AREA_END;           // stays here if no blank word is found
    int rc = BL_ERR_FLAG_FULL;                    // -20 unless a blank word turns up

    memset(&last, 0, sizeof(last));               // state 0 = FLAG_STATE_IDLE

    for (uint32_t addr = FLAG_AREA_START; addr < FLAG_AREA_END; addr += FLAG_REC_SIZE)
    {
        const boot_flag_rec_t *rec = rec_at(addr);

        if (rec_is_blank(rec))
        {
            free_addr = addr;                     // end of the log
            rc = BL_OK;
            break;
        }
        if (rec_is_valid(rec))
        {
            last = *rec;                          // newest valid record so far
            n_valid++;
        }
        else
            n_bad++;                              // e.g. a write cut short: skip it
    }

    if (out)       *out = last;
    if (next_free) *next_free = free_addr;
    if (count)     *count = n_valid;
    if (bad)       *bad = n_bad;
    return rc;
}

int flags_append(uint32_t state, uint32_t img_crc, uint32_t img_version, int32_t detail)
{
    boot_flag_rec_t rec;
    uint32_t addr;
    int rc;

    if (state < FLAG_STATE_IMAGE_READY || state > FLAG_STATE_REJECTED)
        return BL_ERR_FLAG_WRITE;                 // -21: refuse to write a record the scan would call bad

    rc = flags_read_last(NULL, &addr, NULL, NULL);
    if (rc != BL_OK)
        return rc;                                // -20: log full; addr is a blank word otherwise

    rec.marker      = FLAG_REC_MARKER;
    rec.state       = state;
    rec.state_inv   = ~state;
    rec.img_crc     = img_crc;
    rec.img_version = img_version;
    rec.detail      = detail;
    rec.reserved[0] = 0xFFFFFFFFU;
    rec.reserved[1] = 0xFFFFFFFFU;

    HAL_FLASH_Unlock();
    rc = flash_write_flashword(addr, (const uint8_t *)&rec);
    HAL_FLASH_Lock();
    if (rc != 0)
        return BL_ERR_FLAG_WRITE;                 // -21

    if (memcmp(rec_at(addr), &rec, sizeof(rec)) != 0)
        return BL_ERR_FLAG_READBACK;              // -22
    return BL_OK;
}
