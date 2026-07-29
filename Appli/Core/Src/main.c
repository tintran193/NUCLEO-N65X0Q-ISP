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

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DCMIPP_HandleTypeDef hdcmipp;

I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef hlpuart1;

/* USER CODE BEGIN PV */
#define FRAME_WIDTH       640U
#define FRAME_HEIGHT      480U
#define FRAME_BPP         1U

#define FRAME_BUFFER_SIZE  (FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP)
#define CAMERA_BUFFER_ADDR 0x34200000U
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_I2C2_Init(void);
static void MX_DCMIPP_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
static void Camera_CheckFrameBuffer(void);
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
  MX_DCMIPP_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */
  // ==============Check Sensor==========================
  HAL_GPIO_WritePin(GPIOA,
                    GPIO_PIN_0,
                    GPIO_PIN_SET);

  HAL_GPIO_WritePin(GPIOO,
                    GPIO_PIN_5,
                    GPIO_PIN_RESET);

  HAL_Delay(10);

  HAL_GPIO_WritePin(GPIOO,
                    GPIO_PIN_5,
                    GPIO_PIN_SET);

  HAL_Delay(10);

//  HAL_StatusTypeDef status;
//
//  status = HAL_I2C_IsDeviceReady(&hi2c2,
//                                 IMX219_I2C_ADDR,
//                                 3,
//                                 100);
//
//  if (status == HAL_OK)
//  {
//      printf("IMX219 I2C OK\r\n");
//  }
//  else
//  {
//      printf("IMX219 I2C FAIL, status = %d\r\n", status);
//  }

  //=====================Check ID======================
