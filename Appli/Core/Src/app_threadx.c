/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @brief   ThreadX applicative file -- continuous UVC capture thread.
  *
  * NEW FILE for NUCLEO-N65X0Q-ISP (not a copy of Camera_N6_AI_Test's
  * app_threadx.c). That reference file re-does camera/sensor bring-up via
  * DCMIPP's own hardware Bayer2RGB + hand-tuned manual white-balance
  * multiplier constants -- exactly the color path this project is NOT using
  * (see main.c and knowledge_archive.md). This file only starts continuous
  * DCMIPP PIPE1 capture and pumps the existing, already-working ISP_MW/evision
  * middleware (real AWB/AE algorithms, not guessed constants) that main.c
  * already initialized (ISP_Init/ISP_Start) before MX_ThreadX_Init() was
  * ever called. Sensor/GPIO/I2C/RIF/clock init is NOT repeated here.
  *
  * Double-buffer (ping-pong) design mirrored faithfully from
  * Camera_N6_AI_Test/Appli/Src/app_threadx.c (a correct, non-trivial
  * race-condition fix, worth keeping identical):
  *   video_buf[0] and video_buf[1] alternate roles between DCMIPP DMA target
  *   and UVC read source, so DCMIPP DMA never overwrites a buffer that UVC
  *   (ux_device_video.c) is still transmitting.
  *
  * RAM-budget decision (this project only has ~2MB internal RAM, no PSRAM):
  *   video_buf[0] REUSES the exact same fixed-address memory main.c's
  *   single-shot capture used (`camera_framebuffer`, CAMERA_BUFFER_ADDR).
  *   That region is only touched by main.c's pre-RTOS single-shot
  *   verification, which has already completed and stopped PIPE1 by the
  *   time this thread runs -- reusing it instead of allocating a fresh
  *   614400-byte buffer saves 600KB, which the RTOS+USB+JPEG image needs.
  *   video_buf[1] is a newly allocated static RGB565 buffer of the same
  *   size (640x480x2 = 614400 bytes).
  *
  * DCache policy: mirrors the exact mechanism already confirmed working in
  * this project's own main.c single-shot path (SCB_InvalidateDCache_by_Addr
  * after DMA completes) rather than Camera_N6_AI_Test's ".noncacheable
  * section managed by MPU" comment -- that comment turned out to be
  * aspirational: grepping the whole reference project found no MPU_Config
  * call and no variable actually placed in its .noncacheable linker
  * section, and it never calls SCB_EnableDCache() at all (D-cache is simply
  * off there, hence no coherency problem to manage). This project's main.c
  * DOES enable D-cache (SCB_EnableDCache in main()), so it needs a real
  * invalidate, which this file does once per completed frame -- exactly the
  * pattern this project's own knowledge_archive.md documents as already
  * proven on real hardware.
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
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "imx219.h"
#include "app_jpg.h"
#include "isp_api.h"
#include "isp_core.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Reused from main.c: sensor/ISP context already initialized pre-RTOS. */
extern DCMIPP_HandleTypeDef hdcmipp;
extern ISP_HandleTypeDef    hcamera_isp;
extern IMX219_CTX_t         imx219_ctx;

/* Reused from main.c: fixed-address single-shot framebuffer (see header
 * comment above -- reused here as video_buf[0] to save 600KB of RAM). */
extern uint8_t *camera_framebuffer;

#define VIDEO_BUF_WIDTH  640U
#define VIDEO_BUF_HEIGHT 480U
#define VIDEO_BUF_SIZE   (VIDEO_BUF_WIDTH * VIDEO_BUF_HEIGHT * 2U)  /* RGB565, 2 B/px */

