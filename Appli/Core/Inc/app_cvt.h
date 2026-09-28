/**
 ******************************************************************************
 * @file    app_cvt.h
 * @brief   Pixel format conversion functions (LUT-based, BT.601)
 ******************************************************************************
 */
#ifndef APP_CVT_H
#define APP_CVT_H

#include <stdint.h>

/* Initialize internal LUT tables. Must be called once before any CVT function. */
void CVT_FormatInit(void);

/* Output: YUYV-packed YUV422 */
void CVT_FormatGreyToYuv422(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatArgbToYuv422(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatRgb565ToYuv422(uint8_t *p_dst, uint8_t *p_src, int width, int height);

/* Output: JPEG MCU-422 blocks (for STM32 JPEG HW encoder input) */
void CVT_FormatGreyToYuv422Jpeg(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatRgbArgbToYuv422Jpeg(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatRgb888ToYuv422Jpeg(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatRgb565ToYuv422Jpeg(uint8_t *p_dst, uint8_t *p_src, int width, int height);
void CVT_FormatYuv422ToYuv422Jpeg(uint8_t *p_dst, uint8_t *p_src, int width, int height);

#endif /* APP_CVT_H */
