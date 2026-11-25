/*
 * bl_log.h
 *
 *  2026-11-25: bootloader logging (bl_print moved here from main.c),
 *  formatted log lines, and the text for every result code in bl_errors.h.
 */

#ifndef INC_BL_LOG_H_
#define INC_BL_LOG_H_

#include <stdint.h>

/* Send a text to UART8 and USART3 (USART3 adds the "^^" end mark). Same as the old main.c version. */
void bl_print(const char *msg);

/* printf-style log line, at most 127 characters (longer text is cut). Add "\r\n" yourself.
   Use %d for int, %lu or %08lX for uint32_t, %s for text. No %f (newlib-nano).
   The format attribute makes the compiler check every call's arguments. */
void bl_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Short name for a result code from bl_errors.h, e.g. -12 -> "IMG HW REV ERROR". */
const char *bl_err_text(int code);

/* Short name for a boot flag state from boot_flags.h, e.g. 0x01 -> "IMAGE_READY". */
const char *bl_state_text(uint32_t state);

#endif /* INC_BL_LOG_H_ */