/*
 * Bug 14 (see WORKLOG.md) -- the real double-buffering design, after two
 * false starts:
 *
 *   Bug 8's original theory ("reactively calling
 *   HAL_DCMIPP_PIPE_SetMemoryAddress() from the frame-complete ISR stalls
 *   PIPE1 after 1 frame") was WRONG. What actually caused that symptom was
 *   Bug 9 (main.c's single-shot verification leaves the P1FCTCR_CPTMODE
 *   bit set to SNAPSHOT, and HAL_DCMIPP_CSI_PIPE_Start()'s `|= CaptureMode`
 *   never clears it) -- which was fixed independently and applies no
 *   matter which capture-start function is used. Bug 8's fix (drop to a
 *   single fixed buffer) treated the wrong symptom and left the pipe
 *   permanently unprotected against read/write overlap.
 *
 *   Bug 13's fix (DCMIPP's own hardware DBM double-buffer mode,
 *   HAL_DCMIPP_CSI_PIPE_DoubleBufferStart()) didn't fix the torn image
 *   either, and the reason is structural: DBM's hardware alternation runs
 *   completely independently of when the consumer (JPEG encoder) decides
 *   to start reading. `ready_buf_idx` only reflects "most recently
 *   completed buffer" -- by the time the encoder samples it, that buffer
 *   could have anywhere from ~0ms to ~32ms left before DCMIPP cycles back
 *   and starts overwriting it again (uniformly distributed, since the
 *   encoder's start time is not synchronized to DCMIPP's frame cadence).
 *   With a ~27ms encode time, that overlaps the majority of the time --
 *   DBM has no way for software to say "don't touch this one yet, I'm
 *   still reading it."
 *
 *   The actual fix needs real mutual exclusion, which requires software
 *   back in the loop: single-address continuous capture (not DBM) +
 *   reactively redirecting DCMIPP to the *other* buffer after each
 *   completed frame (HAL_DCMIPP_PIPE_SetMemoryAddress(), back to Bug 8's
 *   original mechanism) -- but now ALSO checking `uvc_locked_buf_idx`
 *   before redirecting: if the candidate buffer is the one UVC currently
 *   has locked for encoding, skip the redirect and let DCMIPP keep
 *   overwriting the buffer it just finished (dropping that frame) instead
 *   of touching the locked one. This is exactly Camera_N6_AI_Test's
 *   original app_threadx.c pattern -- correct all along, just needed Bug
 *   9's CPTMODE fix alongside it (which didn't exist yet when Bug 8 first
 *   ruled this approach out).
 */
static __attribute__((aligned(32))) uint8_t video_buf1[VIDEO_BUF_SIZE];
static uint8_t *video_buf[2];

/* Buffer index DCMIPP's DMA is currently targeting. */
static volatile uint8_t dcmipp_buf_idx = 0U;
/* Buffer index of the last fully captured frame -- safe for UVC to read. */
static volatile uint8_t ready_buf_idx  = 0U;
/* Buffer index UVC is currently streaming (0/1), or 0xFF when idle. Set by
 * ux_device_video.c's fill_uvc_payload() before it starts encoding a frame
 * and cleared after -- Capture_OnFrameComplete() below must NOT redirect
 * DCMIPP's DMA into this buffer while it's locked. */
volatile uint8_t uvc_locked_buf_idx = 0xFFU;

/* Set once this thread has started continuous PIPE1 capture; main.c's
 * pre-existing HAL_DCMIPP_PIPE_FrameEventCallback (used during the
 * single-shot warmup/verification) calls Capture_OnFrameComplete() only
 * when this is non-zero, so the two capture modes never fight over frame
 * events -- they're time-disjoint (warmup's PIPE1 is stopped before this
 * flag is ever set). */
volatile uint8_t uvc_capture_active = 0U;

