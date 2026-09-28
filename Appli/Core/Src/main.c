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
#include <stdio.h>
#include "imx219.h"
#include "imx219_port.h"
#include <string.h>
#include "isp_api.h"
#include "isp_core.h"
#include "isp_param_conf_imx219.h"
#include "app_threadx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define FRAME_WIDTH       640U
#define FRAME_HEIGHT      480U
#define FRAME_BPP         2U

#define FRAME_BUFFER_SIZE  (FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP)
//#define FRAME_BUFFER_SIZE (FRAME_WIDTH * FRAME_HEIGHT * 5 / 4)
#define CAMERA_BUFFER_ADDR 0x34200000U
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DCMIPP_HandleTypeDef hdcmipp;

I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef hlpuart1;

/* USER CODE BEGIN PV */

ISP_HandleTypeDef  hcamera_isp;

IMX219_CTX_t imx219_ctx;

/* Bug 21 (see WORKLOG.md): these must start matching what IMX219_Configure640x480()
 * actually writes to the sensor at init (gain=0x00, exposure=0x0640=1600) --
 * GetSensorGainHelper()/GetSensorExposureHelper() report these back to
 * evision's AEC as "the sensor's current state", and if they start out
 * wrong (e.g. exposure=0 while the sensor is really sitting at 1600), AEC's
 * very first correction is computed against a lie, which is consistent
 * with the "flashes bright then crushes to near-black" symptom seen right
 * after AECAlgo was first enabled -- the sensor's real starting exposure
 * (1600, genuinely bright) got misread as 0 (minimum), so AEC's first
 * control step swung drastically in the wrong direction. */
/* Not static: app_threadx.c's UVC loop prints these to see what AEC has
 * actually converged the sensor to (see Bug 21's "still dark" follow-up in
 * WORKLOG.md) -- FPS halving (31->16) after enabling AEC strongly suggests
 * exposure is being driven at/near the frame-length ceiling; confirm with
 * real numbers instead of guessing further. */
int32_t isp_gain = 0;
int32_t isp_exposure = 1600;

//__attribute__((aligned(32)))
//static uint8_t camera_framebuffer[FRAME_BUFFER_SIZE];
__attribute__((aligned(32)))
//uint8_t camera_framebuffer[FRAME_BUFFER_SIZE];

uint8_t *camera_framebuffer =
(uint8_t *)CAMERA_BUFFER_ADDR;


volatile uint32_t csi_irq_count = 0;
volatile uint32_t dcmipp_irq_count = 0;
volatile uint32_t frame_count = 0;
volatile uint32_t frame_received = 0;
volatile uint32_t csi_sr0 = 0;
volatile uint32_t csi_sr1 = 0;
volatile uint32_t csi_ier0 = 0;
volatile uint32_t csi_ier1 = 0;
volatile uint32_t sot_lane0_count = 0;
volatile uint32_t sot_lane1_count = 0;

/* USB OTG HS PCD handle -- used by app_usbx_device.c's device thread
 * (MX_USB1_OTG_HS_PCD_Init() + HAL_PCD_Start()) once ThreadX is running. */
PCD_HandleTypeDef hpcd_USB_OTG_HS1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_I2C2_Init(void);
static void MX_DCMIPP_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
static void Camera_CheckFrameBuffer(void);
static void I2C_ScanBus(I2C_HandleTypeDef *hi2c);
static HAL_StatusTypeDef MX_DCMIPP_ClockConfig(void);

static ISP_StatusTypeDef GetSensorInfoHelper(uint32_t Instance, ISP_SensorInfoTypeDef *SensorInfo);
static ISP_StatusTypeDef SetSensorGainHelper(uint32_t Instance, int32_t Gain);
static ISP_StatusTypeDef GetSensorGainHelper(uint32_t Instance, int32_t *Gain);
static ISP_StatusTypeDef SetSensorExposureHelper(uint32_t Instance, int32_t Exposure);
static ISP_StatusTypeDef GetSensorExposureHelper(uint32_t Instance, int32_t *Exposure);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	ISP_AppliHelpersTypeDef appliHelpers = {0};
  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPUART1_UART_Init();
  MX_I2C2_Init();
  if (MX_DCMIPP_ClockConfig() != HAL_OK)
  {
      Error_Handler();
  }
  MX_DCMIPP_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */
  // ==============Check Sensor==========================
  /*
   * CAM_NRST (GPIOO_5) and EN_MODULE (GPIOA_0) were never actually
   * initialized as GPIO outputs anywhere in this file -- MX_GPIO_Init()
   * only enables port clocks (E/B/A, not even O), it never calls
   * HAL_GPIO_Init() for any pin. Every HAL_GPIO_WritePin() below was
   * writing to pins left in whatever state they happened to be in.
   *
   * That went unnoticed as long as Appli was loaded fresh over SWD (chip
   * reset defaults / this board's external pull-up apparently kept
   * CAM_NRST high anyway). It breaks the moment Appli boots via FSBL:
   * FSBL reconfigures many GPIOs for its external XSPI flash interface
   * (likely including port O) before jumping here, and without
   * explicitly reclaiming these two pins as push-pull outputs, the
   * camera can end up held in reset / EN_MODULE never actually driven --
   * matching the symptom (I2C2 ACKs nothing at all, camera's own status
   * LED never lights). Camera_N6_AI_Test (confirmed working on this same
   * hardware) explicitly calls HAL_GPIO_Init() for both pins before
   * touching them; this now does the same.
   *
   * Order still matters: release/assert CAM_NRST *before* driving
   * EN_MODULE high, not after (see Camera_N6_AI_Test's own comments
   * calling EN_MODULE sequencing "CRITICAL").
   */
  __HAL_RCC_GPIOO_CLK_ENABLE();
  {
      GPIO_InitTypeDef gpio_nrst = {0};
      gpio_nrst.Pin   = GPIO_PIN_5;
      gpio_nrst.Mode  = GPIO_MODE_OUTPUT_PP;
      gpio_nrst.Pull  = GPIO_NOPULL;
      gpio_nrst.Speed = GPIO_SPEED_FREQ_LOW;
      HAL_GPIO_Init(GPIOO, &gpio_nrst);
  }

  HAL_GPIO_WritePin(GPIOO,
                    GPIO_PIN_5,
                    GPIO_PIN_RESET);

  HAL_Delay(10);

  HAL_GPIO_WritePin(GPIOO,
                    GPIO_PIN_5,
                    GPIO_PIN_SET);

  HAL_Delay(10);   /* IMX219 t2 power-up delay */

  /* GPIOA clock is already enabled by MX_GPIO_Init(), but the pin mode
   * itself still needs to be set -- MX_GPIO_Init() never does that. */
  {
      GPIO_InitTypeDef gpio_en = {0};
      gpio_en.Pin   = GPIO_PIN_0;
      gpio_en.Mode  = GPIO_MODE_OUTPUT_PP;
      gpio_en.Pull  = GPIO_NOPULL;
      gpio_en.Speed = GPIO_SPEED_FREQ_LOW;
      HAL_GPIO_Init(GPIOA, &gpio_en);
  }

  HAL_GPIO_WritePin(GPIOA,
                    GPIO_PIN_0,
                    GPIO_PIN_SET);

  HAL_Delay(5);    /* EN_MODULE settling, matches Camera_N6_AI_Test */

  /* Diagnostic: scan I2C2 for any responding device before trying the
   * IMX219 specifically (IMX219_I2C_ADDR = 0x10 << 1 = 0x20). If nothing
   * shows up at all, the problem is the bus/power/reset wiring to the
   * camera connector, not the IMX219 driver itself. */
  I2C_ScanBus(&hi2c2);

  imx219_ctx.handle   = &hi2c2;
  imx219_ctx.ReadReg  = IMX219_I2C_ReadReg;
  imx219_ctx.WriteReg = IMX219_I2C_WriteReg;

  if (IMX219_Init(&imx219_ctx) != 0)
  {
      printf( "IMX219 initialization FAILED\r\n");
  }
  else
  {
      printf("IMX219 initialization OK\r\n");
  }
  /* ============================================================
   * Prepare frame buffer
   * ============================================================ */

  printf("\r\n");
  printf("Preparing frame buffer...\r\n");
  /*
   * Clear the buffer before capture.
   *
   * This allows us to determine later whether DCMIPP
   * actually wrote data into it.
   */
  memset(camera_framebuffer,0x00,FRAME_BUFFER_SIZE);


  /*
   * Reset capture state.
   */
  frame_count = 0U;

  frame_received = 0U;


  /*
   * Since the CPU cache is enabled and the buffer may have
   * previously been cached, clean the cache before DMA starts.
   */
  printf("Testing framebuffer CPU write...\r\n");

  camera_framebuffer[0] = 0xAA;
  camera_framebuffer[1] = 0x55;
  camera_framebuffer[2] = 0x12;
  camera_framebuffer[3] = 0x34;

  SCB_CleanDCache_by_Addr(
      (uint32_t *)camera_framebuffer,
      FRAME_BUFFER_SIZE
  );

  printf(
      "Before capture: %02X %02X %02X %02X\r\n",
      camera_framebuffer[0],
      camera_framebuffer[1],
      camera_framebuffer[2],
      camera_framebuffer[3]
  );

  /*
   * Bug 17 investigation (see WORKLOG.md): the periodic 32-row 0xFF
   * corruption survived two independent, hardware-confirmed-correct
   * IPPlug reconfigurations (MemoryPageSize and MaxOutstandingTransactions),
   * ruling out the AXI write-master's timing/throughput settings entirely.
   * Before chasing the ISP/pixel-pipe hardware next, rule out the simplest
   * remaining explanation: that these exact byte addresses are not really
   * writable RAM at all (a physical gap/alias, not a DMA problem). The
   * memset() above should have zeroed every byte in this buffer, and the
   * SCB_CleanDCache_by_Addr() call just above flushed that write out to
   * physical memory. If these specific bytes don't read back 0x00 here --
   * before DCMIPP has ever touched the buffer -- then DCMIPP was never at
   * fault: the CPU's own write didn't stick, meaning this address range is
   * not genuinely backed, writable memory.
   */
  /* Bug 17-19 diagnostic, no longer needed now that Bug 19's IPPlug
   * WLRURatio/DPREGStart/DPREGEnd fix confirmed the striping fixed (see
   * WORKLOG.md) -- flip to 1 to re-run this write-back check if a similar
   * corruption is ever suspected again. */
