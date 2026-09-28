/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_video.c
  * @author  MCD Application Team
  * @brief   USBX Device Video applicative file
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
#include "ux_device_video.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ux_device_descriptors.h"
#include "app_jpg.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static UX_DEVICE_CLASS_VIDEO *video_instance_ptr;
static volatile int uvc_streaming;
static uint8_t  uvc_fid;          /* Frame ID bit — toggles each frame */
static uint32_t uvc_frame_offset; /* Byte offset into jpeg_out_buf being streamed */

/* MJPEG buffers */
static __attribute__((aligned(32))) uint8_t jpeg_out_buf[65536U];      /* JPEG output */
static int32_t jpeg_frame_len;    /* encoded JPEG size in bytes */

/* Debug counters — printed once per second */
static volatile uint32_t uvc_payload_count;
static volatile uint32_t uvc_frame_sent;
static volatile uint32_t uvc_drop_count;
static volatile uint32_t uvc_done_count;

static USBD_VideoControlTypeDef video_probe_control = {
  .bmHint = 0x0000U,
  .bFormatIndex = 0x01U,
  .bFrameIndex = 0x01U,
  .dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS),
  .wKeyFrameRate = 0x0000U,
  .wPFrameRate = 0x0000U,
  .wCompQuality = 0x0000U,
  .wCompWindowSize = 0x0000U,
  .wDelay = 0x0000U,
  .dwMaxVideoFrameSize = UVC_MAX_FRAME_SIZE,
  .dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS,
  .dwClockFrequency = 0x02DC6C00U,
  .bmFramingInfo = 0x00U,
  .bPreferedVersion = 0x00U,
  .bMinVersion = 0x00U,
  .bMaxVersion = 0x00U,
};

static USBD_VideoControlTypeDef video_commit_control = {
  .bmHint = 0x0000U,
  .bFormatIndex = 0x01U,
  .bFrameIndex = 0x01U,
  .dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS),
  .wKeyFrameRate = 0x0000U,
  .wPFrameRate = 0x0000U,
  .wCompQuality = 0x0000U,
  .wCompWindowSize = 0x0000U,
  .wDelay = 0x0000U,
  .dwMaxVideoFrameSize = UVC_MAX_FRAME_SIZE,
  .dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS,
  .dwClockFrequency = 0x02DC6C00U,
  .bmFramingInfo = 0x00U,
  .bPreferedVersion = 0x00U,
  .bMinVersion = 0x00U,
  .bMaxVersion = 0x00U,
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
/* Functions marked RAMFUNC are copied from XIP flash to AXISRAM at boot.
 * They execute at CPU speed (600 MHz) instead of XIP speed (100 MHz).
 * noinline prevents GCC from inlining them back into XIP callers. */
#define RAMFUNC __attribute__((section(".RamFunc"), noinline))

/* Camera double-buffer API — provided by app_threadx.c */
extern uint8_t  *VIDEO_GetReadyBuffer(void);
extern uint8_t   VIDEO_GetReadyBufferIdx(void);
extern volatile uint8_t uvc_locked_buf_idx;
static void fill_uvc_payload(UX_DEVICE_CLASS_VIDEO_STREAM *stream);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_VIDEO_Activate
  *         This function is called when insertion of a Video device.
  * @param  video_instance: Pointer to the video class instance.
  * @retval none
  */
VOID USBD_VIDEO_Activate(VOID *video_instance)
{
  /* USER CODE BEGIN USBD_VIDEO_Activate */
  video_instance_ptr = (UX_DEVICE_CLASS_VIDEO *)video_instance;
  printf("[UVC] Activated: speed=%s (%lu)\r\n",
         (_ux_system_slave->ux_system_slave_speed == UX_HIGH_SPEED_DEVICE) ? "HS" :
         (_ux_system_slave->ux_system_slave_speed == UX_FULL_SPEED_DEVICE) ? "FS" : "LS?",
         (unsigned long)_ux_system_slave->ux_system_slave_speed);
  /* USER CODE END USBD_VIDEO_Activate */

  return;
}

/**
  * @brief  USBD_VIDEO_Deactivate
  *         This function is called when extraction of a Video device.
  * @param  video_instance: Pointer to the video class instance.
  * @retval none
  */
