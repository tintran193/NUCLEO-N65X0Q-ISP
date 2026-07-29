#ifndef IMX219_PORT_H
#define IMX219_PORT_H

#include "main.h"
#include <stdint.h>

/*
 * STM32-specific I2C functions
 */

int32_t IMX219_I2C_ReadReg(
    void *handle,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
);

int32_t IMX219_I2C_WriteReg(
    void *handle,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
);


#endif /* IMX219_PORT_H */
