/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bl_jump.h"
#include "bl_ota.h"
#include "string.h"
#include "flash_layout.h"
#include "app_header.h"
#include "flash_operations.h"
#include "bl_log.h"
#include <stdio.h>
#include "bl_update.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#define TEST_FLASH_BANK2   /* P3 bench test: uncomment, build, flash, read the log, comment out again */
//#define TEST_IMAGE_CHECK   /* P5 bench test: same use; writes only bank 2 S6 (backup) */
//#define TEST_BOOT_FLAGS    /* P6 bench test: same use; writes at most one record into S1 */
//#define TEST_INSTALL 1     /* P7 bench test: 1 = full install of the running app as version + 1 (rewrites the app area), 2 = wrong CRC in IMAGE_READY (step A must refuse), 3 = seed only: the boot decision installs it (P8) */
//#define PL_TEST_BOOT_DELAY_MS 3000   /* power-loss tests only: wait before the first log line so the terminal can reconnect. Remove before release */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
//#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart8;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART8_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


#ifdef TEST_FLASH_BANK2
/* . Only built when TEST_FLASH_BANK2 is defined. */
static void test_flash_bank2(void)
{
    static const uint8_t pattern[FLASH_WORD_BYTES] =
    {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xF0, 0x0F,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    char line[80];
    uint32_t t0;
    int err;


    /* 1. Guard decisions only: nothing is erased. Expect 0 0 0 1 1 */
    snprintf(line, sizeof(line), "T3 allow B1S0=%d B1S6=%d B1S7=%d BKP=%d RX=%d\r\n",
             flash_erase_allowed(FLASH_BANK_1, BL_SECTOR, 1),
             flash_erase_allowed(FLASH_BANK_1, FLASH_SECTOR_6, 1),
             flash_erase_allowed(FLASH_BANK_1, ARQ_CFG_SECTOR, 1),
             flash_erase_allowed(BKP_BANK, BKP_SECTOR, BKP_NB_SECTORS),
             flash_erase_allowed(RX_BANK, RX_SECTOR, RX_NB_SECTORS));
    bl_print(line);

    /* 2. Guard wired into the erase. Flash stays LOCKED here, so even a broken guard erases nothing. */
    err = flash_erase_sectors(FLASH_BANK_1, ARQ_CFG_SECTOR, 1);
    snprintf(line, sizeof(line), "T3 erase B1S7 locked: %d (expect -32)\r\n", err);
    bl_print(line);

    /* 3. Bank 2 sector 7: erase, write one flash word, read it back */
    HAL_FLASH_Unlock();
    t0  = HAL_GetTick();
    err = flash_erase_rx();
    snprintf(line, sizeof(line), "T3 erase B2S7: %d, %lu ms, hal 0x%08lX\r\n",
             err, (unsigned long)(HAL_GetTick() - t0), (unsigned long)flash_last_hal_error());
    bl_print(line);
    if (err == 0)
        err = flash_write_flashword(RX_START_ADDR, pattern);
    HAL_FLASH_Lock();

    bl_print((err == 0 && memcmp((const void *)RX_START_ADDR, pattern, sizeof(pattern)) == 0)
             ? "T3 write+read B2S7: OK\r\n" : "T3 write+read B2S7: FAIL\r\n");
}
#endif

#ifdef TEST_IMAGE_CHECK
/* . Only built when TEST_IMAGE_CHECK is defined.
   Writes only bank 2 S6 (backup). Never the app, the S1 header sector or the OTA slot. */
#include "bl_image.h"
#include "board_cfg.h"
#include "crc32.h"

#define T5_PAYLOAD  1024U
static uint8_t t5_img[IMG_HDR_SIZE + T5_PAYLOAD] __attribute__((aligned(32)));   // test image in RAM

/* Builds a fresh RAM image every call (1 KB payload), changes one thing, runs image_check on it. */
static int t5_ram_image(uint32_t magic, uint32_t size, uint32_t hw_rev, int flip_bit)
{
    ota_image_hdr_t *h = (ota_image_hdr_t *)t5_img;
    uint8_t *payload   = t5_img + IMG_HDR_SIZE;

    for (uint32_t i = 0; i < T5_PAYLOAD; i++)
        payload[i] = (uint8_t)(i * 7U + 3U);
    h->magic       = magic;
    h->version     = 1;
    h->image_size  = size;
    h->crc         = crc32(payload, T5_PAYLOAD);
    h->hw_rev      = hw_rev;
    h->reserved[0] = h->reserved[1] = h->reserved[2] = 0xFFFFFFFF;
    if (flip_bit)
        payload[100] ^= 0x01;                    // one bit flipped after the CRC was taken
    return image_check((uint32_t)t5_img);
}

static void test_image_check(void)
{
    const uint32_t rx_payload = RX_START_ADDR + IMG_HDR_SIZE;
    ota_image_hdr_t h;
    uint32_t t0, t_off, t_on, crc_off, crc_on;
    int rc, r_crc, r_overlap, r_size, r_still;

    /* 1. image_check on RAM images, no flash access. Expect 0 -10 -11 -11 -12 -13 */
    bl_logf("T5 check good=%d magic=%d size0=%d big=%d hwrev=%d crc=%d\r\n",
            t5_ram_image(APP_MAGIC, T5_PAYLOAD, BOARD_HW_REV, 0),
            t5_ram_image(0x12345678U, T5_PAYLOAD, BOARD_HW_REV, 0),
            t5_ram_image(APP_MAGIC, 0, BOARD_HW_REV, 0),
            t5_ram_image(APP_MAGIC, IMG_MAX_PAYLOAD + 1U, BOARD_HW_REV, 0),
            t5_ram_image(APP_MAGIC, T5_PAYLOAD, BOARD_HW_REV + 1U, 0),
            t5_ram_image(APP_MAGIC, T5_PAYLOAD, BOARD_HW_REV, 1));

    /* 2. Time one full-size CRC pass: 131,040 bytes of bank 2 S7, whatever they hold */
    t0      = HAL_GetTick();
    crc_off = crc32((const uint8_t *)rx_payload, IMG_MAX_PAYLOAD);
    t_off   = HAL_GetTick() - t0;
    SCB_EnableICache();
    t0      = HAL_GetTick();
    crc_on  = crc32((const uint8_t *)rx_payload, IMG_MAX_PAYLOAD);
    t_on    = HAL_GetTick() - t0;
    SCB_DisableICache();                         // back to how the bootloader runs today
    bl_logf("T5 crc 128K: %lu ms I-cache off, %lu ms on, same=%d\r\n",
            (unsigned long)t_off, (unsigned long)t_on, crc_off == crc_on);

    /* 3. Worst-case copy: those 131,040 bytes into the backup (erase + write + read-back). Expect 0 */
    h.magic       = APP_MAGIC;
    h.version     = 0;
    h.image_size  = IMG_MAX_PAYLOAD;
    h.crc         = crc_off;
    h.hw_rev      = BOARD_HW_REV;
    h.reserved[0] = h.reserved[1] = h.reserved[2] = 0xFFFFFFFF;
    t0 = HAL_GetTick();
    rc = copy_image(&h, rx_payload, &region_backup, crc_off);
    bl_logf("T5 copy 128K S7->BKP: %d %s, %lu ms, hal 0x%08lX\r\n", rc, bl_err_text(rc),
            (unsigned long)(HAL_GetTick() - t0), (unsigned long)flash_last_hal_error());

    /* 4. Refusals: each returns before the erase, so the backup from step 3 stays good.
          Expect -14 -33 -11, then 0 */
    r_crc     = copy_image(&h, rx_payload, &region_backup, crc_off ^ 1U);              // wrong expected CRC
    r_overlap = copy_image(&h, BKP_START_ADDR + IMG_HDR_SIZE, &region_backup, crc_off); // source inside dst
    h.image_size = 0;
    r_size    = copy_image(&h, rx_payload, &region_backup, crc_off);                    // size 0
    r_still   = image_check(BKP_START_ADDR);
    bl_logf("T5 refuse crc=%d overlap=%d size0=%d, backup still=%d\r\n", r_crc, r_overlap, r_size, r_still);

    /* 5. The real backup step (Part 7 step B): running app -> backup. Expect 0 */
    if (bootloader_is_app_valid() == 0)
    {
        make_backup_header(&h);
        t0 = HAL_GetTick();
        rc = copy_image(&h, APP_START_ADDR, &region_backup, h.crc);
        bl_logf("T5 backup app v%lu, %lu bytes: %d %s, %lu ms\r\n", (unsigned long)h.version,
                (unsigned long)h.image_size, rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));
    }
    else
        bl_print("T5 backup: no valid app, skipped\r\n");

    /* 6. Leave the backup empty. Expect 0, then -10 (erased magic) */
    HAL_FLASH_Unlock();
    rc = flash_erase_backup();
    HAL_FLASH_Lock();
    r_still = image_check(BKP_START_ADDR);
    bl_logf("T5 erase BKP: %d, backup now=%d\r\n", rc, r_still);
}
#endif

