/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_device.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
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
#include "app_usbx_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
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

static ULONG cdc_acm_interface_number;
static ULONG cdc_acm_configuration_number;
static ULONG video_interface_number;
static ULONG video_configuration_number;
static UX_SLAVE_CLASS_CDC_ACM_PARAMETER cdc_acm_parameter;
static UX_DEVICE_CLASS_VIDEO_PARAMETER video_parameter;
static UX_DEVICE_CLASS_VIDEO_STREAM_PARAMETER video_stream_parameter[USBD_VIDEO_STREAM_NMNBER];
static TX_THREAD ux_device_app_thread;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_device_thread_entry(ULONG thread_input);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application USBX Device Initialization.
  * @param  memory_ptr: memory pointer
  * @retval status
  */

UINT MX_USBX_Device_Init(VOID *memory_ptr)
{
   UINT ret = UX_SUCCESS;
  UCHAR *device_framework_high_speed;
  UCHAR *device_framework_full_speed;
  ULONG device_framework_hs_length;
  ULONG device_framework_fs_length;
  ULONG string_framework_length;
  ULONG language_id_framework_length;
  UCHAR *string_framework;
  UCHAR *language_id_framework;

  UCHAR *pointer;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_USBX_Device_Init0 */

  /* USER CODE END MX_USBX_Device_Init0 */

  /* Get Device Framework High Speed and get the length */
  printf("[USBX_DEV] Building device framework...\r\n");
  device_framework_high_speed = USBD_Get_Device_Framework_Speed(USBD_HIGH_SPEED,
                                                                &device_framework_hs_length);
  printf("[USBX_DEV] HS framework: length=%lu\r\n", device_framework_hs_length);

  /* Get Device Framework Full Speed and get the length */
  device_framework_full_speed = USBD_Get_Device_Framework_Speed(USBD_FULL_SPEED,
                                                                &device_framework_fs_length);
  printf("[USBX_DEV] FS framework: length=%lu\r\n", device_framework_fs_length);

  /* Get String Framework and get the length */
  string_framework = USBD_Get_String_Framework(&string_framework_length);
  printf("[USBX_DEV] String framework: length=%lu\r\n", string_framework_length);

  /* Get Language Id Framework and get the length */
  language_id_framework = USBD_Get_Language_Id_Framework(&language_id_framework_length);
  printf("[USBX_DEV] Language framework: length=%lu\r\n", language_id_framework_length);

  /* Install the device portion of USBX */
  printf("[USBX_DEV] Initializing USBX device stack...\r\n");
  if (ux_device_stack_initialize(device_framework_high_speed,
                                 device_framework_hs_length,
                                 device_framework_full_speed,
                                 device_framework_fs_length,
                                 string_framework,
                                 string_framework_length,
                                 language_id_framework,
                                 language_id_framework_length,
                                 UX_NULL) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_DEVICE_INITIALIZE_ERROR */
    printf("[ERROR] ux_device_stack_initialize failed\r\n");
    return UX_ERROR;
    /* USER CODE END USBX_DEVICE_INITIALIZE_ERROR */
  }

  /* Initialize the cdc acm class parameters for the device */
  cdc_acm_parameter.ux_slave_class_cdc_acm_instance_activate   = USBD_CDC_ACM_Activate;
  cdc_acm_parameter.ux_slave_class_cdc_acm_instance_deactivate = USBD_CDC_ACM_Deactivate;
  cdc_acm_parameter.ux_slave_class_cdc_acm_parameter_change    = USBD_CDC_ACM_ParameterChange;

  /* USER CODE BEGIN CDC_ACM_PARAMETER */

  /* USER CODE END CDC_ACM_PARAMETER */

  /* Get cdc acm configuration number */
  cdc_acm_configuration_number = USBD_Get_Configuration_Number(CLASS_TYPE_CDC_ACM, 0);

  /* Find cdc acm interface number */
  cdc_acm_interface_number = USBD_Get_Interface_Number(CLASS_TYPE_CDC_ACM, 0);

  /* Initialize the device cdc acm class */
  printf("[USBX_DEV] Registering CDC ACM class: cfg=%u, itf=%u\r\n",
         cdc_acm_configuration_number, cdc_acm_interface_number);
  if (ux_device_stack_class_register(_ux_system_slave_class_cdc_acm_name,
                                     ux_device_class_cdc_acm_entry,
                                     cdc_acm_configuration_number,
                                     cdc_acm_interface_number,
                                     &cdc_acm_parameter) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_DEVICE_CDC_ACM_REGISTER_ERROR */
    printf("[ERROR] CDC ACM class register failed\r\n");
    return UX_ERROR;
    /* USER CODE END USBX_DEVICE_CDC_ACM_REGISTER_ERROR */
  }

  /* Initialize the video class parameters for the device */
  printf("[USBX_DEV] Initializing video stream parameters...\r\n");
  video_stream_parameter[0].ux_device_class_video_stream_parameter_callbacks.ux_device_class_video_stream_change
    = USBD_VIDEO_StreamChange;

  video_stream_parameter[0].ux_device_class_video_stream_parameter_callbacks.ux_device_class_video_stream_payload_done
    = USBD_VIDEO_StreamPayloadDone;

  video_stream_parameter[0].ux_device_class_video_stream_parameter_callbacks.ux_device_class_video_stream_request
    = USBD_VIDEO_StreamRequest;

  video_stream_parameter[0].ux_device_class_video_stream_parameter_max_payload_buffer_nb
    = USBD_VIDEO_PAYLOAD_BUFFER_NUMBER;
  printf("[USBX_DEV] Video stream buffer_nb=%u\r\n", USBD_VIDEO_PAYLOAD_BUFFER_NUMBER);

  video_stream_parameter[0].ux_device_class_video_stream_parameter_max_payload_buffer_size
    = USBD_VIDEO_StreamGetMaxPayloadBufferSize();
  printf("[USBX_DEV] Video stream max_payload_buffer_size=%lu\r\n",
         video_stream_parameter[0].ux_device_class_video_stream_parameter_max_payload_buffer_size);

  video_stream_parameter[0].ux_device_class_video_stream_parameter_thread_stack_size = 8192U;
  printf("[USBX_DEV] Video stream thread_stack_size=%lu\r\n",
         video_stream_parameter[0].ux_device_class_video_stream_parameter_thread_stack_size);

  video_stream_parameter[0].ux_device_class_video_stream_parameter_thread_entry
    = ux_device_class_video_write_thread_entry;

  /* Set the parameters for Video device */
  video_parameter.ux_device_class_video_parameter_streams_nb  = USBD_VIDEO_STREAM_NMNBER;
  video_parameter.ux_device_class_video_parameter_streams     = video_stream_parameter;

  video_parameter.ux_device_class_video_parameter_callbacks.ux_slave_class_video_instance_activate
    = USBD_VIDEO_Activate;

  video_parameter.ux_device_class_video_parameter_callbacks.ux_slave_class_video_instance_deactivate
    = USBD_VIDEO_Deactivate;

  printf("[USBX_DEV] Video parameter setup: streams_nb=%u, stream_ptr=%p\r\n",
         USBD_VIDEO_STREAM_NMNBER, (void*)video_stream_parameter);
  printf("[USBX_DEV] Stream[0] buffer_nb=%u, buffer_size=%lu, callbacks set\r\n",
         video_stream_parameter[0].ux_device_class_video_stream_parameter_max_payload_buffer_nb,
         video_stream_parameter[0].ux_device_class_video_stream_parameter_max_payload_buffer_size);

  /* USER CODE BEGIN VIDEO_PARAMETER */

  /* USER CODE END VIDEO_PARAMETER */

  /* Get video configuration number */
  video_configuration_number = USBD_Get_Configuration_Number(CLASS_TYPE_VIDEO, 0);
  printf("[USBX_DEV] VIDEO config number=%u\r\n", video_configuration_number);

  /* Find video interface number */
  video_interface_number = USBD_Get_Interface_Number(CLASS_TYPE_VIDEO, 0);
  printf("[USBX_DEV] VIDEO interface number=%u\r\n", video_interface_number);

  /* Initialize the device VIDEO */
  printf("[USBX_DEV] Registering VIDEO class: cfg=%u, itf=%u, param=%p\r\n",
         video_configuration_number, video_interface_number, (void*)&video_parameter);
  {
    UINT video_reg_status;
    video_reg_status = ux_device_stack_class_register(_ux_system_device_class_video_name,
                                                      ux_device_class_video_entry,
                                                      video_configuration_number,
                                                      video_interface_number,
                                                      (VOID *)&video_parameter);
    if (video_reg_status != UX_SUCCESS)
    {
      printf("[ERROR] VIDEO class register failed, status=%u\r\n", video_reg_status);
      if (video_reg_status == UX_MEMORY_INSUFFICIENT)
      {
        printf("[ERROR] VIDEO class register: UX_MEMORY_INSUFFICIENT\r\n");
      }
      return video_reg_status;
    }
  }
  printf("[USBX_DEV] VIDEO class registration OK\r\n");

  /* Allocate the stack for device application main thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, UX_DEVICE_APP_THREAD_STACK_SIZE,
                       TX_NO_WAIT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_ALLOCATE_STACK_ERROR */
    return TX_POOL_ERROR;
    /* USER CODE END MAIN_THREAD_ALLOCATE_STACK_ERROR */
  }

  /* Create the device application main thread */
  printf("[USBX_DEV] Creating app_ux_device_thread (priority=%u)...\r\n", UX_DEVICE_APP_THREAD_PRIO);
  if (tx_thread_create(&ux_device_app_thread, UX_DEVICE_APP_THREAD_NAME, app_ux_device_thread_entry,
                       0, pointer, UX_DEVICE_APP_THREAD_STACK_SIZE, UX_DEVICE_APP_THREAD_PRIO,
                       UX_DEVICE_APP_THREAD_PREEMPTION_THRESHOLD, UX_DEVICE_APP_THREAD_TIME_SLICE,
                       UX_DEVICE_APP_THREAD_START_OPTION) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_CREATE_ERROR */
    printf("[ERROR] app_ux_device_thread create failed\r\n");
    return TX_THREAD_ERROR;
    /* USER CODE END MAIN_THREAD_CREATE_ERROR */
  }

  /* USER CODE BEGIN MX_USBX_Device_Init1 */
  printf("[USBX_DEV] MX_USBX_Device_Init completed successfully\r\n");
  /* USER CODE END MX_USBX_Device_Init1 */

  return ret;
}

