/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32n6xx_it.h"
#include <stdio.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
extern volatile uint32_t csi_irq_count;
extern volatile uint32_t dcmipp_irq_count;
extern volatile uint32_t csi_sr0;
extern volatile uint32_t csi_sr1;

extern volatile uint32_t csi_ier0;
extern volatile uint32_t csi_ier1;

extern volatile uint32_t sot_lane0_count;
extern volatile uint32_t sot_lane1_count;
extern TIM_HandleTypeDef htim6;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DCMIPP_HandleTypeDef hdcmipp;
extern PCD_HandleTypeDef hpcd_USB_OTG_HS1;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */

  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/*
 * PendSV_Handler and SysTick_Handler are intentionally NOT defined here.
 * ThreadX's tx_initialize_low_level.S (Core/Src/) provides both once
 * tx_kernel_enter() runs -- PendSV for context switching, SysTick for the
 * RTOS tick (HAL's own tick is moved to TIM6 instead, see
 * stm32n6xx_hal_timebase_tim.c / HAL_TIM_PeriodElapsedCallback in main.c).
 * Defining them here too would be a duplicate-symbol link error against
 * ThreadX's port, exactly like Camera_N6_AI_Test's own stm32n6xx_it.c
 * (which also omits both for the same reason).
 */

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DCMIPP global interrupt.
  */
void DCMIPP_IRQHandler(void)
{
  /* USER CODE BEGIN DCMIPP_IRQn 0 */
    dcmipp_irq_count++;
  /* USER CODE END DCMIPP_IRQn 0 */
  HAL_DCMIPP_IRQHandler(&hdcmipp);
  /* USER CODE BEGIN DCMIPP_IRQn 1 */

  /* USER CODE END DCMIPP_IRQn 1 */
}

/**
  * @brief This function handles CSI global interrupt.
  */
void CSI_IRQHandler(void)
{
  /* USER CODE BEGIN CSI_IRQn 0 */
    uint32_t sr1;

    csi_irq_count++;

    sr1 = CSI->SR1;

    if (sr1 & (1UL << 1))
    {
        sot_lane0_count++;
    }

    if (sr1 & (1UL << 9))
    {
        sot_lane1_count++;
    }
  /* USER CODE END CSI_IRQn 0 */
  HAL_DCMIPP_CSI_IRQHandler(&hdcmipp);
  /* USER CODE BEGIN CSI_IRQn 1 */

  /* USER CODE END CSI_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt (HAL time base once
  *        ThreadX has claimed SysTick -- see stm32n6xx_hal_timebase_tim.c).
  */
void TIM6_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/**
  * @brief This function handles USB1 OTG HS global interrupt.
  */
/* Diagnostic only (see WORKLOG.md): counts how many times this ISR actually
 * fires, to tell "USB interrupt never fires at all" (NVIC/PHY/clock-level
 * problem) apart from "interrupt fires but USBX/PCD never responds"
 * (protocol/driver-level problem) -- printed periodically from
 * app_threadx.c's capture thread. Confirmed non-zero but stops climbing
 * after a handful of hits (matches the host giving up retrying after a few
 * failed enumeration attempts, per dmesg) -- so the interrupt DOES reach
 * the CPU, but something in what it's actually reporting (or in USBX's
 * handling of it) never lets a SETUP packet get answered. Printing the raw
 * GINTSTS bits below (before HAL_PCD_IRQHandler clears them) shows exactly
 * which hardware event each of those handful of interrupts actually was --
 * e.g. USBRST (bus reset) and ENUMDNE (enumeration/speed done) firing but
 * RXFLVL/OEPINT (an actual SETUP packet arriving on EP0) never doing so
 * would mean the SETUP packet itself is never reaching the core/FIFO,
 * which is a very different bug from USBX mishandling a SETUP it did get. */
volatile uint32_t usb_irq_count = 0U;

void USB1_OTG_HS_IRQHandler(void)
{
  usb_irq_count++;
  /* Per-IRQ GINTSTS dump (used to diagnose Bugs 11/12, see WORKLOG.md)
   * removed now that USB enumerates/streams reliably -- it fires on every
   * SOF and was drowning out the rest of the log. Re-add temporarily if
   * USB-level diagnostics are ever needed again; usb_irq_count alone is
   * cheap enough to leave in permanently. */

  /*
   * Bug 11 (see WORKLOG.md): the previous diagnostic (this same ISR,
   * printing raw GINTSTS) showed OEPINT firing on real hardware -- a SETUP
   * packet does physically arrive -- but IEPINT (the device actually
   * transmitting a response) never once fires afterward. This matches the
   * PC-side symptom exactly: GET_DESCRIPTOR times out with no response at
   * all, not even a NAK-forever.
   *
   * hpcd_USB_OTG_HS1.Setup[12] (stm32n6xx_hal_pcd.h) is where the OTG
   * core's own internal DMA (dma_enable=ENABLE in
   * MX_USB1_OTG_HS_PCD_Init()) writes the incoming SETUP packet bytes --
   * it's an ordinary cacheable global struct field, and NEITHER
   * stm32n6xx_hal_pcd.c NOR USBX's ux_dcd_stm32_callback.c ever calls
   * SCB_InvalidateDCache_by_Addr() on it before HAL_PCD_IRQHandler() reads
   * it and hands it to USBX's request parser. Since this project enables
   * D-Cache (SCB_EnableDCache() in main()), the CPU can read back stale
   * cached SETUP bytes instead of what the DMA actually just wrote --
   * exactly the same class of bug this project's OWN camera pipeline
   * already needed explicit cache invalidation for (camera_framebuffer /
   * video_buf, see knowledge_archive.md). USBX silently failing to
   * recognize a garbage/stale SETUP request (instead of erroring loudly)
   * is consistent with OEPINT firing but no IN response ever following.
   *
   * Fix: invalidate the Setup buffer's cache lines ourselves, right before
   * HAL_PCD_IRQHandler() processes them -- cheapest possible place to do
   * it without patching vendored HAL/USBX source.
   */
  SCB_InvalidateDCache_by_Addr((uint32_t *)hpcd_USB_OTG_HS1.Setup,
                                (int32_t)sizeof(hpcd_USB_OTG_HS1.Setup));

  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS1);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
