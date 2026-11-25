/*
 * boot_flags.h
 *
 *  boot flag records. The state of an update is the last valid
 *  32-byte record in bank 1 S1, after the app header (0x08020020 onwards).
 *  Shared with the blink app: keep both copies identical, and include only <stdint.h> here.
 */

#ifndef INC_BOOT_FLAGS_H_
#define INC_BOOT_FLAGS_H_

#include <stdint.h>

#define FLAG_REC_MARKER         0x464C4147U   // "FLAG" in ASCII; bytes 47 41 4C 46 in flash (little-endian)

/* States. Who writes each one: see the boot flags table in the guide. */
#define FLAG_STATE_IDLE         0x00U   // no valid record; never written to flash
#define FLAG_STATE_IMAGE_READY  0x01U   // app: S7 written and checked
#define FLAG_STATE_BACKUP_DONE  0x02U   // bootloader: backup copied and checked
#define FLAG_STATE_SLOT_READY   0x03U   // bootloader: slot copied and checked
#define FLAG_STATE_INSTALLED    0x04U   // bootloader: app checked, new header written
#define FLAG_STATE_TRIAL        0x05U   // Phase 15 (defined only)
#define FLAG_STATE_CONFIRMED    0x06U   // Phase 15 (defined only)
#define FLAG_STATE_ROLLED_BACK  0x07U   // Phase 15 (defined only)
#define FLAG_STATE_REJECTED     0x08U   // bootloader: failure before the app area was erased

/* One record = one 32-byte flash word, written once, never changed. */
typedef struct
{
    uint32_t marker;         // 0x00  FLAG_REC_MARKER
    uint32_t state;          // 0x04  FLAG_STATE_IMAGE_READY .. FLAG_STATE_REJECTED
    uint32_t state_inv;      // 0x08  ~state (sanity check)
    uint32_t img_crc;        // 0x0C  crc field of the image this update is about
    uint32_t img_version;    // 0x10  version field of that image
    int32_t  detail;         // 0x14  0, an error code for REJECTED, or -40 (backup skipped)
    uint32_t reserved[2];    // 0x18  0xFFFFFFFF
} boot_flag_rec_t;

_Static_assert(sizeof(boot_flag_rec_t) == 32, "boot_flag_rec_t must be one 32-byte flash word");

/* Reads the log. out = last valid record (state FLAG_STATE_IDLE if none), next_free = first blank word,
   count = valid records, bad = words that are neither blank nor valid. Any pointer may be NULL.
   Returns 0, or -20 if the log is full (next_free is then 0x08040000). Never writes. */
int flags_read_last(boot_flag_rec_t *out, uint32_t *next_free, uint32_t *count, uint32_t *bad);

/* Adds one record in the first blank word. Returns 0, -20 log full, -21 write failed or
   invalid state, -22 read-back differs. Never erases. */
int flags_append(uint32_t state, uint32_t img_crc, uint32_t img_version, int32_t detail);

#endif /* INC_BOOT_FLAGS_H_ */