static TX_SEMAPHORE  frame_ready_sem;
static TX_THREAD     capture_thread;
static __attribute__((aligned(32))) UCHAR capture_thread_stk[4096];
static volatile uint32_t uvc_frame_count = 0U;
static volatile uint32_t ovr_count       = 0U;
static volatile uint32_t axierr_count    = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void CaptureUVC_Thread(ULONG arg);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;

  /* USER CODE BEGIN App_ThreadX_Init */
  ret = tx_semaphore_create(&frame_ready_sem, "frame_rdy", 0);
  if (ret != TX_SUCCESS)
  {
    printf("[ERROR] tx_semaphore_create failed: %u\r\n", ret);
    return ret;
  }

  ret = tx_thread_create(&capture_thread,
                         "CaptureUVC",
                         CaptureUVC_Thread,
                         0,
                         capture_thread_stk,
                         sizeof(capture_thread_stk),
                         5, 5,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    printf("[ERROR] tx_thread_create (CaptureUVC) failed: %u\r\n", ret);
    return ret;
  }
  printf("[OK] CaptureUVC thread created (AUTO_START)\r\n");
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */
  printf("\r\n[RTOS] MX_ThreadX_Init entered, starting ThreadX kernel...\r\n");
  fflush(stdout);
  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */
  /* Unreachable unless the scheduler stops. */
  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/**
 * @brief  Capture thread: starts continuous DCMIPP PIPE1 capture into the
 *         double-buffer above, keeps the ISP_MW/evision AWB/AE middleware
 *         pumped (ISP_BackgroundProcess), initializes the RGB565->MJPEG
 *         path, and prints periodic fps/diagnostic info.
 *
 * Does NOT touch sensor/GPIO/I2C/RIF/clock init -- all of that already ran
 * in main.c before MX_ThreadX_Init()/tx_kernel_enter() was ever called.
 */