#define DEBUG_MEM_TEST 0
#if DEBUG_MEM_TEST
  printf("[MEM_TEST] pre-capture CPU write-back check at known-bad-row offsets:\r\n");
  {
      static const uint32_t bad_rows[] = {24U, 56U, 88U, 120U, 152U, 184U, 216U,
                                          248U, 280U, 312U, 344U, 376U, 408U, 440U, 472U};
      SCB_InvalidateDCache_by_Addr((uint32_t *)camera_framebuffer, FRAME_BUFFER_SIZE);
      for (uint32_t _i = 0U; _i < (sizeof(bad_rows) / sizeof(bad_rows[0])); _i++)
      {
          uint32_t _row = bad_rows[_i];
          uint8_t _b0 = camera_framebuffer[_row * 1280U];
          uint8_t _b1 = camera_framebuffer[_row * 1280U + 1U];
          printf("  row%03lu byte0/1 = %02X %02X %s\r\n", (unsigned long)_row, _b0, _b1,
                 ((_b0 == 0U) && (_b1 == 0U)) ? "OK" : "*** STUCK, NOT WRITABLE ***");
      }
  }
#endif

//
  /* Fill init struct with Camera driver helpers */
  appliHelpers.GetSensorInfo = GetSensorInfoHelper;
  appliHelpers.SetSensorGain = SetSensorGainHelper;
  appliHelpers.GetSensorGain = GetSensorGainHelper;
  appliHelpers.SetSensorExposure = SetSensorExposureHelper;
  appliHelpers.GetSensorExposure = GetSensorExposureHelper;

  /* Initialize the Image Signal Processing middleware */
  if(ISP_Init(&hcamera_isp, &hdcmipp, 0, &appliHelpers, ISP_IQParamCacheInit[0]) != ISP_OK)
  {
	printf("ISP Init failed\r\n");
    Error_Handler();
  }
  printf("ISP Init OK\r\n");

  printf("STAT AREA:\r\n");
  printf("X0=%lu\r\n", hcamera_isp.statArea.X0);
  printf("Y0=%lu\r\n", hcamera_isp.statArea.Y0);
  printf("XSIZE=%lu\r\n", hcamera_isp.statArea.XSize);
  printf("YSIZE=%lu\r\n", hcamera_isp.statArea.YSize);

  printf("before ISP_Start\r\n");

  if(ISP_Start(&hcamera_isp)!=ISP_OK)
  {
      printf("ISP start failed\r\n");
      Error_Handler();
  }

  printf("after ISP_Start\r\n");

  /*
   * Bug 17 investigation, round 3 (see WORKLOG.md): the full 480-row scan
   * found an exact, deterministic period-32 pattern -- 12 of every 32 rows
   * corrupted at fixed offsets, present even with zero system load. This
   * has already ruled out AXI/IPPlug timing (two parameters changed,
   * hardware-confirmed via register readback, zero effect) and the
   * destination memory itself (CPU write-back test passed on every
   * known-bad row). That leaves the hardware pixel pipe between CSI
   * ingest and the AXI write: either the raw CSI/DCMIPP capture path
   * itself, or the ISP's hardware Bayer2RGB demosaic block PIPE1 always
   * runs data through (DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1 requires it --
   * there's no raw-passthrough packer format available on PIPE1).
   *
   * This is a one-shot bisection test: disable the demosaic block only
   * (leaving everything else -- CSI, IPPlug, pixel packer -- exactly as
   * configured) and re-run the same [MEM_SCAN]. The image itself will
   * look wrong (raw Bayer data reinterpreted as RGB565, not real colors)
   * but that doesn't matter for this test -- only whether the same
   * period-32 0xFF pattern is still there:
   *   - Pattern GONE  -> the demosaic hardware block is the culprit.
   *   - Pattern SAME  -> demosaic is innocent; the bug is upstream, in
   *                      CSI reception or DCMIPP's own raw capture path.
   * TEMPORARY -- revert this block (or flip the #if to 0) once the test
   * result is read; do not ship with demosaic disabled.
   */
#define BUG17_DEMOSAIC_BISECT_TEST 0
#if BUG17_DEMOSAIC_BISECT_TEST
  printf("[BUG17_TEST] disabling ISP RawBayer2RGB demosaic for this run\r\n");
  if (HAL_DCMIPP_PIPE_DisableISPRawBayer2RGB(&hdcmipp, DCMIPP_PIPE1) != HAL_OK)
  {
      printf("[BUG17_TEST] HAL_DCMIPP_PIPE_DisableISPRawBayer2RGB FAILED\r\n");
  }
