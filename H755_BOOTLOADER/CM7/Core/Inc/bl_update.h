/*
 * bl_update.h
 *
 * install sequence. Steps A (check S7), B (back up the running app),
 * C (copy S7 into the slot), D (install from the slot), driven by the boot flag records.
 */

#ifndef INC_BL_UPDATE_H_
#define INC_BL_UPDATE_H_


#include <stdint.h>
#include "boot_flags.h"

/* Runs the install from the step that matches last->state, to the end, in this boot:
   IMAGE_READY -> A B C D,  BACKUP_DONE -> C D,  SLOT_READY -> D,  any other state -> nothing.
   Returns 0 when the boot can go on: new app installed, update rejected, or nothing to do.
   Returns non-zero when the app area was erased and could not be rebuilt in this boot:
   the caller must stop. The value is the reason (bl_errors.h). The next reset retries step D. */
int bl_update_resume(const boot_flag_rec_t *last);

/* Step D on its own, from whatever image is in the slot (Part 8 uses it for the Fix A fallback).
   Same return rule as bl_update_resume(). */
int bl_update_install(void);

/* The whole boot decision: read the flags, resume an install in progress,
   Fix A fallback (app invalid, slot valid -> install from the slot), app check.
   Returns 0 when the app is ready to start; otherwise the reason it is not (the caller stops):
   1..4 from bootloader_is_app_valid(), or a negative code from step D. */
int bl_boot_decide(void);

#endif /* INC_BL_UPDATE_H_ */