VOID USBD_VIDEO_Deactivate(VOID *video_instance)
{
  /* USER CODE BEGIN USBD_VIDEO_Deactivate */
  video_instance_ptr = NULL;
  uvc_streaming = 0;
  printf("[UVC] Video class deactivated\r\n");
  /* USER CODE END USBD_VIDEO_Deactivate */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamChange
  *         This function is invoked to inform application that the
  *         alternate setting are changed.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  alternate_setting: interface alternate setting.
  * @retval none
  */
VOID USBD_VIDEO_StreamChange(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                             ULONG alternate_setting)
{
  /* USER CODE BEGIN USBD_VIDEO_StreamChange */
  if (alternate_setting == 1U)
  {
    uvc_streaming = 1;
    uvc_frame_offset = 0U;
    uvc_fid = 0U;
    jpeg_frame_len = 0;
    uvc_locked_buf_idx = 0xFFU;
    {
      ULONG _spd = _ux_system_slave->ux_system_slave_speed;
      ULONG _mps = (_spd == UX_HIGH_SPEED_DEVICE) ? USBD_VIDEO_EPIN_HS_MPS : USBD_VIDEO_EPIN_FS_MPS;
      printf("[UVC] Stream ON alt=%lu speed=%s(%lu) ep_mps=%lu\r\n",
             (unsigned long)alternate_setting,
             (_spd == UX_HIGH_SPEED_DEVICE) ? "HS" :
             (_spd == UX_FULL_SPEED_DEVICE) ? "FS" : "?",
             (unsigned long)_spd,
             (unsigned long)_mps);
    }

    /* Seed the USBX payload ring with UVC header-only (2-byte) payloads.
     * Calling fill_uvc_payload() here would block in the USBX device-control
     * thread (small stack). Real data flows from USBD_VIDEO_StreamPayloadDone. */
    for (ULONG i = 0U; i < USBD_VIDEO_PAYLOAD_BUFFER_NUMBER; i++)
    {
      UCHAR *seed_buf;
      ULONG  seed_len;
      if (ux_device_class_video_write_payload_get(video_stream, &seed_buf, &seed_len) == UX_SUCCESS
          && seed_len >= 2U)
      {
        seed_buf[0] = 2U;            /* bHeaderLength = 2 */
        seed_buf[1] = uvc_fid & 1U; /* bmHeaderInfo: FID only, no EOF */
        ux_device_class_video_write_payload_commit(video_stream, 2U);
      }
    }

    if (ux_device_class_video_transmission_start(video_stream) != UX_SUCCESS)
    {
      printf("[UVC] transmission_start failed\r\n");
      uvc_streaming = 0;
    }
    else
    {
      printf("[UVC] transmission_start OK\r\n");
    }
  }
  else
  {
    uvc_streaming = 0;
    uvc_locked_buf_idx = 0xFFU;
    printf("[UVC] Stream OFF (alt=%lu)\r\n", (unsigned long)alternate_setting);
  }
  /* USER CODE END USBD_VIDEO_StreamChange */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamPayloadDone
  *         This function is invoked when stream data payload received.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  length: transfer length.
  * @retval none
  */
VOID USBD_VIDEO_StreamPayloadDone(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                                  ULONG length)
{
  /* USER CODE BEGIN USBD_VIDEO_StreamPayloadDone */
  (void)length;

  /* Track worst-case gap between consecutive done callbacks.
   * USB HS bInterval=1 → expected gap ≤ 1ms.  FS → ≤ 8ms.
   * Values > 100ms mean USB ISO transfers are stalling. */
  static uint32_t _done_prev_tick = 0U;
  static uint32_t _done_max_gap   = 0U;
  static uint32_t _done_report_tick = 0U;
  uint32_t _done_now = (uint32_t)tx_time_get();
  uint32_t _done_gap = _done_now - _done_prev_tick;
  if (_done_gap > _done_max_gap) _done_max_gap = _done_gap;
  _done_prev_tick = _done_now;
  if ((_done_now - _done_report_tick) >= 5000U)
  {
    printf("[UVC] PayloadDone max_gap=%lums (HS<1ms FS<8ms; >100ms=stall)\r\n",
           (unsigned long)_done_max_gap);
    _done_max_gap     = 0U;
    _done_report_tick = _done_now;
  }

  uvc_done_count++;
  if (uvc_streaming)
    fill_uvc_payload(video_stream);
  /* USER CODE END USBD_VIDEO_StreamPayloadDone */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamRequest
  *         This function is invoked to manage the UVC class requests.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  transfer: Pointer to the transfer request.
  * @retval status
  */
UINT USBD_VIDEO_StreamRequest(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                              UX_SLAVE_TRANSFER *transfer)
{
   UINT status  = UX_SUCCESS;

  /* USER CODE BEGIN USBD_VIDEO_StreamRequest */
  UCHAR *data;
  UCHAR request;
  USHORT length;
  UCHAR control_selector;
  USBD_VideoControlTypeDef *control;
  ULONG payload_size;

  UX_PARAMETER_NOT_USED(video_stream);

  request = transfer->ux_slave_transfer_request_setup[UX_SETUP_REQUEST];
  control_selector = transfer->ux_slave_transfer_request_setup[UX_SETUP_VALUE + 1];
  length = ux_utility_short_get(transfer->ux_slave_transfer_request_setup + UX_SETUP_LENGTH);
  data = transfer->ux_slave_transfer_request_data_pointer;

  switch (control_selector)
  {
    case UX_DEVICE_CLASS_VIDEO_VS_PROBE_CONTROL:
      control = &video_probe_control;
      break;

    case UX_DEVICE_CLASS_VIDEO_VS_COMMIT_CONTROL:
      control = &video_commit_control;
      break;

    default:
      printf("[UVC] Unsupported CS=0x%02X req=0x%02X len=%u\r\n",
             control_selector, request, length);
      return UX_ERROR;
  }

  control->dwMaxVideoFrameSize = UVC_MAX_FRAME_SIZE;
  control->dwClockFrequency = 0x02DC6C00U;
  control->bFormatIndex = 0x01U;
  control->bFrameIndex = 0x01U;

  if (_ux_system_slave->ux_system_slave_speed == UX_FULL_SPEED_DEVICE)
  {
    control->dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS);
    control->dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS;
  }
  else
  {
    control->dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_HS);
    control->dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_HS_MPS;
  }

  payload_size = UX_MIN((ULONG)length, (ULONG)sizeof(USBD_VideoControlTypeDef));

  switch (request)
  {
    case UX_DEVICE_CLASS_VIDEO_SET_CUR:
      if (length >= 26U)
      {
        ux_utility_memory_copy((VOID *)control, data, payload_size);

        /* Keep device-owned fields coherent even if host proposes other values. */
        control->dwMaxVideoFrameSize = UVC_MAX_FRAME_SIZE;
        control->dwClockFrequency = 0x02DC6C00U;
        if (_ux_system_slave->ux_system_slave_speed == UX_FULL_SPEED_DEVICE)
        {
          control->dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS);
          control->dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS;
        }
        else
        {
          control->dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_HS);
          control->dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_HS_MPS;
        }
      }
      printf("[UVC] SET_CUR cs=0x%02X len=%u fmt=%u frame=%u interval=%lu payload=%lu\r\n",
             control_selector,
             length,
             control->bFormatIndex,
             control->bFrameIndex,
             (unsigned long)control->dwFrameInterval,
             (unsigned long)control->dwMaxPayloadTransferSize);
      status = UX_SUCCESS;
      break;

    case UX_DEVICE_CLASS_VIDEO_GET_DEF:
    case UX_DEVICE_CLASS_VIDEO_GET_CUR:
    case UX_DEVICE_CLASS_VIDEO_GET_MIN:
    case UX_DEVICE_CLASS_VIDEO_GET_MAX:
      ux_utility_memory_copy(data, (VOID *)control, payload_size);
      status = ux_device_stack_transfer_request(transfer, payload_size, payload_size);
      printf("[UVC] GET req=0x%02X cs=0x%02X len=%u reply=%lu interval=%lu payload=%lu\r\n",
             request,
             control_selector,
             length,
             (unsigned long)payload_size,
             (unsigned long)control->dwFrameInterval,
             (unsigned long)control->dwMaxPayloadTransferSize);
      break;

    default:
      printf("[UVC] Unsupported req=0x%02X cs=0x%02X len=%u\r\n",
             request, control_selector, length);
      status = UX_ERROR;
      break;
  }
  /* USER CODE END USBD_VIDEO_StreamRequest */

  return status;
}