#endif

  /* ============================================================
   * Start DCMIPP snapshot
   * ============================================================ */

  printf("DCMIPP: starting capture\r\n");

  if (HAL_DCMIPP_CSI_PIPE_Start(
          &hdcmipp,
          DCMIPP_PIPE1,
          DCMIPP_VIRTUAL_CHANNEL0,
          (uint32_t)camera_framebuffer,
          DCMIPP_MODE_CONTINUOUS) != HAL_OK)
  {
      printf("DCMIPP continuous failed\r\n");
      Error_Handler();
  }
  printf("DCMIPP continuous OK\r\n");

  printf("DCMIPP capture started\r\n");
  printf("Requested frame buffer address = 0x%08lX\r\n",
         (uint32_t)camera_framebuffer);
  printf("P1PPM0AR1 = 0x%08lX\r\n", DCMIPP->P1PPM0AR1);
  printf("P1FCTCR   = 0x%08lX\r\n", DCMIPP->P1FCTCR);
  printf("P1PPCR    = 0x%08lX\r\n", DCMIPP->P1PPCR);
  printf("CMCR      = 0x%08lX\r\n", DCMIPP->CMCR);

  /* ============================================================
   * Start IMX219 streaming
   * ============================================================ */

  printf("IMX219: starting streaming\r\n");

  if (IMX219_Start(&imx219_ctx) != 0)
  {
      printf("IMX219: start failed\r\n");

      Error_Handler();
  }

  printf("IMX219: streaming started\r\n");

  /* Start the Image Signal Processing */
  //HAL_Delay(100);