#ifdef TEST_BOOT_FLAGS
/* . Only built when TEST_BOOT_FLAGS is defined.
   Writes at most one record into S1, and only when the log has no valid record. Never erases. */
#include "boot_flags.h"

static void t6_print_log(void)
{
    boot_flag_rec_t last;
    uint32_t next_free, count, bad;
    int rc = flags_read_last(&last, &next_free, &count, &bad);

    bl_logf("T6 log: %d, %s, %lu valid, %lu bad, next 0x%08lX\r\n", rc, bl_state_text(last.state),
            (unsigned long)count, (unsigned long)bad, (unsigned long)next_free);
    bl_logf("T6 last: crc 0x%08lX, v%lu, detail %ld\r\n", (unsigned long)last.img_crc,
            (unsigned long)last.img_version, (long)last.detail);
}

static void test_boot_flags(void)
{
    uint32_t count, t0;
    int rc;

    /* 1. The log as it is now. After a reset or a power-off this must show the record from the first run */
    t6_print_log();

    /* 2. Only on a log with no valid record: add IMAGE_READY with test values, then read the log again */
    flags_read_last(NULL, NULL, &count, NULL);
    if (count == 0)
    {
        t0 = HAL_GetTick();
        rc = flags_append(FLAG_STATE_IMAGE_READY, 0x12345678U, 2U, 0);
        bl_logf("T6 append IMAGE_READY: %d %s, %lu ms\r\n", rc, bl_err_text(rc), (unsigned long)(HAL_GetTick() - t0));
        t6_print_log();
    }

    /* 3. An invalid state is refused before anything is written. Expect -21 */
    rc = flags_append(9U, 0, 0, 0);
    bl_logf("T6 append state 9: %d %s\r\n", rc, bl_err_text(rc));
}
#endif

