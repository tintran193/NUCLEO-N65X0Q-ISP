#include "imx219_port.h"
#include "imx219.h"


int32_t IMX219_I2C_ReadReg(
    void *handle,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
) {
    I2C_HandleTypeDef *hi2c;
    HAL_StatusTypeDef status;
    /*
     * Convert generic handle
     * back to STM32 I2C handle.
     */

    hi2c = (I2C_HandleTypeDef *)handle;

    status = HAL_I2C_Mem_Read(
                hi2c,
                IMX219_I2C_ADDR,
                reg,
                I2C_MEMADD_SIZE_16BIT,
                data,
                length,
                100
            );

    if (status != HAL_OK)
    {
        return -1;
    }
    return 0;//HAL_OK
}

int32_t IMX219_I2C_WriteReg(
    void *handle,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
) {
    I2C_HandleTypeDef *hi2c;

    HAL_StatusTypeDef status;

    hi2c = (I2C_HandleTypeDef *)handle;

    status = HAL_I2C_Mem_Write(
                hi2c,
                IMX219_I2C_ADDR,
                reg,
                I2C_MEMADD_SIZE_16BIT,
                data,
                length,
                100
            );

    if (status != HAL_OK)
    {
        return -1;
    }
    return 0; //HAL_OK
}
