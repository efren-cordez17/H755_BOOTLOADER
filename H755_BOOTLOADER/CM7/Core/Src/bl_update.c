/*
 * bl_update.c
 *
 * install sequence A-D. Each step logs one line after it finishes,
 * and adds its flag record only after its own check has passed.
 */


#include "bl_update.h"
#include "main.h"                // HAL_GetTick, HAL_FLASH_Unlock / Lock
#include "flash_layout.h"
#include "app_header.h"
#include "bl_image.h"            // image_check, copy_image, make_backup_header, regions
#include "bl_ota.h"              // bl_ota_install_from_slot
#include "bl_jump.h"             // bootloader_is_app_valid
#include "flash_operations.h"    // flash_erase_backup
#include "bl_errors.h"
#include "bl_log.h"

/* ---- Helpers ------------------------------------------------------------ */

/* Same rule as bootloader_is_app_valid(): the reset handler (2nd word of the
   vector table) must point into the app area, or the image was linked for another address. */
static int vector_in_app_area(uint32_t payload)
{
    uint32_t reset_handler = *(const uint32_t *)(payload + 4);

    return reset_handler >= APP_START_ADDR && reset_handler < APP_START_ADDR + APP_MAX_SIZE;
}

/* Adds the record for a finished step. Returns 0, or the flags_append() code (-20..-22). */
static int record(uint32_t state, const boot_flag_rec_t *last, int32_t detail)
{
    int rc = flags_append(state, last->img_crc, last->img_version, detail);

    if (rc != BL_OK)
        bl_logf("%s not recorded: %d %s\r\n", bl_state_text(state), rc, bl_err_text(rc));
    return rc;
}

/* A failure while the old app is still intact: record REJECTED and let the old app boot. */
static int reject(uint32_t img_crc, uint32_t img_version, int code)
{
    int rc = flags_append(FLAG_STATE_REJECTED, img_crc, img_version, code);

    bl_logf("REJECTED %d %s\r\n", code, bl_err_text(code));
    if (rc != BL_OK)
        bl_logf("REJECTED not recorded: %d %s\r\n", rc, bl_err_text(rc));
    return BL_OK;                                          // the old app boots
}

/* bl_ota_run() codes that come back before bl_ota_begin() erases the app area */
static int app_area_untouched(int rc)
{
    return rc == BL_ERR_SLOT_HDR_READ || rc == BL_ERR_SLOT_MAGIC || rc == BL_ERR_SLOT_SIZE ||
           (rc <= BL_ERR_IMG_MAGIC && rc >= BL_ERR_IMG_CRC);   // -1, -2, -3, -10..-13
}

/* ---- Steps -------------------------------------------------------------- */

/* A: is the image in bank 2 S7 still the one the IMAGE_READY record names? */
static int step_a(const boot_flag_rec_t *last)
{
    const ota_image_hdr_t *s7 = (const ota_image_hdr_t *)RX_START_ADDR;
    uint32_t t0 = HAL_GetTick();
    int rc = image_check(RX_START_ADDR);                           // -10..-13

    if (rc == BL_OK && s7->crc != last->img_crc)
        rc = BL_ERR_CRC_MISMATCH;                                  // -14: S7 changed after IMAGE_READY
    if (rc == BL_OK && !vector_in_app_area(RX_START_ADDR + IMG_HDR_SIZE))
        rc = BL_ERR_IMG_VECTOR;                                    // -15: not linked for the app area

    if (rc == BL_OK)
        bl_logf("A S7 OK v%lu %luB %lu ms\r\n", (unsigned long)s7->version,
                (unsigned long)s7->image_size, (unsigned long)(HAL_GetTick() - t0));
    else
        bl_logf("A S7 FAIL %d %s %lu ms\r\n", rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));
    return rc;
}

/* B: copy the running app into the backup (bank 2 S6), then record BACKUP_DONE. */
static int step_b(const boot_flag_rec_t *last)
{
    uint32_t t0 = HAL_GetTick();
    int32_t detail = BL_OK;
    int app = bootloader_is_app_valid();
    int rc;

    if (app != 0)
    {
        /* Nothing valid to back up. Empty the backup so an older one is never taken for this app. */
        HAL_FLASH_Unlock();
        rc = flash_erase_backup();                                 // -30, -32
        HAL_FLASH_Lock();
        detail = BL_WARN_NO_BACKUP;                                // -40, kept in the record
        bl_logf("B BACKUP SKIP: no valid app (%d %s), backup erased: %d %lu ms\r\n",
                app, bl_err_text(app), rc, (unsigned long)(HAL_GetTick() - t0));
    }
    else
    {
        ota_image_hdr_t h;

        make_backup_header(&h);
        rc = copy_image(&h, APP_START_ADDR, &region_backup, h.crc);   // -11, -14, -30..-33, -10..-13
        if (rc == BL_OK)
            bl_logf("B BACKUP OK v%lu %luB %lu ms\r\n", (unsigned long)h.version,
                    (unsigned long)h.image_size, (unsigned long)(HAL_GetTick() - t0));
        else
            bl_logf("B BACKUP FAIL %d %s %lu ms\r\n", rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));
    }

    if (rc != BL_OK)
        return rc;
    return record(FLAG_STATE_BACKUP_DONE, last, detail);
}

