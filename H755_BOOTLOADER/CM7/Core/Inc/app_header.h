/*
 * app_header.h
 *
 *  Created on: 20-Dec-2025
 *      Author: arunrawat
 */

#ifndef INC_APP_HEADER_H_
#define INC_APP_HEADER_H_

#include "flash_layout.h"

#define APP_MAGIC 	0xABCDEFAB

typedef struct
{
	uint32_t ota_flag;
    uint32_t magic;
    uint32_t size;      // application size in bytes
    uint32_t crc;       // CRC32 of application
    uint32_t version;
    uint32_t reserved[3];   // pads the header to 32 bytes = one H7 flash
} app_header_t;

typedef struct
{
    uint32_t magic;        // OTA image magic
    uint32_t image_size;   // Application binary size ONLY
    uint32_t crc;          // CRC32 of application binary ONLY
    uint32_t version;      // Firmware version
} ota_image_hdr_t;

#endif /* INC_APP_HEADER_H_ */
