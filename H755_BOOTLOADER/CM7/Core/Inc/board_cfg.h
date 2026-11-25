/*
 * board_cfg.h
 *
 *  2026-11-25: board identity used by the image check.
 *  Shared with APP: keep both copies identical.
 */

#ifndef INC_BOARD_CFG_H_
#define INC_BOARD_CFG_H_

/* Hardware revision this firmware is built for.
 * TBD: default until the real source is decided (bootloader constant,
 * ArQ configuration sector, or strap pins).
 * Every image packaged for this board must carry the same value
 * in its header hw_rev field (make_s7_image.py default = 1). */

#define BOARD_HW_REV        0x00000001U

#endif /* INC_BOARD_CFG_H_ */