/**
  * @brief  USBD_VIDEO_StreamGetMaxPayloadBufferSize
  *         Get video stream max payload buffer size.
  * @param  none
  * @retval max payload
  */
ULONG USBD_VIDEO_StreamGetMaxPayloadBufferSize(VOID)
{
  ULONG max_playload = 0U;

  /* USER CODE BEGIN USBD_VIDEO_StreamGetMaxPayloadBufferSize */
  /* +4 = USBX internal payload length overhead */
  max_playload = USBD_VIDEO_EPIN_HS_MPS + 4U;
  /* USER CODE END USBD_VIDEO_StreamGetMaxPayloadBufferSize */

  return max_playload;
}

/* USER CODE BEGIN 1 */

/**
 * @brief  Fill one USBX video payload buffer with MJPEG data.
 *
 * At the start of each UVC frame (uvc_frame_offset == 0):
 *   1. Lock the camera double-buffer (held for entire encode duration).
 *   2. video_buf is already the cropped 640x360 region (DCMIPP hardware
 *      crop, see main.c's MX_DCMIPP_Init() -- post-Bug-23, see WORKLOG.md).
 *   3. JPEG-encode directly from video_buf → jpeg_out_buf via HAL polling mode.
 *   4. Release the lock after encode.
 *
 * Subsequent calls stream chunks of jpeg_out_buf until EOF.
 *
 * Packet layout: [2-byte UVC header] [up to (MPS-2) JPEG bytes]
 * UVC header byte 0 = bHeaderLength = 2
 * UVC header byte 1 = bmHeaderInfo: bit0=FID, bit1=EOF
 */