//  if (IMX219_Init() != HAL_OK)
//  {
//      printf("IMX219 initialization failed\r\n");
//
//      Error_Handler();
//  }
//  else
//  {
//      printf("IMX219 initialization OK\r\n");
//  }
//
//  uint16_t camera_id;
//
//  if (IMX219_ReadID(&camera_id) == HAL_OK)
//  {
//      printf("IMX219 ID = 0x%04X\r\n",
//             camera_id);
//  }
  //IMX219_Init();
  IMX219_CTX_t imx219_ctx =
  {
      .handle = &hi2c2,
      .ReadReg = IMX219_I2C_ReadReg,
      .WriteReg = IMX219_I2C_WriteReg
  };

  uint16_t sensor_id;

  int32_t status;

  status =
      IMX219_ReadID(
          &imx219_ctx,
          &sensor_id
      );

  printf(
      "IMX219_ReadID status = %ld\r\n",
      status
  );

  printf(
      "IMX219 ID = 0x%04X\r\n",
      sensor_id
  );

  uint8_t value;

  value = 0x00;

  if (IMX219_WriteReg(
          &imx219_ctx,
          0x0100,
          &value,
          1
      ) != 0)
  {
      printf("Write failed\r\n");
  }
  else
  {
      printf(
          "Write OK: REG=0x0100 DATA=0x00\r\n"
      );
  }

  value = 0xFF;

  if (IMX219_ReadReg(
          &imx219_ctx,
          0x0100,
          &value,
          1
      ) != 0)
  {
      printf("Read failed\r\n");
  }
  else
  {
      printf(
          "Read OK: REG=0x0100 DATA=0x%02X\r\n",
          value
      );
  }

  if (IMX219_Init(&imx219_ctx) != 0)
  {
      printf(
          "IMX219 initialization FAILED\r\n"
      );
  }
  else
  {
      printf(
          "IMX219 initialization OK\r\n"
      );
  }
  /* ============================================================
   * Prepare frame buffer
   * ============================================================ */

  printf("\r\n");
  printf("Preparing frame buffer...\r\n");

  printf("Before memset\r\n");

  camera_framebuffer[0] = 0xAA;
  camera_framebuffer[1] = 0x55;

  printf("After write\r\n");
  /*
   * Clear the buffer before capture.
   *
   * This allows us to determine later whether DCMIPP
   * actually wrote data into it.
   */
  memset(
      camera_framebuffer,
      0x00,
      FRAME_BUFFER_SIZE
  );


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


  /* ============================================================
   * Start DCMIPP snapshot
   * ============================================================ */

  printf("DCMIPP: starting capture\r\n");

  if (HAL_DCMIPP_CSI_PIPE_Start(
          &hdcmipp,
          DCMIPP_PIPE0,
          DCMIPP_VIRTUAL_CHANNEL0,
          (uint32_t)camera_framebuffer,
          DCMIPP_MODE_SNAPSHOT) != HAL_OK)
  {
      printf("DCMIPP capture failed\r\n");

      Error_Handler();
  }

  printf("DCMIPP capture started\r\n");
  printf("Requested framebuffer address = 0x%08lX\r\n",
         (uint32_t)camera_framebuffer);
  printf("P0PPM0AR1 = 0x%08lX\r\n", DCMIPP->P0PPM0AR1);
  printf("P0FCTCR   = 0x%08lX\r\n", DCMIPP->P0FCTCR);
  printf("P0PPCR    = 0x%08lX\r\n", DCMIPP->P0PPCR);
  printf("P0PPM0AR1 = 0x%08lX\r\n", DCMIPP->P0PPM0AR1);
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

      if (HAL_DCMIPP_PIPE_GetDataCounter(
              &hdcmipp,
              DCMIPP_PIPE0,
              &data_counter
          ) == HAL_OK)
      {
          printf(
              "DCMIPP data counter = %lu\r\n",
              data_counter
          );
          //
          printf("P0SR = 0x%08lX\r\n", DCMIPP->P0SR); // Check OK
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

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	    HAL_Delay(200);
	    BSP_LED_Toggle(LED_RED);
	    HAL_Delay(200);
	    BSP_LED_Toggle(LED_GREEN);
//	    HAL_Delay(200);
//	    BSP_LED_Toggle(LED_BLUE);
	    HAL_Delay(200);
	    printf("ok\n");
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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
  pCSI_PipeConfig.DataTypeIDA = DCMIPP_DT_RAW8;
  pCSI_PipeConfig.DataTypeIDB = DCMIPP_DT_RAW8;
  if (HAL_DCMIPP_CSI_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE0, &pCSI_PipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  pCSI_Config.PHYBitrate = DCMIPP_CSI_PHY_BT_450;
  pCSI_Config.DataLaneMapping = DCMIPP_CSI_PHYSICAL_DATA_LANES;
  pCSI_Config.NumberOfLanes = DCMIPP_CSI_TWO_DATA_LANES;
  HAL_DCMIPP_CSI_SetConfig(&hdcmipp, &pCSI_Config);
  pPipeConfig.FrameRate = DCMIPP_FRAME_RATE_ALL;
  pPipeConfig.PixelPipePitch = 640;
  pPipeConfig.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_MONO_Y8_G8_1;
  if (HAL_DCMIPP_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE0, &pPipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DCMIPP_CSI_SetVCConfig(&hdcmipp, 0U, DCMIPP_CSI_DT_BPP8) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMIPP_Init 2 */

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
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_ETH1, &RIMC_master);

  /* RIF-Aware IPs Config */

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  /* USER CODE BEGIN RIF_Init 1 */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
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

void HAL_DCMIPP_PIPE_FrameEventCallback(
    DCMIPP_HandleTypeDef *hdcmipp,
    uint32_t Pipe)
{
    if (Pipe == DCMIPP_PIPE0)
    {
        frame_count++;
        frame_received = 1U;
    }
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

    printf("Non-zero bytes = %lu\r\n",
           non_zero_count);

    printf("Checksum       = 0x%08lX\r\n",
           checksum);

    printf("First 32 bytes:\r\n");

    for (uint32_t i = 0U; i < 64U; i++)
    {
        printf("%02X ", camera_framebuffer[i]);

        if (((i + 1U) % 16U) == 0U)
        {
            printf("\r\n");
        }
    }

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
