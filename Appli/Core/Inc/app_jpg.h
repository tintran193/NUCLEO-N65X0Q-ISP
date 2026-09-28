/**
 ******************************************************************************
 * @file    app_jpg.h
 * @brief   JPEG hardware encoder API (blocking polling mode).
 *          Adapted from x-cube-n6-camera-capture reference project.
 ******************************************************************************
 */
#ifndef APP_JPG_H
#define APP_JPG_H

#include <stdint.h>

#define JPG_SRC_YUV422  0
#define JPG_SRC_RGB888  1
#define JPG_SRC_RGB565  2

typedef struct {
  int width;
  int height;
  int fmt_src;  /* JPG_SRC_YUV422, JPG_SRC_RGB888 or JPG_SRC_RGB565 */
} JPG_conf_t;

/**
 * @brief  Initialize the JPEG peripheral and configure encoding parameters.
 *         Calls CVT_FormatInit() internally.
 *         MUST be called before JPG_Encode().
 *         No semaphore or IRQ setup needed.
 */
int  JPG_Init(JPG_conf_t *conf);

/**
 * @brief  Encode one frame to JPEG using HAL_JPEG_Encode() polling mode.
 * @param  p_dst      Output JPEG buffer.
 * @param  p_src      Input frame in the format specified by conf.fmt_src.
 * @param  dst_size   Output buffer capacity (bytes).
 * @param  src_size   Input buffer size (bytes, informational).
 * @retval Encoded JPEG size in bytes, or negative on error.
 */
int  JPG_Encode(uint8_t *p_dst, uint8_t *p_src, int dst_size, int src_size);

void JPG_Deinit(void);

/**
 * @brief  JPEG IRQ compatibility hook (unused in polling mode).
 */
void JPG_PeriphIRQHandler(void);

#endif /* APP_JPG_H */