/**
  * @brief  Function implementing app_ux_device_thread_entry.
  * @param  thread_input: User thread input parameter.
  * @retval none
  */
static VOID app_ux_device_thread_entry(ULONG thread_input)
{
  /* USER CODE BEGIN app_ux_device_thread_entry */
  TX_PARAMETER_NOT_USED(thread_input);

  extern PCD_HandleTypeDef hpcd_USB_OTG_HS1;

  printf("[APP_UX_THREAD] app_ux_device_thread_entry starting...\r\n");
  
  /* Initialize PCD (calls HAL_PCD_MspInit → clocks + NVIC) */
  printf("[APP_UX_THREAD] Calling MX_USB1_OTG_HS_PCD_Init...\r\n");
  MX_USB1_OTG_HS_PCD_Init();

  /* FIFO already configured in MX_USB1_OTG_HS_PCD_Init() before HAL_PCD_Init()
   * Do NOT reconfigure here — it will override defaults */

   
  printf("[APP_UX_THREAD] Linking USBX DCD to HAL PCD...\r\n");
  ux_dcd_stm32_initialize((ULONG)USB1_OTG_HS, (ULONG)&hpcd_USB_OTG_HS1);

  /* Pull-up D+ to connect to host */
  printf("[APP_UX_THREAD] Calling HAL_PCD_Start() to enable USB PHY...\r\n");
  HAL_PCD_Start(&hpcd_USB_OTG_HS1);
  printf("[APP_UX_THREAD] HAL_PCD_Start successful - D+ pull-up active\r\n");

  printf("[USB] PCD init + DCD connect done - waiting for enumeration...\r\n");

  while (1)
  {
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
  }
  /* USER CODE END app_ux_device_thread_entry */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