static void CaptureUVC_Thread(ULONG arg)
{
    (void)arg;

    printf("\r\n[UVC_CAP] Starting continuous capture thread...\r\n");

    /* video_buf[0] aliases main.c's single-shot camera_framebuffer (RAM
     * budget decision, see file header). video_buf[1] is the new buffer. */
    video_buf[0] = camera_framebuffer;
    video_buf[1] = video_buf1;

    /* ---- JPEG init: RGB565 source (this project's real ISP output),
     * NOT JPG_SRC_YUV422 like Camera_N6_AI_Test -- that is exactly the
     * hand-tuned-color path this project is deliberately avoiding.
     * Encode resolution 640x360 (see crop note in ux_device_video.c). */
    JPG_conf_t jpg_conf = { .width = 640, .height = 360, .fmt_src = JPG_SRC_RGB565 };
    if (JPG_Init(&jpg_conf) != 0)
        printf("[ERROR] JPG_Init failed\r\n");
    else
        printf("[OK]    JPEG encoder initialized (640x360 RGB565->YUV422 Q70)\r\n");

    /*
     * Bug 9 (see WORKLOG.md): HAL_DCMIPP_CSI_PIPE_Start()'s internal
     * DCMIPP_SetConfig() does `P1FCTCR |= CaptureMode` -- an OR, never a
     * clear. DCMIPP_MODE_CONTINUOUS is 0 and DCMIPP_MODE_SNAPSHOT is the
     * P1FCTCR_CPTMODE bit, so main.c's single-shot verification (which
     * starts PIPE1 in DCMIPP_MODE_SNAPSHOT right before this thread ever
     * runs) leaves that CPTMODE bit permanently set in hardware --
     * HAL_DCMIPP_CSI_PIPE_Stop() only clears CPTREQ/PIPEN, never CPTMODE.
     * Starting again with DCMIPP_MODE_CONTINUOUS (`|= 0`) therefore does
     * NOT clear it: the pipe silently stays in SNAPSHOT mode, captures
     * exactly one frame, and stops cleanly (no OVRF/AXI-error -- it isn't
     * malfunctioning, it's doing exactly what SNAPSHOT mode does). Matches
     * the real-hardware symptom exactly (frame_count stuck at 1 forever).
     * Fix: explicitly clear CPTMODE before starting continuous capture --
     * this HAL driver never does it for you.
     */
    CLEAR_BIT(DCMIPP->P1FCTCR, DCMIPP_P1FCTCR_CPTMODE);

    /* See Bug 14's file-header comment: single-address continuous capture,
     * with software-mediated ping-pong (Capture_OnFrameComplete() below)
     * instead of DCMIPP's own free-running DBM mode -- DBM can't express
     * "don't touch this buffer yet, the encoder is still reading it",
     * which is exactly what caused Bug 13's torn image to persist even
     * with real hardware double-buffering. */
    dcmipp_buf_idx = 0U;
    ready_buf_idx  = 0U;
    if (HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0,
                                   (uint32_t)video_buf[0],
                                   DCMIPP_MODE_CONTINUOUS) != HAL_OK)
    {
        printf("[ERROR] HAL_DCMIPP_CSI_PIPE_Start (continuous) failed\r\n");
    }
    else
    {
        printf("[OK]    DCMIPP PIPE1 continuous capture started (buf[0]=0x%08lX buf[1]=0x%08lX)\r\n",
               (unsigned long)(uint32_t)video_buf[0],
               (unsigned long)(uint32_t)video_buf[1]);
    }

    /* From here on, main.c's HAL_DCMIPP_PIPE_FrameEventCallback forwards
     * PIPE1 frame-complete events to Capture_OnFrameComplete() below. */
    uvc_capture_active = 1U;

    /* Sensor is already streaming continuously (started once by main.c
     * before the single-shot verification and never stopped) -- nothing
     * to restart on the sensor side. */

    printf("[OK]    USB UVC streaming enabled -- connect USB cable to view on PC\r\n");

    uint32_t last_tick  = (uint32_t)tx_time_get();
    uint32_t last_count = 0U;

    while (1)
    {
        if (tx_semaphore_get(&frame_ready_sem, TX_TIMER_TICKS_PER_SECOND) == TX_SUCCESS)
        {
            uvc_frame_count++;
            while (tx_semaphore_get(&frame_ready_sem, TX_NO_WAIT) == TX_SUCCESS)
                uvc_frame_count++;
        }

        /* Keep the real AWB/AE middleware converging -- this project's
         * ISP_MW/evision needs ISP_BackgroundProcess() pumped continuously,
         * exactly like main.c's own pre-RTOS warmup loop already does. */
        ISP_BackgroundProcess(&hcamera_isp);

        uint32_t now = (uint32_t)tx_time_get();
        if ((now - last_tick) >= 1000U)
        {
            uint32_t fps = uvc_frame_count - last_count;
            extern volatile uint32_t usb_irq_count;
            extern int32_t isp_gain, isp_exposure;
            printf("[UVC_CAP] fps=%lu frames=%lu ovr=%lu axierr=%lu usb_irq=%lu "
                   "isp_gain=%ld isp_exposure=%ld\r\n",
                   (unsigned long)fps, (unsigned long)uvc_frame_count,
                   (unsigned long)ovr_count, (unsigned long)axierr_count,
                   (unsigned long)usb_irq_count,
                   (long)isp_gain, (long)isp_exposure);

            /* Bug 15-19 diagnostics, no longer needed now that Bug 19's
             * IPPlug fix confirmed the striping fixed (see WORKLOG.md) --
             * flip to 1 to re-enable this register-level poll if similar
             * symptoms recur. */
#define DEBUG_DCMIPP_DIAG 0
#if DEBUG_DCMIPP_DIAG
            printf("[DCMIPP_DIAG] PipeState[1]=%u ErrorCode=0x%08lX P1SR=0x%08lX CMSR2=0x%08lX CMSR1=0x%08lX\r\n",
                   (unsigned)hdcmipp.PipeState[DCMIPP_PIPE1],
                   (unsigned long)hdcmipp.ErrorCode,
                   (unsigned long)DCMIPP->P1SR,
                   (unsigned long)DCMIPP->CMSR2,
                   (unsigned long)DCMIPP->CMSR1);
            printf("[DCMIPP_DIAG] CSI SR0=0x%08lX SR1=0x%08lX (clearing ErrorCode to catch fresh occurrences)\r\n",
                   (unsigned long)CSI->SR0, (unsigned long)CSI->SR1);
#endif
            hdcmipp.ErrorCode = HAL_DCMIPP_ERROR_NONE;

            last_count = uvc_frame_count;
            last_tick  = now;
        }

        /* Avoid starving lower-priority USBX/maintenance threads. */
        tx_thread_sleep(1);
    }
}

