/**
 ******************************************************************************
 * @file    app_jpg.c
 * @brief   JPEG hardware encoder — blocking poll mode (HAL_JPEG_Encode).
 *
 * Architecture:
 *   HAL_JPEG_Encode() is used — a fully blocking call that polls the JPEG
 *   input/output FIFOs internally (no interrupts, no DMA, no RTOS primitives).
 *   This matches the reference project (x-cube-n6-camera-capture) exactly.
 *
 *   Root cause of the previous IT-mode failure:
 *     HAL_JPEG_Encode_IT() requires HAL_JPEG_GetDataCallback to signal
 *     "no more input" when the buffer is exhausted.  The weak default is a NOP,
 *     so the HAL reset JpegInCount=0 and tried to re-feed the same buffer
 *     indefinitely — the hardware never reached EOC → EncodeCpltCallback
 *     never fired → volatile flag stayed 0 → timeout every call.
 *
 *   Root cause of the previous polling-mode enc=0B bug:
 *     JPG_Encode returned ctx->hjpeg.JpegOutCount, which is reset to 0 by
 *     JPEG_Process immediately after the final DataReadyCallback.  The correct
 *     value to return is ctx->jpg_encode_len (accumulated in the callback).
 *
 *   Call order:
 *     1. JPG_Init(&conf)  — configures JPEG HW (no IRQ needed)
 *     2. JPG_Encode()     — blocks per frame (640x360 Q70)
 ******************************************************************************
 */

#include "app_jpg.h"
#include "app_cvt.h"

#include <stdio.h>
#include "stm32n6xx_hal.h"

/* Functions marked RAMFUNC are copied from XIP flash to AXISRAM at boot.
 * They execute at CPU speed (600 MHz) instead of XIP speed (100 MHz).
 * noinline prevents GCC from inlining them back into XIP callers. */
#define RAMFUNC __attribute__((section(".RamFunc"), noinline))

/* JPEG output resolution (center-cropped 640x360 from 640x480 camera) */
#define JPEG_ENC_W   640
#define JPEG_ENC_H   360

/* MCU buffer: 640×360 YUV422 in JPEG MCU layout.
 * Size = 640*360*2 = 460800 bytes.  Kept in SRAM (no PSRAM on target board). */
static __attribute__((aligned(32))) uint8_t mcu_buffer[JPEG_ENC_W * JPEG_ENC_H * 2];

typedef struct {
  JPEG_HandleTypeDef hjpeg;
  JPG_conf_t         conf;
  uint32_t           jpg_encode_len;  /* accumulated JPEG output bytes */
} jpg_ctx_t;

static jpg_ctx_t jpg_ctx;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */


int JPG_Init(JPG_conf_t *conf)
{
  JPEG_ConfTypeDef jpeg_conf = { 0 };
  jpg_ctx_t *ctx = &jpg_ctx;

  CVT_FormatInit();

  ctx->hjpeg.Instance = JPEG;
  if (HAL_JPEG_Init(&ctx->hjpeg) != HAL_OK)
    return -1;

  jpeg_conf.ColorSpace        = JPEG_YCBCR_COLORSPACE;
  jpeg_conf.ChromaSubsampling = JPEG_422_SUBSAMPLING;
  jpeg_conf.ImageWidth        = conf->width;
  jpeg_conf.ImageHeight       = conf->height;
  jpeg_conf.ImageQuality      = 70;  /* Q70: ~15 KB/frame → ~15 payloads → 30+ fps */

  if (HAL_JPEG_ConfigEncoding(&ctx->hjpeg, &jpeg_conf) != HAL_OK)
    return -1;

  ctx->conf = *conf;

  printf("[UVC] JPEG HW encoder initialized (%dx%d YUV422 Q70 - POLLING mode)\r\n",
         conf->width, conf->height);
  return 0;
}

void JPG_Deinit(void)
{
  HAL_JPEG_DeInit(&jpg_ctx.hjpeg);
}

/* Timing counters — written by JPG_Encode, read by caller for diagnostics.
 * Uses DWT cycle counter (600 MHz) for accurate sub-ms measurement. */
volatile uint32_t jpg_cvt_us;
volatile uint32_t jpg_hal_us;