//  printf("before ISP_Start\r\n");
//
//  if(ISP_Start(&hcamera_isp)!=ISP_OK)
//  {
//      printf("ISP start failed\r\n");
//      Error_Handler();
//  }
//
//  printf("after ISP_Start\r\n");

  /* give the ISP 60 frames to set color balance */
  {
      uint32_t bgp_start_tick = HAL_GetTick();
      uint32_t bgp_last_print = bgp_start_tick;
      uint8_t  bgp_timed_out  = 0U;

      printf("Waiting for AWB/AE warm-up (60 frames, PIPE1 continuous)...\r\n");

      while(frame_count < 60)
      {
        if (ISP_BackgroundProcess(&hcamera_isp) != ISP_OK)
        {
          printf("BGP failed\r\n");
          BSP_LED_Toggle(LED_RED);
        }

        /* This loop used to be able to hang forever with zero output if
         * DCMIPP never completes a single frame on PIPE1 (frame_count is
         * only incremented from HAL_DCMIPP_PIPE_FrameEventCallback, which
         * needs a real frame-complete interrupt). Print progress every
         * ~1s and bail out with diagnostics after 10s instead of hanging
         * silently. */
        if ((HAL_GetTick() - bgp_last_print) >= 1000U)
        {
            bgp_last_print = HAL_GetTick();
            printf("  [warmup] frame_count=%lu  CSI_IRQ=%lu  DCMIPP_IRQ=%lu  "
                   "SOT_L0=%lu  SOT_L1=%lu\r\n",
                   frame_count, csi_irq_count, dcmipp_irq_count,
                   sot_lane0_count, sot_lane1_count);
        }

        if ((HAL_GetTick() - bgp_start_tick) >= 10000U)
        {
            bgp_timed_out = 1U;
            break;
        }
      }

      if (bgp_timed_out)
      {
          printf("\r\n");
          printf("AWB/AE WARMUP TIMEOUT (10s, frame_count=%lu / 60)\r\n",
                 frame_count);
          printf("CSI IRQ count    = %lu\r\n", csi_irq_count);
          printf("DCMIPP IRQ count = %lu\r\n", dcmipp_irq_count);
          printf("SOT lane0 count  = %lu\r\n", sot_lane0_count);
          printf("SOT lane1 count  = %lu\r\n", sot_lane1_count);
          printf("CSI SR0  = 0x%08lX\r\n", CSI->SR0);
          printf("CSI SR1  = 0x%08lX\r\n", CSI->SR1);
          printf("P1SR     = 0x%08lX\r\n", DCMIPP->P1SR);
          printf("CMSR2    = 0x%08lX\r\n", DCMIPP->CMSR2);
          printf("\r\n");
      }
      else
      {
          printf("BGP OK\r\n");
      }

      /* Bug 15-19 diagnostics, no longer needed now that Bug 19's IPPlug
       * fix confirmed the striping fixed (see WORKLOG.md) -- flip to 1 to
       * re-run this row/frame corruption scan if similar symptoms recur. */
#define DEBUG_MEM_SCAN 0
#if DEBUG_MEM_SCAN
      /*
       * Bug 15 investigation, round 3 (see WORKLOG.md): the UVC-streamed
       * image is corrupted past roughly row 68 of every frame, but DCMIPP
       * itself reports zero errors (no overrun, no pipe error, no D-PHY
       * fault) the entire time -- ruling out every hypothesis tried so
       * far. This is a controlled experiment to isolate whether that
       * truncation is inherent to CONTINUOUS-mode capture itself, or
       * specific to running under RTOS+USB+JPEG system load: this warmup
       * loop just finished dozens of continuous-mode PIPE1 captures into
       * this exact same camera_framebuffer address, with ZERO USB/JPEG
       * DMA activity competing for AXI bandwidth (ThreadX/USBX haven't
       * even started yet at this point in main()). Dump the same
       * row-60/68/76 pattern the UVC path checks -- if row 68+ is already
       * 0xFF here too, the bug has nothing to do with USB/JPEG contention;
       * if it's real valid data here, contention is confirmed as the
       * cause.
       */
      SCB_InvalidateDCache_by_Addr((uint32_t *)camera_framebuffer, FRAME_BUFFER_SIZE);
      printf("[WARMUP_SRC] row60  first32B: ");
      for (uint32_t _i = 0U; _i < 32U; _i++)
          printf("%02X ", camera_framebuffer[60U * 1280U + _i]);
      printf("\r\n[WARMUP_SRC] row68  first32B: ");
      for (uint32_t _i = 0U; _i < 32U; _i++)
          printf("%02X ", camera_framebuffer[68U * 1280U + _i]);
      printf("\r\n[WARMUP_SRC] row76  first32B: ");
      for (uint32_t _i = 0U; _i < 32U; _i++)
          printf("%02X ", camera_framebuffer[76U * 1280U + _i]);
      printf("\r\n[WARMUP_SRC] row200 first32B: ");
      for (uint32_t _i = 0U; _i < 32U; _i++)
          printf("%02X ", camera_framebuffer[200U * 1280U + _i]);
      printf("\r\n[WARMUP_SRC] row400 first32B: ");
      for (uint32_t _i = 0U; _i < 32U; _i++)
          printf("%02X ", camera_framebuffer[400U * 1280U + _i]);
      printf("\r\n");

      /*
       * Bug 15 investigation, round 4: rows 68/76 read as 0xFF while rows
       * 200/400 read as real data, EVEN WITH ZERO SYSTEM LOAD (this is the
       * pre-RTOS warmup loop). That is not a truncation pattern -- it is a
       * *localized band* of bad memory somewhere between row ~68 and row
       * 200, with good memory both before and after it. camera_framebuffer
       * lives at a hardcoded fixed address (CAMERA_BUFFER_ADDR =
       * 0x34200000) that sits exactly at the end of the linker's own `RAM`
       * region (STM32N657X0HXQ_LRUN.ld: ORIGIN 0x34000400, LENGTH 2047K ->
       * ends at 0x34200000) -- i.e. it was deliberately placed in a
       * *different*, separate physical RAM bank specifically so it
       * wouldn't eat into the tight 2047K budget everything else shares.
       * The suspicion: that separate bank (or the next one after it) may
       * not be as large / contiguous as assumed, and part of this 614400-
       * byte buffer may fall into a real gap between two physical SRAM
       * blocks (unbacked address space reads back as 0xFF on this chip).
       * Binary-search the actual boundaries empirically instead of relying
       * on remembered reference-manual numbers -- byte 0 of every 8th row
       * is enough to map where "valid" flips to "0xFF" and back.
       */
      /*
       * Bug 17 investigation, round 2 (see WORKLOG.md): the coarse every-8th-
       * row scan below made the bad rows look like a clean "every 32nd row"
       * period (24, 56, 88, ...) -- but this project's own earlier
       * row-60/68/76/200/400 spot-checks (still printed above, unchanged)
       * showed row 68 and row 76 BOTH bad while the coarse scan's neighbors
       * at 64/72/80 are all good -- i.e. the true bad-row set is NOT evenly
       * spaced at exactly 32, the 8-row stride was just aliasing a messier
       * pattern into a falsely clean-looking one. Scan EVERY row (not every
       * 8th) and check three columns per row (start/middle/end), so the next
       * log shows the real shape of the defect instead of a stride artifact:
       * whether it's a clean period, an irregular/jittery pattern (pointing
       * at a CSI line-sync glitch rather than an AXI/IPPlug timing issue,
       * both of which are now ruled out -- see WORKLOG.md), and whether each
       * bad row is corrupted end-to-end or only partially.
       */
      printf("[MEM_SCAN] full 480-row scan, col0 bitmap ('.'=ok '#'=FFFF):\r\n");
      {
          static uint8_t bad[480];
          uint32_t bad_count = 0U;
          uint32_t full_line_bad_count = 0U;
          uint32_t prev_bad_row = 0xFFFFFFFFU;
          uint32_t min_gap = 0xFFFFFFFFU;
          uint32_t max_gap = 0U;

          for (uint32_t _row = 0U; _row < 480U; _row++)
          {
              uint16_t _col0 = ((uint16_t)camera_framebuffer[_row * 1280U] << 8) |
                                camera_framebuffer[_row * 1280U + 1U];
              bad[_row] = (_col0 == 0xFFFFU) ? 1U : 0U;

              if (bad[_row])
              {
                  uint16_t _colmid = ((uint16_t)camera_framebuffer[_row * 1280U + 640U] << 8) |
                                      camera_framebuffer[_row * 1280U + 641U];
                  uint16_t _colend = ((uint16_t)camera_framebuffer[_row * 1280U + 1278U] << 8) |
                                      camera_framebuffer[_row * 1280U + 1279U];
                  if ((_colmid == 0xFFFFU) && (_colend == 0xFFFFU))
                  {
                      full_line_bad_count++;
                  }
                  bad_count++;
                  if (prev_bad_row != 0xFFFFFFFFU)
                  {
                      uint32_t _gap = _row - prev_bad_row;
                      if (_gap < min_gap) { min_gap = _gap; }
                      if (_gap > max_gap) { max_gap = _gap; }
                  }
                  prev_bad_row = _row;
              }
          }

          for (uint32_t _row = 0U; _row < 480U; _row++)
          {
              printf("%c", bad[_row] ? '#' : '.');
              if (((_row + 1U) % 64U) == 0U) { printf("\r\n"); }
          }

          printf("\r\n[MEM_SCAN] bad_rows=%lu (of which full-line bad=%lu)  "
                 "gap_between_bad_rows: min=%lu max=%lu\r\n",
                 (unsigned long)bad_count, (unsigned long)full_line_bad_count,
                 (unsigned long)((min_gap == 0xFFFFFFFFU) ? 0U : min_gap),
                 (unsigned long)max_gap);

          printf("[MEM_SCAN] bad row numbers: ");
          for (uint32_t _row = 0U; _row < 480U; _row++)
          {
              if (bad[_row]) { printf("%lu ", (unsigned long)_row); }
          }
          printf("\r\n");
      }
      printf("[MEM_SCAN] done\r\n");
#endif /* DEBUG_MEM_SCAN */
  }

  /* stop the acquisition */
  HAL_DCMIPP_CSI_PIPE_Stop(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0);



  frame_count = 0;
  frame_received = 0;
  /* Start Snapshot again */

  if (HAL_DCMIPP_CSI_PIPE_Start(
		  &hdcmipp,
		  DCMIPP_PIPE1,
		  DCMIPP_VIRTUAL_CHANNEL0 ,
		  (uint32_t)camera_framebuffer,
		  DCMIPP_MODE_SNAPSHOT) != HAL_OK)
  {
	printf("DCMIPP snapshot failed\r\n");
    Error_Handler();
  }
  printf("DCMIPP snapshot OK\r\n");

  /* ============================================================
   * Verify sensor streaming
   * ============================================================ */

  uint8_t mode = 0U;

  if (IMX219_ReadReg(
          &imx219_ctx,
          0x0100,
          &mode,
          1) == 0)
  {
      printf(
          "MODE_SELECT after start = 0x%02X\r\n",
          mode
      );
  }


  /* ============================================================
   * Wait for frame
   * ============================================================ */

  printf("Waiting for frame...\r\n");

  uint32_t start_time = HAL_GetTick();

  while (frame_received == 0U)
  {
      if ((HAL_GetTick() - start_time) >= 5000U)
      {
          printf("\r\n");
          printf("CAPTURE TIMEOUT\r\n");

          printf(
              "CSI IRQ count    = %lu\r\n",
              csi_irq_count
          );

          printf(
              "DCMIPP IRQ count = %lu\r\n",
              dcmipp_irq_count
          );

          printf(
              "Frame count      = %lu\r\n",
              frame_count
          );

          break;
      }
  }


  /* ============================================================
   * Frame result
   * ============================================================ */

  if (frame_received != 0U)
  {
      printf("\r\n");
      printf("FRAME RECEIVED\r\n");
      /* ADD HERE */
      uint32_t data_counter = 0U;

      /*
       * HAL_DCMIPP_PIPE_GetDataCounter() ignores the Pipe argument and
       * always reads P0DCCNTR (Pipe0's dump counter) regardless of what's
       * passed here -- an ST HAL limitation, not a bug in this file. Pipe0
       * is never started in this app, so this will always read 0 for
       * PIPE1 no matter how the capture actually went. Trust
       * Camera_CheckFrameBuffer()'s Non-zero-bytes/checksum instead.
       */
      if (HAL_DCMIPP_PIPE_GetDataCounter(
              &hdcmipp,
              DCMIPP_PIPE1,
              &data_counter) == HAL_OK)
      {
          printf(
              "DCMIPP data counter = %lu\r\n",
              data_counter
          );
          //
          {
              uint32_t p1sr  = DCMIPP->P1SR;
              uint32_t cmsr2 = DCMIPP->CMSR2;

              printf("P1SR  = 0x%08lX  (OVRF=%lu LSTFRM=%lu LSTLINE=%lu)\r\n",
                     p1sr,
                     (p1sr & DCMIPP_P1SR_OVRF)  ? 1UL : 0UL,
                     (p1sr & DCMIPP_P1SR_LSTFRM) ? 1UL : 0UL,
                     (p1sr & DCMIPP_P1SR_LSTLINE) ? 1UL : 0UL);
              printf("CMSR2 = 0x%08lX  (P1OVRF=%lu)\r\n",
                     cmsr2,
                     (cmsr2 & DCMIPP_CMSR2_P1OVRF) ? 1UL : 0UL);
              printf("PIPE1 OVERRUN: %s\r\n",
                     (p1sr & DCMIPP_P1SR_OVRF) ? "YES (still broken)" : "no");
          }
      }
      else
      {
          printf("Failed to read DCMIPP data counter\r\n");
      }

      /*
       * DCMIPP/DMA wrote the frame into RAM.
       *
       * Invalidate the CPU cache so that the CPU reads
       * the new data written by DCMIPP.
       */
      SCB_InvalidateDCache_by_Addr(
          (uint32_t *)camera_framebuffer,
          FRAME_BUFFER_SIZE
      );
      printf("D-Cache invalidated\r\n");

      printf(
          "After capture: %02X %02X %02X %02X\r\n",
          camera_framebuffer[0],
          camera_framebuffer[1],
          camera_framebuffer[2],
          camera_framebuffer[3]
      );

      /*
       * Check whether the buffer contains real data.
       */
      Camera_CheckFrameBuffer();
  }
  else
  {
      printf("\r\n");
      printf("NO FRAME RECEIVED\r\n");
  }
  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_RED);
  BSP_LED_Init(LED_GREEN);

  /* USER CODE BEGIN WHILE */
  /*
   * Single-shot capture verification above is done (frame confirmed
   * good/bad on real hardware, exactly as before -- untouched). Instead of
   * the old LED-blink idle loop, hand off to ThreadX: MX_ThreadX_Init()
   * starts the RTOS kernel, whose capture thread (app_threadx.c) restarts
   * PIPE1 in continuous mode and streams RGB565->MJPEG frames out over USB
   * UVC (app_usbx_device.c / ux_device_video.c). tx_kernel_enter() never
   * returns.
   */
  BSP_LED_On(LED_GREEN);
  printf("\r\n[MAIN] Single-shot verification complete -- starting ThreadX/USBX UVC pipeline\r\n");
  MX_ThreadX_Init();

  /* Unreachable. */
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  while (1)
  {
  }
  /* USER CODE END 3 */
}