/* C: copy S7 into the OTA slot, then record SLOT_READY. */
static int step_c(const boot_flag_rec_t *last)
{
    uint32_t t0 = HAL_GetTick();
    int rc = copy_image((const ota_image_hdr_t *)RX_START_ADDR, RX_START_ADDR + IMG_HDR_SIZE,
                        &region_slot, last->img_crc);              // -11, -14, -30..-33, -10..-13

    if (rc == BL_OK)
        bl_logf("C SLOT OK %lu ms\r\n", (unsigned long)(HAL_GetTick() - t0));
    else
        bl_logf("C SLOT FAIL %d %s %lu ms\r\n", rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));

    if (rc != BL_OK)
        return rc;
    return record(FLAG_STATE_SLOT_READY, last, BL_OK);
}

/* D: install from the slot into the app area, check the app, then record INSTALLED.
   img_crc / img_version only go into a REJECTED record. */
static int step_d(uint32_t img_crc, uint32_t img_version)
{
    const app_header_t *app = (const app_header_t *)APP_HEADER_ADDR;
    uint32_t t0 = HAL_GetTick();
    int rc = bl_ota_install_from_slot();                          // erases S2-S3, copies, writes the S1 header

    if (rc == BL_OK)
        rc = bootloader_is_app_valid();                           // read-back of the app area: 1..4 on failure

    if (rc == BL_OK)
    {
        bl_logf("D INSTALL OK v%lu %luB %lu ms\r\n", (unsigned long)app->version,
                (unsigned long)app->size, (unsigned long)(HAL_GetTick() - t0));
        /* S1 was erased with the new header, so this is the first record of a new log */
        if (flags_append(FLAG_STATE_INSTALLED, app->crc, app->version, BL_OK) != BL_OK)
            bl_print("INSTALLED not recorded\r\n");                // the new app is fine either way
        return BL_OK;
    }

    bl_logf("D INSTALL FAIL %d %s %lu ms\r\n", rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));
    if (app_area_untouched(rc))
        return reject(img_crc, img_version, rc);                   // old app still there: boot it
    bl_print("D app area erased: stop; the next reset retries D\r\n");
    return rc;                                                     // caller must stop
}

/* ---- Public ------------------------------------------------------------- */

int bl_update_resume(const boot_flag_rec_t *last)
{
    uint32_t state = last->state;
    int rc;

    if (state != FLAG_STATE_IMAGE_READY && state != FLAG_STATE_BACKUP_DONE && state != FLAG_STATE_SLOT_READY)
        return BL_OK;                                              // nothing to resume

    if (state == FLAG_STATE_IMAGE_READY)
    {
        rc = step_a(last);
        if (rc == BL_OK)
            rc = step_b(last);
        if (rc != BL_OK)
            return reject(last->img_crc, last->img_version, rc);
        state = FLAG_STATE_BACKUP_DONE;
    }

    if (state == FLAG_STATE_BACKUP_DONE)
    {
        rc = step_c(last);
        if (rc != BL_OK)
            return reject(last->img_crc, last->img_version, rc);
    }

    return step_d(last->img_crc, last->img_version);              // SLOT_READY
}

int bl_update_install(void)
{
    const ota_image_hdr_t *slot = (const ota_image_hdr_t *)OTA_START_ADDR;

    return step_d(slot->crc, slot->version);
}

/* ---- Boot decision--------------------------------------------- */

int bl_boot_decide(void)
{
    boot_flag_rec_t last;
    uint32_t n_valid, n_bad;
    int rc = flags_read_last(&last, NULL, &n_valid, &n_bad);
    int app;

    bl_logf("FLAGS %s, %lu valid, %lu bad\r\n", bl_state_text(last.state),
            (unsigned long)n_valid, (unsigned long)n_bad);

    /* 1. Resume an install in progress. With a full log no step could record its result,
          so only D may run (it erases S1 itself). */
    if (rc == BL_ERR_FLAG_FULL && last.state != FLAG_STATE_SLOT_READY)
        bl_print("FLAG LOG FULL: no install step can run until S1 is cleared\r\n");
    else
    {
        rc = bl_update_resume(&last);                      // does nothing unless IMAGE_READY, BACKUP_DONE or SLOT_READY
        if (rc != BL_OK)
            return rc;                                     // app area erased: stop; the next reset retries D
    }

    /* 2. Check the app once. Fix A fallback: app broken but a good image in the slot -> install it */
    app = bootloader_is_app_valid();
    if (app != BL_OK && bl_ota_check_slot() == BL_OK)
    {
        bl_logf("App invalid (%d %s), slot valid: reinstalling..\r\n", app, bl_err_text(app));
        rc = bl_update_install();
        if (rc != BL_OK)
            return rc;                                     // app area erased: stop
        app = bootloader_is_app_valid();                   // D may also have refused: check again
    }
    return app;                                            // 0 = start the app; 1..4 = app not valid
}