#ifdef TEST_INSTALL
/* Only built when TEST_INSTALL is defined (1 or 2).
   On an empty flag log with a valid app: S7 = copy of the running app labelled version + 1, then IMAGE_READY.
   Then it runs the install sequence, as Part 8 will in the real boot.
   Mode 1 rewrites the app area (same code, new version number). Mode 2 puts a wrong CRC into
   IMAGE_READY: step A must refuse it, and the app, slot and backup must stay as they were. */
#include "boot_flags.h"
#include "bl_image.h"
#include "bl_update.h"
#include "bl_errors.h"

static const flash_region_t t7_region_rx = { RX_START_ADDR, RX_BANK, RX_SECTOR, RX_NB_SECTORS };

static void test_install(void)
{
    boot_flag_rec_t last;
    uint32_t count;
    ota_image_hdr_t h;
    int rc;

    flags_read_last(&last, NULL, &count, NULL);
    bl_logf("T7 flags: %s, %lu valid\r\n", bl_state_text(last.state), (unsigned long)count);

    /* 1. Seed, only on an empty log with a valid running app */
    if (count == 0 && bootloader_is_app_valid() == 0)
    {
        make_backup_header(&h);
        h.version += 1U;                                           // same code, next version number
        rc = copy_image(&h, APP_START_ADDR, &t7_region_rx, h.crc);
        bl_logf("T7 seed S7: v%lu %luB: %d %s\r\n", (unsigned long)h.version,
                (unsigned long)h.image_size, rc, bl_err_text(rc));
        if (rc == BL_OK)
        {
			#if TEST_INSTALL == 2
            rc = flags_append(FLAG_STATE_IMAGE_READY, h.crc ^ 1U, h.version, 0);   // wrong CRC on purpose

            #else
            rc = flags_append(FLAG_STATE_IMAGE_READY, h.crc, h.version, 0);

            #endif
            bl_logf("T7 seed IMAGE_READY: %d %s\r\n", rc, bl_err_text(rc));
        }
        flags_read_last(&last, NULL, &count, NULL);
    }

	#if TEST_INSTALL != 3
    /* 2. Resume from the last record, as Part 8 will do in every boot */
    rc = bl_update_resume(&last);
    flags_read_last(&last, NULL, &count, NULL);
    bl_logf("T7 resume: %d %s, flags now %s, %lu valid\r\n", rc, bl_err_text(rc),
            bl_state_text(last.state), (unsigned long)count);
	#endif

}
#endif

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_UART8_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100);
  #ifdef PL_TEST_BOOT_DELAY_MS
    HAL_Delay(PL_TEST_BOOT_DELAY_MS);   /* test only: time to reconnect the terminal */
  #endif
      // W25Q_Reset();                                  // F411: remove

    /* H755 */

    bl_print("Inside Bootloader!!\r\n");

    /* TESTS */
    #ifdef TEST_FLASH_BANK2
  	  test_flash_bank2();
    #endif

    #ifdef TEST_IMAGE_CHECK
  	  test_image_check();
    #endif

    #ifdef TEST_BOOT_FLAGS
  	  test_boot_flags();
    #endif

    #ifdef TEST_INSTALL
  	  test_install();
    #endif


    /* Boot decision: flags, resume, Fix A fallback and app check, all in bl_update.c */
    int err = bl_boot_decide();
    if (err != 0)
    {
        bl_logf("Failed to Jump!! %d %s\r\n", err, bl_err_text(err));
        while (1)
        {
        }
    }

    bl_logf("JUMP v%lu, %lu ms after reset\r\n",
            (unsigned long)((const app_header_t *)APP_HEADER_ADDR)->version, (unsigned long)HAL_GetTick());
    JumpToApplication();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART8_Init(void)
{

  /* USER CODE BEGIN UART8_Init 0 */

  /* USER CODE END UART8_Init 0 */

  /* USER CODE BEGIN UART8_Init 1 */

  /* USER CODE END UART8_Init 1 */
  huart8.Instance = UART8;
  huart8.Init.BaudRate = 9600;
  huart8.Init.WordLength = UART_WORDLENGTH_8B;
  huart8.Init.StopBits = UART_STOPBITS_1;
  huart8.Init.Parity = UART_PARITY_NONE;
  huart8.Init.Mode = UART_MODE_TX_RX;
  huart8.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart8.Init.OverSampling = UART_OVERSAMPLING_16;
  huart8.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart8.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart8.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
  huart8.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
  if (HAL_UART_Init(&huart8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart8, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart8, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart8) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART8_Init 2 */

  /* USER CODE END UART8_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(M7_STAT_GPIO_Port, M7_STAT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : M7_STAT_Pin */
  GPIO_InitStruct.Pin = M7_STAT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(M7_STAT_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
