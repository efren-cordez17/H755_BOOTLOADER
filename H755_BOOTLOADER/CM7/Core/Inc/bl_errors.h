/*
 * bl_errors.h
 *
 *  Created on: Nov 25, 2026
 *      Author: jefje
 *
 *  Every bootloader result code in one place.
 *  Existing codes keep their numbers. New codes run from -8 down.
 *  Log text for each code comes in Part 4 (bl_err_text).
 */

#ifndef INC_BL_ERRORS_H_
#define INC_BL_ERRORS_H_


#define BL_OK                    0

/* bootloader_is_app_valid() (existing, positive codes) */
#define BL_APP_ERR_MAGIC         1      // S1 header magic wrong
#define BL_APP_ERR_RESET         2      // reset handler outside the app area
#define BL_APP_ERR_SIZE          3      // header size 0 or over IMG_MAX_PAYLOAD
#define BL_APP_ERR_CRC           4      // app CRC differs from the header

/* bl_ota_run() (existing) */
#define BL_ERR_SLOT_HDR_READ    (-1)    // reading the slot header failed      (app not erased)
#define BL_ERR_SLOT_MAGIC       (-2)    // slot header magic wrong              (app not erased)
#define BL_ERR_SLOT_SIZE        (-3)    // slot header size wrong               (app not erased)
#define BL_ERR_SLOT_READ        (-4)    // reading the payload failed           (app erased)
#define BL_ERR_APP_WRITE        (-5)    // writing the app failed               (app erased)
#define BL_ERR_COPY_CHECK       (-6)    // CRC of the copied bytes differs      (app erased)
/* -7 retired: old slot CRC code, now BL_ERR_IMG_CRC */

/* New */
#define BL_ERR_APP_ERASE        (-8)    // erasing S2-S3 failed                 (app partly erased)
#define BL_ERR_HDR_WRITE        (-9)    // S1 erase or header write failed      (app erased)
#define BL_ERR_IMG_MAGIC        (-10)   // image_check: magic wrong
#define BL_ERR_IMG_SIZE         (-11)   // image_check: size 0 or over IMG_MAX_PAYLOAD
#define BL_ERR_IMG_HWREV        (-12)   // image_check: hw_rev differs from BOARD_HW_REV
#define BL_ERR_IMG_CRC          (-13)   // image_check: payload CRC differs
#define BL_ERR_CRC_MISMATCH     (-14)   // image CRC differs from the one in the flag record
#define BL_ERR_IMG_VECTOR       (-15)   // step A: the image reset handler is outside the app area
#define BL_ERR_FLAG_FULL        (-20)   // no blank word left in the flag area
#define BL_ERR_FLAG_WRITE       (-21)   // programming a flag record failed
#define BL_ERR_FLAG_READBACK    (-22)   // flag record read back differently
#define BL_ERR_ERASE            (-30)   // HAL erase failed (see flash_last_hal_error())
#define BL_ERR_COPY_WRITE       (-31)   // writing the backup or the slot failed
#define BL_ERR_ERASE_REFUSED    (-32)   // erase guard: protected or invalid sector
#define BL_ERR_COPY_OVERLAP     (-33)   // copy_image: the source lies inside the destination (nothing erased)
#define BL_WARN_NO_BACKUP       (-40)   // warning only: no valid running app, backup skipped

#endif /* INC_BL_ERRORS_H_ */
