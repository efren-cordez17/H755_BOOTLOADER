/*
 * bl_ota.h
 *
 *  Created on: 03-Jan-2026
 *      Author: arunrawat
 */

#ifndef INC_BL_OTA_H_
#define INC_BL_OTA_H_

#include "app_header.h"


typedef struct
{
    uint32_t write_addr;       /* Current flash write address */
    uint32_t total_received;   /* Bytes written so far */
    uint32_t image_size;       /* Expected firmware size */
    uint32_t expected_crc;     /* Expected final CRC */
    uint32_t running_crc;      /* CRC accumulator */
} bl_ota_ctx_t;




typedef struct
{
    int  (*read)(uint8_t *buf, uint32_t len);
    void (*set_total_size)(uint32_t total_size);
} ota_stream_t;

int bl_ota_run(bl_ota_ctx_t *ctx, ota_stream_t *stream);
int bl_ota_install_from_slot(void);   /* resets the slot reader, then bl_ota_run(); same return codes */
int bl_ota_check_slot(void);   /* 0 = good image in the slot; -10 magic, -11 size, -12 hw_rev, -13 CRC */
int bl_flash_write_app_header(uint32_t magic, uint32_t size, uint32_t crc, uint32_t version);

#endif /* INC_BL_OTA_H_ */
