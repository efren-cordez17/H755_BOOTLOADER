/*
 * bl_jump.c
 *
 *  Created on: 16-Dec-2025
 *      Author: arunrawat
 */


#include "main.h"
#include "bl_jump.h"
#include "flash_layout.h"
#include "app_header.h"
#include "crc32.h"

typedef void (*pFunction)(void);
extern UART_HandleTypeDef huart8;
extern UART_HandleTypeDef huart3;

void JumpToApplication(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    pFunction appEntry;

    /* Read application stack pointer */
    appStack = *(volatile uint32_t*)APP_START_ADDR;


    /* Read reset handler address */
    appResetHandler = *(volatile uint32_t*)(APP_START_ADDR + 4);
    appEntry = (pFunction)appResetHandler;

    /* 1. Give back every peripheral the bootloader used */
        HAL_UART_DeInit(&huart8);
        HAL_UART_DeInit(&huart3);
        HAL_RCC_DeInit();
        HAL_DeInit();

	/* 2. Disable interrupts */
        __disable_irq();

	/* 3. Stop SysTick */
		SysTick->CTRL = 0;
		SysTick->LOAD = 0;
		SysTick->VAL  = 0;

	/* 4. Switch off and clear every interrupt line */
		for (uint32_t i = 0; i < 8; i++)
		{
			NVIC->ICER[i] = 0xFFFFFFFF;
			NVIC->ICPR[i] = 0xFFFFFFFF;
		}

	/* 5. Caches off, MPU off */
		SCB_DisableDCache();
		SCB_DisableICache();
		HAL_MPU_Disable();

	/* 6. Point the vector table at the application */
		SCB->VTOR = APP_START_ADDR;

	/* 7. Set main stack pointer and jump */
		__set_MSP(appStack);
		appEntry();
}

int bootloader_is_app_valid(void)
{
	uint32_t HDR_ADDR = APP_HEADER_ADDR;
    const app_header_t *app_hdr = (const app_header_t *)HDR_ADDR;

    /* 1. Magic */
    if (app_hdr->magic != APP_MAGIC)
        return 1;

    /* 2. Reset handler sanity */
    uint32_t reset_handler = *(uint32_t *)(APP_START_ADDR + 4);
     if (reset_handler < APP_START_ADDR ||
         reset_handler >= (APP_START_ADDR + APP_MAX_SIZE))
         return 2;

    /* 3. Size sanity */
    if (app_hdr->size == 0 || app_hdr->size > APP_MAX_SIZE)
        return 3;

    /* 4. CRC check */
    uint32_t calc_crc =
        crc32((const uint8_t *)APP_START_ADDR, app_hdr->size);

    if (calc_crc != app_hdr->crc)
        return 4;

    return 0;   // VALID
}