RAMFUNC int JPG_Encode(uint8_t *p_dst, uint8_t *p_src, int dst_size, int src_size)
{
  jpg_ctx_t *ctx = &jpg_ctx;
  HAL_StatusTypeDef ret;
  (void)src_size;

  /* Enable DWT cycle counter if not already running */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t c0 = DWT->CYCCNT;

  /* Convert source frame to JPEG MCU-422 layout */
  switch (ctx->conf.fmt_src)
  {
    case JPG_SRC_YUV422:
      CVT_FormatYuv422ToYuv422Jpeg(mcu_buffer, p_src,
                                   ctx->conf.width, ctx->conf.height);
      break;
    case JPG_SRC_RGB888:
      CVT_FormatRgb888ToYuv422Jpeg(mcu_buffer, p_src,
                                   ctx->conf.width, ctx->conf.height);
      break;
    case JPG_SRC_RGB565:
      /* RGB565 is the format this project's ISP_MW/evision pipeline actually
       * outputs (PIPE1 DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1) -- proper AWB/AE
       * middleware, not n6-ai-test's hand-tuned DCMIPP Bayer2RGB + manual WB
       * multipliers. CVT_FormatRgb565ToYuv422Jpeg() already exists in app_cvt.c
       * (it was unused there because that project's pipeline never produced
       * RGB565); wiring it up here is the only change needed. */
      CVT_FormatRgb565ToYuv422Jpeg(mcu_buffer, p_src,
                                   ctx->conf.width, ctx->conf.height);
      break;
    default:
      return -1;
  }

  uint32_t c1 = DWT->CYCCNT;

  /* Reset accumulated output length */
  ctx->jpg_encode_len = 0U;

  /* Blocking polling encode. Timeout is in HAL ms ticks. */
  ret = HAL_JPEG_Encode(&ctx->hjpeg,
                        mcu_buffer, (uint32_t)sizeof(mcu_buffer),
                        p_dst,      (uint32_t)dst_size,
                        1000U);

  uint32_t c2 = DWT->CYCCNT;
  /* Convert cycles to microseconds: cycles / (CPU_MHz) */
  jpg_cvt_us = (c1 - c0) / 600U;
  jpg_hal_us = (c2 - c1) / 600U;

  if (ret != HAL_OK)
  {
    printf("[JPG] WARN: HAL_JPEG_Encode failed (ret=%d, out=%lu cvt=%luus hal=%luus)\r\n",
           (int)ret, (unsigned long)ctx->jpg_encode_len,
           (unsigned long)jpg_cvt_us, (unsigned long)jpg_hal_us);
    return -1;
  }

  return (int)ctx->jpg_encode_len;
}

/* -------------------------------------------------------------------------
 * RAMFUNC override of HAL_GetTick (weak in stm32n6xx_hal.c).
 * Called ~9600 times per frame inside HAL_JPEG_Encode polling loop.
 * Default is in XIP flash — override keeps entire encode in SRAM.
 * ---------------------------------------------------------------------- */
extern __IO uint32_t uwTick;
RAMFUNC uint32_t HAL_GetTick(void)
{
  return uwTick;
}

/* -------------------------------------------------------------------------
 * HAL callback used by HAL_JPEG_Encode() to report output chunks
 * ---------------------------------------------------------------------- */

/**
 * @brief  Called each time output data is ready in the JPEG output FIFO.
 *         Accumulate partial chunk lengths to get total encoded size.
 */
RAMFUNC void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg,
                                uint8_t *pDataOut, uint32_t OutDataLength)
{
  (void)hjpeg;
  (void)pDataOut;
  jpg_ctx.jpg_encode_len += OutDataLength;
}

/**
 * @brief  HAL asks for a new input chunk after current one is consumed.
 *         For single-buffer frame encode, explicitly signal end-of-input.
 */
RAMFUNC void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
  (void)NbDecodedData;
  HAL_JPEG_ConfigInputBuffer(hjpeg, NULL, 0U);
}

/* -------------------------------------------------------------------------
 * HAL MSP (clock gating)
 * ---------------------------------------------------------------------- */

void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{
  (void)hjpeg;
  __HAL_RCC_JPEG_CLK_ENABLE();
}

void HAL_JPEG_MspDeInit(JPEG_HandleTypeDef *hjpeg)
{
  (void)hjpeg;
  __HAL_RCC_JPEG_CLK_DISABLE();
}

/* -------------------------------------------------------------------------
 * IRQ entry point — called from JPEG_IRQHandler in stm32n6xx_it.c
 * ---------------------------------------------------------------------- */

RAMFUNC void JPG_PeriphIRQHandler(void)
{
  /* Polling mode: JPEG IRQ is unused. Keep stub for existing IRQ wiring. */
}
