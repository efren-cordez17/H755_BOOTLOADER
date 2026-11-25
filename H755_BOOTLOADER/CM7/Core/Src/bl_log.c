
/*
 * bl_log.c
 *
 *  2026-11-25: bl_print moved here from main.c; bl_logf and bl_err_text added.
 */

#include "main.h"
#include "bl_log.h"
#include "bl_errors.h"
#include "boot_flags.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart8;      // defined in main.c
extern UART_HandleTypeDef huart3;

#define BL_LOG_LINE_MAX   128          // 127 characters + '\0'; about 132 ms on UART8 at 9600 baud

void bl_print(const char *msg)
{
    uint16_t len = (uint16_t)strlen(msg);

    HAL_UART_Transmit(&huart8, (uint8_t *)msg, len, 200);
    HAL_UART_Transmit(&huart3, (uint8_t *)msg, len, 200);
    HAL_UART_Transmit(&huart3, (uint8_t *)"^^", 2, 50);
}

void bl_logf(const char *fmt, ...)
{
    char line[BL_LOG_LINE_MAX];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);   // always ends with '\0'; cuts text that is too long
    va_end(args);

    bl_print(line);
}

const char *bl_err_text(int code)
{
    switch (code)
    {
    case BL_OK:                 return "OK";

    case BL_APP_ERR_MAGIC:      return "MAGIC ERROR";
    case BL_APP_ERR_RESET:      return "RESET ERROR";
    case BL_APP_ERR_SIZE:       return "SIZE ERROR";
    case BL_APP_ERR_CRC:        return "CRC ERROR";

    case BL_ERR_SLOT_HDR_READ:  return "SLOT READ ERROR";
    case BL_ERR_SLOT_MAGIC:     return "SLOT MAGIC ERROR";
    case BL_ERR_SLOT_SIZE:      return "SLOT SIZE ERROR";
    case BL_ERR_SLOT_READ:      return "SLOT READ ERROR";
    case BL_ERR_APP_WRITE:      return "FLASH WRITE ERROR";
    case BL_ERR_COPY_CHECK:     return "COPY CHECK ERROR";
    case BL_ERR_COPY_OVERLAP:   return "COPY OVERLAP";

    case BL_ERR_APP_ERASE:      return "APP ERASE ERROR";
    case BL_ERR_HDR_WRITE:      return "HEADER WRITE ERROR";
    case BL_ERR_IMG_MAGIC:      return "IMG MAGIC ERROR";
    case BL_ERR_IMG_SIZE:       return "IMG SIZE ERROR";
    case BL_ERR_IMG_HWREV:      return "IMG HW REV ERROR";
    case BL_ERR_IMG_CRC:        return "IMG CRC ERROR";
    case BL_ERR_CRC_MISMATCH:   return "CRC MISMATCH";
    case BL_ERR_IMG_VECTOR:     return "IMG VECTOR ERROR";
    case BL_ERR_FLAG_FULL:      return "FLAG AREA FULL";
    case BL_ERR_FLAG_WRITE:     return "FLAG WRITE ERROR";
    case BL_ERR_FLAG_READBACK:  return "FLAG READBACK ERROR";
    case BL_ERR_ERASE:          return "ERASE ERROR";
    case BL_ERR_COPY_WRITE:     return "COPY WRITE ERROR";
    case BL_ERR_ERASE_REFUSED:  return "ERASE REFUSED";
    case BL_WARN_NO_BACKUP:     return "NO VALID APP, NO BACKUP";

    default:                    return "UNKNOWN CODE";
    }
}

const char *bl_state_text(uint32_t state)
{
    switch (state)
    {
    case FLAG_STATE_IDLE:         return "IDLE";
    case FLAG_STATE_IMAGE_READY:  return "IMAGE_READY";
    case FLAG_STATE_BACKUP_DONE:  return "BACKUP_DONE";
    case FLAG_STATE_SLOT_READY:   return "SLOT_READY";
    case FLAG_STATE_INSTALLED:    return "INSTALLED";
    case FLAG_STATE_TRIAL:        return "TRIAL";
    case FLAG_STATE_CONFIRMED:    return "CONFIRMED";
    case FLAG_STATE_ROLLED_BACK:  return "ROLLED_BACK";
    case FLAG_STATE_REJECTED:     return "REJECTED";
    default:                      return "UNKNOWN STATE";
    }
}