static void fill_uvc_payload(UX_DEVICE_CLASS_VIDEO_STREAM *stream)
{
    UCHAR *buf;
    ULONG  buf_len;

    if (ux_device_class_video_write_payload_get(stream, &buf, &buf_len) != UX_SUCCESS)
    {
        uvc_drop_count++;
        return;
    }

    /* ---- Start of a new JPEG frame: encode now ---- */
    if (uvc_frame_offset == 0U)
    {
        /* Lock camera buffer for full encode duration (no copy buffer).
         * Post-Bug-23 (see WORKLOG.md): the 640x360 crop is now done by
         * DCMIPP's own hardware crop block (main.c's MX_DCMIPP_Init()) --
         * video_buf[0]/[1] ARE the cropped 640x360 region already, so no
         * "+60*640*2" row-skip is needed here anymore (that used to skip
         * past 60 rows of a full 640x480 capture; capturing those rows in
         * the first place was exactly the wasted RAM this change removes). */
        uvc_locked_buf_idx = VIDEO_GetReadyBufferIdx();
        uint8_t *src_frame = VIDEO_GetReadyBuffer();

        /* Bug 15-19 diagnostics, no longer needed now that Bug 19's IPPlug
         * fix confirmed the striping fixed (see WORKLOG.md) -- flip to 1 to
         * re-enable this throttled source-buffer dump if similar image
         * corruption symptoms recur. */
#define DEBUG_UVC_SRC 0
#if DEBUG_UVC_SRC
        {
            static uint32_t _src_dump_tick = 0U;
            uint32_t _now = (uint32_t)tx_time_get();
            if ((_now - _src_dump_tick) >= 2000U)
            {
                _src_dump_tick = _now;
                printf("[UVC_SRC] buf_idx=%u row60 first32B: ", uvc_locked_buf_idx);
                for (uint32_t _i = 0U; _i < 32U; _i++)
                    printf("%02X ", src_frame[_i]);
                printf("\r\n[UVC_SRC] row60+8 (MCU row1) first32B: ");
                for (uint32_t _i = 0U; _i < 32U; _i++)
                    printf("%02X ", src_frame[8U * 1280U + _i]);
                printf("\r\n[UVC_SRC] row60+16 (MCU row2) first32B: ");
                for (uint32_t _i = 0U; _i < 32U; _i++)
                    printf("%02X ", src_frame[16U * 1280U + _i]);
                printf("\r\n");
            }
        }
#endif /* DEBUG_UVC_SRC */

        uint32_t _t_enc0 = (uint32_t)tx_time_get();
        int enc = JPG_Encode(jpeg_out_buf, src_frame,
                             (int)sizeof(jpeg_out_buf), 640 * 360 * 2);
        uvc_locked_buf_idx = 0xFFU;  /* release lock after encode */
        uint32_t _t_enc1 = (uint32_t)tx_time_get();
        /* Throttled log: JPEG encode time (ThreadX ticks, typically near ms with 1kHz tick). */
        extern volatile uint32_t jpg_cvt_us, jpg_hal_us;
        static uint32_t _enc_log_tick = 0U;
        if ((_t_enc1 - _enc_log_tick) >= 1000U)
        {
            _enc_log_tick = _t_enc1;
            printf("[JPG] enc=%ldB t=%lums (cvt=%luus hal=%luus)\r\n",
                   (long)enc, (unsigned long)(_t_enc1 - _t_enc0),
                   (unsigned long)jpg_cvt_us, (unsigned long)jpg_hal_us);
        }
        if (enc <= 0)
        {
            /* Encode failed — send header-only EOF to keep USBX write thread alive */
            buf[0] = 2U;
            buf[1] = (uvc_fid & 1U) | 0x02U;  /* FID + EOF */
            ux_device_class_video_write_payload_commit(stream, 2U);
            uvc_fid ^= 1U;
            uvc_drop_count++;
            return;
        }
        jpeg_frame_len = enc;
    }

    /* ---- Stream the next chunk of jpeg_out_buf ---- */
    ULONG ep_mps = (_ux_system_slave->ux_system_slave_speed == UX_HIGH_SPEED_DEVICE)
                   ? USBD_VIDEO_EPIN_HS_MPS : USBD_VIDEO_EPIN_FS_MPS;
    uint32_t max_data = (ep_mps > 2U) ? (ep_mps - 2U) : 0U;
    if (buf_len >= 2U && max_data > (buf_len - 2U))
        max_data = buf_len - 2U;

    uint32_t remaining = (uint32_t)jpeg_frame_len - uvc_frame_offset;
    uint32_t chunk     = (remaining < max_data) ? remaining : max_data;
    uint8_t  eof       = (uvc_frame_offset + chunk >= (uint32_t)jpeg_frame_len) ? 1U : 0U;

    buf[0] = 2U;
    buf[1] = (uvc_fid & 1U) | (eof << 1U);
    memcpy(buf + 2U, jpeg_out_buf + uvc_frame_offset, chunk);
    uvc_frame_offset += chunk;

    ux_device_class_video_write_payload_commit(stream, 2U + chunk);
    uvc_payload_count++;

    if (eof)
    {
        uvc_frame_offset = 0U;
        uvc_fid ^= 1U;
        uvc_frame_sent++;

        static uint32_t _last_tick;
        static uint32_t _last_frames;
        static uint32_t _last_payloads;
        static uint32_t _last_drops;
        static uint32_t _last_done;
        uint32_t now = (uint32_t)tx_time_get();
        if ((now - _last_tick) >= 1000U)
        {
            uint32_t f = uvc_frame_sent    - _last_frames;
            uint32_t p = uvc_payload_count - _last_payloads;
            uint32_t d = uvc_drop_count    - _last_drops;
            uint32_t c = uvc_done_count    - _last_done;
            printf("[UVC] fps=%lu payloads=%lu done_cb=%lu drops=%lu"
                   " (jpeg_B=%ld ep_mps=%lu)\r\n",
                   (unsigned long)f, (unsigned long)p,
                   (unsigned long)c, (unsigned long)d,
                   (long)jpeg_frame_len, (unsigned long)ep_mps);
            _last_frames   = uvc_frame_sent;
            _last_payloads = uvc_payload_count;
            _last_drops    = uvc_drop_count;
            _last_done     = uvc_done_count;
            _last_tick     = now;
        }
    }
}

/* USER CODE END 1 */