/**
 * @brief  Called from main.c's existing HAL_DCMIPP_PIPE_FrameEventCallback
 *         once continuous UVC capture is active (uvc_capture_active != 0).
 *         Does the double-buffer swap + DCache invalidate + semaphore give.
 *
 * Kept as a separate function (rather than redefining
 * HAL_DCMIPP_PIPE_FrameEventCallback here) because main.c already defines
 * that callback non-weak for its confirmed-working single-shot warmup and
 * verification path -- this project cannot have two definitions of the
 * same HAL callback, and that existing code must not be touched. main.c's
 * callback now also calls this hook; see the small addition there.
 */
void Capture_OnFrameComplete(DCMIPP_HandleTypeDef *hdcmipp_cb)
{
    /* Step 1: the buffer DCMIPP just finished writing is now ready for
     * CPU/UVC to read. See Bug 14's file-header comment for why this
     * software-mediated ping-pong (not DCMIPP's own DBM mode) is what
     * actually gives race-free double buffering on this hardware. */
    ready_buf_idx = dcmipp_buf_idx;

    /* Step 2: invalidate DCache once per completed frame (mirrors this
     * project's own confirmed-working single-shot pattern in main.c). */
    SCB_InvalidateDCache_by_Addr((uint32_t *)video_buf[ready_buf_idx], (int32_t)VIDEO_BUF_SIZE);

    /* Step 3: redirect DCMIPP to the other buffer, UNLESS UVC currently
     * has it locked for encoding -- in that case keep writing into the
     * buffer just completed (dropping this one frame) rather than
     * corrupt the buffer UVC is mid-read on. This mutual exclusion is
     * exactly what DCMIPP's own hardware DBM mode (Bug 13) cannot express
     * in software, and why that fix alone didn't stop the tearing. */
    uint8_t candidate = dcmipp_buf_idx ^ 1U;
    if (candidate != uvc_locked_buf_idx)
    {
        dcmipp_buf_idx = candidate;
        HAL_DCMIPP_PIPE_SetMemoryAddress(hdcmipp_cb, DCMIPP_PIPE1,
                                         DCMIPP_MEMORY_ADDRESS_0,
                                         (uint32_t)video_buf[dcmipp_buf_idx]);
    }
    /* else: UVC holds the candidate buffer locked -- drop this camera
     * frame. DCMIPP keeps writing to the same buffer (dcmipp_buf_idx
     * unchanged), which is safe: UVC only ever reads video_buf[ready_buf_idx]
     * (the OTHER one), never the one DCMIPP is currently targeting. */

    /* Step 4: signal a new frame is ready. */
    tx_semaphore_put(&frame_ready_sem);
}

/**
 * @brief  Pipe error callback -- fires on OVR (DCMIPP DMA write denied
 *         before the frame completed). Only meaningful once continuous
 *         capture is active; harmless no-op during main.c's own warmup
 *         (which has its own diagnostics already).
 */
void HAL_DCMIPP_PIPE_ErrorCallback(DCMIPP_HandleTypeDef *hdcmipp_cb, uint32_t Pipe)
{
    (void)hdcmipp_cb;
    (void)Pipe;
    if (uvc_capture_active)
        ovr_count++;
}

/**
 * @brief  Return pointer to the last fully-captured video frame.
 *         Used by UVC streaming code (ux_device_video.c). DCache has
 *         already been invalidated in Capture_OnFrameComplete().
 */
uint8_t *VIDEO_GetReadyBuffer(void)
{
    return video_buf[ready_buf_idx];
}

/**
 * @brief  Return the index (0 or 1) of the ready buffer.
 */
uint8_t VIDEO_GetReadyBufferIdx(void)
{
    return (uint8_t)ready_buf_idx;
}

/**
 * @brief  Return video frame buffer size in bytes (640x480x2 = 614400).
 */
uint32_t VIDEO_GetBufferSize(void)
{
    return VIDEO_BUF_SIZE;
}

/* USER CODE END 1 */