/**
  * @brief  Configure the DCMIPP pixel clock and CSI D-PHY config clock.
  *
  * Neither of these was ever configured anywhere in this file before --
  * MX_DCMIPP_Init() only touches the DCMIPP/CSI peripheral's own config
  * registers, never RCC. Without a CSI D-PHY reference clock in
  * particular, the D-PHY receiver just sits in Ultra-Low-Power/idle state
  * forever: confirmed on hardware via CSI->SR1 showing ULPNCLF/ULPNACTF/
  * ULPNDL0F/ULPNDL1F all set and SOT_L0/SOT_L1/CSI_IRQ counters staying
  * at 0 indefinitely (see the AWB/AE warmup-loop diagnostics in main()) --
  * the sensor's own I2C-configured streaming mode doesn't matter if the
  * receiver's control clock was never running to begin with. Matches
  * Camera_N6_AI_Test's MX_DCMIPP_ClockConfig(), confirmed working on this
  * same hardware/PLL1 tree (PLL1 = 1200MHz on both projects, verified
  * identical PLLM/N/P1/P2 in each project's FSBL).
  * @retval HAL_OK or an HAL_RCCEx_PeriphCLKConfig error status.
  */
static HAL_StatusTypeDef MX_DCMIPP_ClockConfig(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  HAL_StatusTypeDef ret;

  /* DCMIPP pixel clock via IC17 */
  clk.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  clk.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  clk.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC17].ClockDivider   = 4;    /* PLL1(1200MHz)/4 = 300 MHz */
  ret = HAL_RCCEx_PeriphCLKConfig(&clk);
  if (ret != HAL_OK)
  {
      return ret;
  }

  /* CSI D-PHY config clock via IC18 */
  clk.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  clk.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC18].ClockDivider   = 60;   /* PLL1(1200MHz)/60 = 20 MHz */
  ret = HAL_RCCEx_PeriphCLKConfig(&clk);
  if (ret != HAL_OK)
  {
      return ret;
  }

  return HAL_OK;
}

/**
  * @brief DCMIPP Initialization Function
  * @param None
  * @retval None
  */
