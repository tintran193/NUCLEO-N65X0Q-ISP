/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx.c
  * @author  MCD Application Team
  * @brief   USBX applicative file
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
#include "app_usbx.h"
#include <stdio.h>

/**
  * @brief  Application USBX Initialization.
  * @param  memory_ptr: memory pointer
  * @retval status
  *
  * NOTE: Called from usbx_init_thread at priority 2 — higher than USBX internal
  * threads (UX_THREAD_PRIORITY_CLASS = P4) and the camera thread (P5).
  * No other thread can preempt this thread, so there is no UART contention and
  * no risk of internal USBX threads starting before initialisation completes.
  * tx_thread_sleep() calls are therefore NOT used here.
  */
UINT MX_USBX_Init(VOID *memory_ptr)
{
  UINT ret = UX_SUCCESS;
  UINT ux_si_ret;

  UCHAR *pointer;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_USBX_Init0 */
  printf("[USBX] MX_USBX_Init starting...\r\n");
  /* USER CODE END MX_USBX_Init0 */

  /* Allocate the stack for USBX Memory */
  printf("[USBX] Allocating USBX memory stack (%u bytes)...\r\n", USBX_MEMORY_STACK_SIZE);
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
                       USBX_MEMORY_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_ALLOCATE_STACK_ERROR */
    printf("[ERROR] USBX memory allocation failed\r\n");
    return TX_POOL_ERROR;
    /* USER CODE END USBX_ALLOCATE_STACK_ERROR */
  }

  /* Initialize USBX Memory */
  printf("[USBX] Initializing USBX system (ptr=0x%08lX sz=%u)...\r\n",
         (unsigned long)pointer, USBX_MEMORY_STACK_SIZE);

  ux_si_ret = ux_system_initialize(pointer, USBX_MEMORY_STACK_SIZE, UX_NULL, 0);

  printf("[USBX] ux_system_initialize returned %u\r\n", (unsigned)ux_si_ret);

  if (ux_si_ret != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_SYSTEM_INITIALIZE_ERROR */
    printf("[ERROR] ux_system_initialize failed\r\n");
    return UX_ERROR;
    /* USER CODE END USBX_SYSTEM_INITIALIZE_ERROR */
  }

  printf("[USBX] Calling MX_USBX_Device_Init...\r\n");

  ret = MX_USBX_Device_Init(byte_pool);

  printf("[USBX] MX_USBX_Device_Init returned %u\r\n", (unsigned)ret);

  if(ret != UX_SUCCESS)
  {
  /* USER CODE BEGIN MX_USBX_Device_Init_Error */
    printf("[ERROR] MX_USBX_Device_Init failed status=%u\r\n", (unsigned)ret);
    tx_thread_suspend(tx_thread_identify());
  /* USER CODE END MX_USBX_Device_Init_Error */
  }

  printf("[USBX] MX_USBX_Init completed successfully\r\n");

  /* USER CODE BEGIN MX_USBX_Init1 */

  /* USER CODE END MX_USBX_Init1 */

  return ret;
}
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