static void MX_DCMIPP_Init(void)
{

  /* USER CODE BEGIN DCMIPP_Init 0 */

  /* USER CODE END DCMIPP_Init 0 */

  DCMIPP_CSI_PIPE_ConfTypeDef pCSI_PipeConfig = {0};
  DCMIPP_CSI_ConfTypeDef pCSI_Config = {0};
  DCMIPP_PipeConfTypeDef pPipeConfig = {0};

  /* USER CODE BEGIN DCMIPP_Init 1 */

  /* USER CODE END DCMIPP_Init 1 */
  hdcmipp.Instance = DCMIPP;
  if (HAL_DCMIPP_Init(&hdcmipp) != HAL_OK)
  {
    Error_Handler();
  }

  /** Pipe 1 Config
  */
  pCSI_PipeConfig.DataTypeMode = DCMIPP_DTMODE_DTIDA;
  pCSI_PipeConfig.DataTypeIDA = DCMIPP_DT_RAW10;
  pCSI_PipeConfig.DataTypeIDB = DCMIPP_DT_RAW10;
  if (HAL_DCMIPP_CSI_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &pCSI_PipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* Bug 18 (see WORKLOG.md): must match imx219.c's PLL_OP_MPY (register
   * 0x030D). That was found hand-edited to a value giving ~224 Mbps/lane
   * instead of the correct ~912 Mbps/lane for this exact sensor/resolution
   * (confirmed against ../Camera_N6_AI_Test, which uses 0x72 + BT_900 on
   * the same IMX219 640x480 RAW10 2-lane 30fps config and has no striping).
   * Running the D-PHY receiver's bit-rate calibration ~4x below the link's
   * actual intended speed is almost certainly the root cause of the exact,
   * deterministic period-32-row corruption chased through Bugs 15-17 --
   * a physical-layer mismatch no DCMIPP/ISP register can compensate for. */
  pCSI_Config.PHYBitrate = DCMIPP_CSI_PHY_BT_900;
  pCSI_Config.DataLaneMapping = DCMIPP_CSI_PHYSICAL_DATA_LANES;
  pCSI_Config.NumberOfLanes = DCMIPP_CSI_TWO_DATA_LANES;
  HAL_DCMIPP_CSI_SetConfig(&hdcmipp, &pCSI_Config);
  pPipeConfig.FrameRate = DCMIPP_FRAME_RATE_ALL;
  pPipeConfig.PixelPipePitch = 1280; //640 for RAW8, 1280 for RGB565
  pPipeConfig.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
  if (HAL_DCMIPP_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &pPipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DCMIPP_CSI_SetVCConfig(&hdcmipp, 0U, DCMIPP_CSI_DT_BPP10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMIPP_Init 2 */

  /*
   * PIPE1 IPPlug (AXI write-master) config.
   *
   * Without this, PIPE1's AXI client (CLIENT2) is left at its
   * hardware-reset defaults: effectively no internal FIFO depth and no
   * outstanding-transaction headroom. The extra latency added by the
   * ISP Bayer2RGB pipeline stage between the CSI input and the AXI
   * write is enough for that near-zero buffer to fill up mid-line,
   * which is what P1SR's Overrun flag and the truncated
   * "Non-zero bytes" count (see Camera_README.md, PIPE1 section) show:
   * the pipe stalls a few lines into the frame instead of completing it.
   * PIPE0 is never started in this app, so it is left unconfigured and
   * CLIENT2 can safely take the whole FIFO pool.
   */
  /*
   * Bug 16 (see WORKLOG.md): MemoryPageSize (64 bytes) was configured
   * SMALLER than Traffic's AXI burst size (128 bytes) -- backwards from
   * what this field is for (it describes the memory-side page/row boundary
   * IPPlug must not let a single burst straddle). This has been wrong
   * since this exact IPPlug config was first added (the original PIPE1
   * overrun fix, this session's very first bug), and produced a perfectly
   * periodic corruption -- every 32nd captured row (rows congruent to a
   * fixed value mod 32, confirmed via a full 480-row memory scan) read
   * back as untouched 0xFF -- present even with zero system load, so not
   * a USB/JPEG contention issue. Every existing diagnostic in this project
   * only ever printed/checked row 0 (never in the corrupted phase by
   * construction, since the corrupted rows are offset from a multiple of
   * 32), which is why a burst/page mismatch that's been present since the
   * very first camera bring-up was never caught until the UVC pipeline
   * finally made the full frame visible.
   *
   * Bug 17 (see WORKLOG.md): the 256-byte MemoryPageSize fix above was
   * flashed and produced a byte-for-byte IDENTICAL 32-row periodic 0xFF
   * pattern -- proof this specific field was never the real lever (the
   * old confirmation printf below only read back IPC2R1/R2/R3, the
   * per-client registers, and never actually checked IPGR1, the register
   * MemoryPageSize is written to -- so the previous "fix" was flashed
   * without ever confirming the write took hold in hardware at all).
   * Two changes this round: (1) print IPGR1 too, so a stale/rejected write
   * is now visible instead of assumed; (2) as the next candidate lever
   * per this project's own contingency plan, cut MaxOutstandingTransactions
   * way down (16 -> 4) -- if 16 in-flight 128B writes is more than this
   * particular AXI target bank can sustain, its own internal queue could
   * be silently dropping/corrupting writes on a fixed period unrelated to
   * MemoryPageSize entirely, which would explain why quadrupling the page
   * size changed nothing.
   */
  /*
   * Bug 19 (see WORKLOG.md): Bug 18's IMX219 PLL/binning/frame-length fixes
   * were flashed, confirmed to actually reach hardware (different real
   * pixel brightness values captured), and STILL produced the exact same
   * byte-for-byte period-32 corruption. Combined with Bugs 16/17 (IPPlug
   * MemoryPageSize/MaxOutstandingTransactions, also confirmed via register
   * readback) and the demosaic bisection test, every hypothesis tried so
   * far has been conclusively falsified with hardware evidence. Two IPPlug
   * fields were never actually varied: WLRURatio (AXI arbitration weight)
   * and DPREGStart/DPREGEnd (this client's slice of the shared internal
   * FIFO). ../Camera_N6_AI_Test's CLIENT2 (its own PIPE1, the confirmed-
   * working active capture pipe) uses WLRURatio=4 (not 15) and
   * DPREGStart/End=0x100/0x1FF, a 256-word half of the FIFO (not the full
   * 0x000-0x3FF pool this project gives CLIENT2). Matching those exactly,
   * along with MemoryPageSize/Traffic (both back to 64 bytes, equal to
   * each other -- the reference proves 64/64 works fine, so Bug 16's
   * "page must be > burst" theory was never right either) and
   * MaxOutstandingTransactions=8, to fully exhaust the IPPlug parameter
   * space against a known-working configuration instead of guessing
   * further blind.
   */
  DCMIPP_IPPlugConfTypeDef pIPPlugConfig = {0};
  pIPPlugConfig.Client                     = DCMIPP_CLIENT2;
  pIPPlugConfig.MemoryPageSize             = DCMIPP_MEMORY_PAGE_SIZE_64BYTES;
  pIPPlugConfig.Traffic                    = DCMIPP_TRAFFIC_BURST_SIZE_64BYTES;
  pIPPlugConfig.MaxOutstandingTransactions = DCMIPP_OUTSTANDING_TRANSACTION_8;
  pIPPlugConfig.WLRURatio                  = 4U;
  pIPPlugConfig.DPREGStart                 = 0x100U;
  pIPPlugConfig.DPREGEnd                   = 0x1FFU;
  if (HAL_DCMIPP_SetIPPlugConfig(&hdcmipp, &pIPPlugConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* Read back what actually landed in hardware, not just what we asked
   * for -- confirms the IPPlug fix took effect before any capture starts.
   * Bug 17: IPGR1 (MemoryPageSize, a GLOBAL register, not per-client) was
   * missing from this readback -- add it so a rejected/stale page-size
   * write is visible instead of silently assumed. */
  printf("IPPlug CLIENT2: IPGR1=0x%08lX IPC2R1=0x%08lX IPC2R2=0x%08lX IPC2R3=0x%08lX\r\n",
         DCMIPP->IPGR1, DCMIPP->IPC2R1, DCMIPP->IPC2R2, DCMIPP->IPC2R3);

  /* USER CODE END DCMIPP_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10C0ECFF;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

  /* USER CODE BEGIN RIF_Init 0 */

  /* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;

  /*RIMC configuration*/
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);

  /*
   * Bug 10 (see WORKLOG.md): RIF_MASTER_INDEX_OTG1 (RIMC) and
   * RIF_RISC_PERIPH_INDEX_OTG1HS / RIF_RISC_PERIPH_INDEX_JPEG (RISC) were
   * never added here when the UVC/ThreadX pipeline introduced USB and the
   * HW JPEG encoder -- this function predates both and was never revisited.
   * The comment below (from the original I2C RIF bug, Bug 2) already
   * documented that the reference project's Security_Config() sets exactly
   * these for OTG1/OTG1HS/JPEG -- it just never got acted on until now.
   * Symptom without this: USB enumerates far enough to chirp/negotiate High
   * Speed (host sees "new high-speed USB device"), but every subsequent
   * control transfer that actually moves data over AXI via the OTG core's
   * internal DMA (dma_enable=ENABLE in MX_USB1_OTG_HS_PCD_Init) times out --
   * `dmesg`: "device descriptor read/64, error -110" -- because RIF
   * silently drops the OTG DMA's AXI writes/reads instead of erroring,
   * exactly like Bug 2's I2C2 pins. Same root cause pattern, different
   * peripheral.
   */
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);

  /*
   * RIF_MASTER_INDEX_ETH1 and its whole associated GPIO security block
   * (originally: GPIOA_10/11, GPIOB_0/3/6/7/10/11, GPIOE_3/5/6 all marked
   * GPIO_PIN_SEC) were removed here.
   *
   * This project has no Ethernet driver code anywhere -- it's leftover
   * CubeMX codegen for a peripheral that isn't actually used. On this
   * package GPIOB_PIN_10/11 double as I2C2_SCL/I2C2_SDA (see
   * HAL_I2C_MspInit in stm32n6xx_hal_msp.c), and marking them SEC was
   * blocking the camera's I2C bus completely: I2C2 ACK'd nothing at any
   * address at all, not just the IMX219, even though
   * MX_I2C2_Init()/HAL_I2C_Init() reported success (RIF silently drops
   * disallowed writes rather than faulting, so the peripheral looked
   * configured in software but its registers never actually took effect).
   * Reference project Camera_N6_AI_Test, confirmed working on this same
   * hardware, has no GPIO RIF calls and no ETH1 master at all -- its
   * Security_Config() only touches RIMC for DCMIPP/OTG1 and RISC slave
   * attributes for CSI/DCMIPP/OTG1HS/JPEG. Mirrored that here instead of
   * guessing pin-by-pin.
   */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* USER CODE BEGIN RIF_Init 1 */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  /* Bug 10 (see WORKLOG.md) -- USB OTG1 HS and the HW JPEG encoder, both
   * now in active use by the UVC pipeline, need the same RISC slave
   * attribute the camera peripherals already had, or RIF silently drops
   * their AXI DMA transactions. */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_JPEG,   RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  /* USER CODE END RIF_Init 1 */
  /* USER CODE BEGIN RIF_Init 2 */

  /* USER CODE END RIF_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
PUTCHAR_PROTOTYPE
{
HAL_UART_Transmit(&hlpuart1, (uint8_t *)ch, 1, 0xFFFF);
return ch;
}
int _write(int fd, char * ptr, int len){
HAL_UART_Transmit(&hlpuart1, (uint8_t *) ptr, len, HAL_MAX_DELAY);
return len;
}

/* Set by app_threadx.c's capture thread once continuous UVC capture has
 * started; forwards frame-complete events to its double-buffer swap logic.
 * See app_threadx.c's file header for why this is a separate hook function
 * rather than a second definition of this HAL callback (this callback is
 * also used, unmodified below, by the single-shot warmup/verification
 * above, which must not be touched). */
extern volatile uint8_t uvc_capture_active;
extern void Capture_OnFrameComplete(DCMIPP_HandleTypeDef *hdcmipp);

void HAL_DCMIPP_PIPE_FrameEventCallback(
    DCMIPP_HandleTypeDef *hdcmipp,
    uint32_t Pipe)
{
    if (Pipe == DCMIPP_PIPE1)
    {
        frame_count++;
        frame_received = 1U;

        if (uvc_capture_active)
        {
            Capture_OnFrameComplete(hdcmipp);
        }
    }
}

static void I2C_ScanBus(I2C_HandleTypeDef *hi2c)
{
    uint32_t found = 0U;
    uint8_t  ready[128] = {0};

    printf("\r\n");
    printf("========== I2C2 BUS SCAN (all 0x00-0x7F) ==========\r\n");

    /* Probe every possible 7-bit address, including the reserved
     * 0x00-0x02 / 0x78-0x7F range -- listed for completeness even though
     * real devices won't answer there. HAL wants the 8-bit form (addr<<1). */
    for (uint16_t addr7 = 0x00U; addr7 <= 0x7FU; addr7++)
    {
        if (HAL_I2C_IsDeviceReady(hi2c,
                                   (uint16_t)(addr7 << 1),
                                   2U,
                                   5U) == HAL_OK)
        {
            ready[addr7] = 1U;
            found++;
        }
    }

    /* Classic i2cdetect-style grid: one row per 0x_0 address, 16 columns. */
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (uint16_t row = 0U; row < 0x80U; row += 16U)
    {
        printf("%02x: ", (unsigned)row);
        for (uint16_t col = 0U; col < 16U; col++)
        {
            uint16_t addr7 = row + col;

            if (ready[addr7])
            {
                printf("%02x ", (unsigned)addr7);
            }
            else
            {
                printf("-- ");
            }
        }
        printf("\r\n");
    }

    printf("\r\n");
    if (found == 0U)
    {
        printf("No devices found at all. Check camera cable/power/reset wiring.\r\n");
    }
    else
    {
        printf("Found %lu device(s) total.\r\n", (unsigned long)found);
    }
    printf("Expected IMX219 address = 0x%02X (7-bit: 0x%02X) -> %s\r\n",
           (unsigned)IMX219_I2C_ADDR,
           (unsigned)(IMX219_I2C_ADDR >> 1),
           ready[IMX219_I2C_ADDR >> 1] ? "PRESENT" : "NOT FOUND");
    printf("====================================================\r\n");
    printf("\r\n");
}

static void Camera_CheckFrameBuffer(void)
{
    uint32_t non_zero_count = 0U;
    uint32_t checksum = 0U;

    for (uint32_t i = 0U;
         i < FRAME_BUFFER_SIZE;
         i++)
    {
        if (camera_framebuffer[i] != 0U)
        {
            non_zero_count++;
        }

        checksum += camera_framebuffer[i];
    }

    printf("\r\n");
    printf("========== FRAME BUFFER ==========\r\n");

    printf("Address       = 0x%08lX\r\n",
           (uint32_t)camera_framebuffer);

    printf("Size          = %lu bytes\r\n",
           (uint32_t)FRAME_BUFFER_SIZE);

    printf("Width         = %lu\r\n",
           (uint32_t)FRAME_WIDTH);

    printf("Height        = %lu\r\n",
           (uint32_t)FRAME_HEIGHT);

    printf("Pitch         = %lu bytes\r\n",
           (uint32_t)(FRAME_WIDTH * FRAME_BPP));

    printf("Non-zero bytes = %lu / %lu (%lu%%)\r\n",
           non_zero_count,
           (uint32_t)FRAME_BUFFER_SIZE,
           (uint32_t)((uint64_t)non_zero_count * 100U / FRAME_BUFFER_SIZE));

    printf("Checksum       = 0x%08lX\r\n",
           checksum);

    printf("FRAME COMPLETE: %s\r\n",
           (non_zero_count == (uint32_t)FRAME_BUFFER_SIZE) ? "YES" : "NO (truncated)");

    printf("First 64 bytes:\r\n");

    for (uint32_t i = 0U; i < 64U; i++)
    {
        printf("%02X ", camera_framebuffer[i]);

        if (((i + 1U) % 16U) == 0U)
        {
            printf("\r\n");
        }
    }
//    for(int i=0;i<40;i+=5)
//    {
//        printf("%02X %02X %02X %02X %02X\r\n",
//            camera_framebuffer[i],
//            camera_framebuffer[i+1],
//            camera_framebuffer[i+2],
//            camera_framebuffer[i+3],
//            camera_framebuffer[i+4]);
//    }

    printf("==================================\r\n");
}

void HAL_DCMIPP_ErrorCallback(DCMIPP_HandleTypeDef *hdcmipp)
{
	printf("DCMIPP ERROR!\r\n");
    printf("DCMIPP Error = 0x%08lX\r\n", hdcmipp->ErrorCode);

    csi_sr0  = CSI->SR0;
    csi_sr1  = CSI->SR1;
    csi_ier0 = CSI->IER0;
    csi_ier1 = CSI->IER1;

    printf("CSI SR0  = 0x%08lX\r\n", csi_sr0);
    printf("CSI SR1  = 0x%08lX\r\n", csi_sr1);
    printf("CSI IER0 = 0x%08lX\r\n", csi_ier0);
    printf("CSI IER1 = 0x%08lX\r\n", csi_ier1);

    printf("ACTIVE0  = 0x%08lX\r\n",
           csi_sr0 & csi_ier0);

    printf("ACTIVE1  = 0x%08lX\r\n",
           csi_sr1 & csi_ier1);
}

static ISP_StatusTypeDef GetSensorInfoHelper(uint32_t Instance,
                                             ISP_SensorInfoTypeDef *Info)
{
    UNUSED(Instance);

    memset(Info,0,sizeof(*Info));

    strcpy(Info->name,"IMX219");

    Info->bayer_pattern = ISP_DEMOS_TYPE_RGGB;
    Info->color_depth   = 10;

    /* Mode 640x480 */
    Info->width  = 640;
    Info->height = 480;

    /* Bug 22 (see WORKLOG.md): 255 is past the IMX219's own documented
     * analog gain ceiling -- ../Camera_N6_AI_Test's comment on this same
     * register: "Range: 0x00=1x ... 0xC0=4x ... 0xE0=8x ... 0xE8=16x(max)".
     * Telling AEC it can go up to 255 let it drive ANALOG_GAIN past the
     * sensor's valid range (confirmed on hardware: AEC pinned isp_gain at
     * 255, isp_exposure at the 3522 ceiling, and the image is still dark --
     * writes above 0xE8 are out of spec and not doing anything useful).
     * 0xE8 = 232 is the real ceiling. */
    Info->gain_min = 0;
    Info->gain_max = 232;

    Info->exposure_min = 1;
    /* Bug 20 (see WORKLOG.md): this was 1762 (0x06E3-1), matching the
     * FRM_LENGTH_LINES value (0x06E3=1763) imx219.c had BEFORE Bug 18
     * restored it to the correct 0x0DC6=3526 (../Camera_N6_AI_Test's
     * confirmed-working value) -- this constant was never updated to match,
     * so evision's AEC has been told the sensor's exposure ceiling is
     * roughly HALF what it actually is ever since Bug 18's fix. Found while
     * investigating a "flashes bright for ~1s then crushes to near-black"
     * symptom after first enabling AECAlgo. Matches Camera_N6_AI_Test's own
     * COARSE_INTEGRATION_TIME margin ("max - 4 lines"). */
    Info->exposure_max = 3522;      // FrameLength(3526) - 4

    return ISP_OK;
}

static ISP_StatusTypeDef SetSensorGainHelper(uint32_t Instance, int32_t Gain)
{
  UNUSED(Instance);
  isp_gain = Gain;
  return (ISP_StatusTypeDef) IMX219_SetGain(&imx219_ctx,Gain);
}

static ISP_StatusTypeDef GetSensorGainHelper(uint32_t Instance, int32_t *Gain)
{
  UNUSED(Instance);
  *Gain = isp_gain;
  return ISP_OK;
}

static ISP_StatusTypeDef SetSensorExposureHelper(uint32_t Instance, int32_t Exposure)
{
  UNUSED(Instance);
  isp_exposure = Exposure;
  return (ISP_StatusTypeDef) IMX219_SetExposure(&imx219_ctx, Exposure);
}

static ISP_StatusTypeDef GetSensorExposureHelper(uint32_t Instance, int32_t *Exposure)
{
  UNUSED(Instance);
  *Exposure = isp_exposure;
  return ISP_OK;
}

/**
 * @brief  Vsync Event callback on pipe
 * @param  hdcmipp DCMIPP device handle
 *         Pipe    Pipe receiving the callback
 * @retval None
 */
void HAL_DCMIPP_PIPE_VsyncEventCallback(DCMIPP_HandleTypeDef *hdcmipp, uint32_t Pipe)
{
  UNUSED(hdcmipp);
  /* Update the frame counter and call the ISP statistics handler */
  switch (Pipe)
  {
    case DCMIPP_PIPE0 :
      ISP_IncDumpFrameId(&hcamera_isp);
      break;
    case DCMIPP_PIPE1 :
      ISP_IncMainFrameId(&hcamera_isp);
      ISP_GatherStatistics(&hcamera_isp);
      break;
    case DCMIPP_PIPE2 :
      ISP_IncAncillaryFrameId(&hcamera_isp);
      break;
  }
}

/**
  * @brief  USB1_OTG_HS PCD Initialization Function (ported from
  *         Camera_N6_AI_Test/Appli/Src/main.c -- FIFO sizing and init
  *         sequence copied as-is; called from app_usbx_device.c's device
  *         thread after ThreadX is running).
  * @retval None
  */
void MX_USB1_OTG_HS_PCD_Init(void)
{
  hpcd_USB_OTG_HS1.Instance = USB1_OTG_HS;
  hpcd_USB_OTG_HS1.Init.dev_endpoints = 9;
  hpcd_USB_OTG_HS1.Init.speed = PCD_SPEED_HIGH;
  hpcd_USB_OTG_HS1.Init.phy_itface = USB_OTG_HS_EMBEDDED_PHY;
  hpcd_USB_OTG_HS1.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.use_dedicated_ep1 = DISABLE;
  hpcd_USB_OTG_HS1.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.dma_enable = ENABLE;
#ifdef USB_OTG_HS_EXTERNAL_VBUS_SUPPORT
  hpcd_USB_OTG_HS1.Init.use_external_vbus = DISABLE;
#endif

  printf("[USB] Calling HAL_PCD_Init()...\r\n");
  if (HAL_PCD_Init(&hpcd_USB_OTG_HS1) != HAL_OK)
  {
    printf("[ERROR] HAL_PCD_Init failed\r\n");
    Error_Handler();
  }
  printf("[USB] HAL_PCD_Init successful\r\n");

  /* Configure FIFO AFTER HAL_PCD_Init() so USB clocks/core are ready. */
  if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_HS1, 0x100) != HAL_OK)
  {
    printf("[ERROR] HAL_PCDEx_SetRxFiFo failed\r\n");
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 0, 0x10) != HAL_OK)
  {
    printf("[ERROR] HAL_PCDEx_SetTxFiFo EP0 failed\r\n");
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 1, 0x10) != HAL_OK)
  {
    printf("[ERROR] HAL_PCDEx_SetTxFiFo EP1 failed\r\n");
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 2, 0x80) != HAL_OK)
  {
    printf("[ERROR] HAL_PCDEx_SetTxFiFo EP2 failed\r\n");
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 4, 0x100) != HAL_OK)
  {
    printf("[ERROR] HAL_PCDEx_SetTxFiFo EP4 failed\r\n");
    Error_Handler();
  }
  printf("[USB] FIFO configured: RX=256w, EP0=64B, EP1=64B, EP2=512B, EP4=1024B\r\n");
}

/**
  * @brief  Period elapsed callback in non blocking mode.
  * @note   ThreadX's tx_initialize_low_level.S claims SysTick for its own
  *         RTOS tick once tx_kernel_enter() runs, so HAL's time base is
  *         moved to TIM6 instead (see stm32n6xx_hal_timebase_tim.c, ported
  *         from Camera_N6_AI_Test, which overrides the weak
  *         HAL_InitTick()/HAL_SuspendTick()/HAL_ResumeTick()). Because that
  *         override is linked in from HAL_Init() onward, TIM6 is actually
  *         the tick source for the whole program, pre-RTOS init included --
  *         this callback is what increments uwTick/HAL_GetTick() throughout,
  *         and HAL_Delay() in the existing pre-RTOS bring-up code keeps
  *         working exactly as before, just driven by TIM6 instead of
  *         SysTick.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}
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

#ifdef  USE_FULL_ASSERT
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
