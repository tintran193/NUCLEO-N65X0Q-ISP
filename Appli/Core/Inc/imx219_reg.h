#ifndef IMX219_REG_H
#define IMX219_REG_H

#include <stdint.h>

typedef struct
{
    void *handle;

    int32_t (*ReadReg)(
        void *handle,
        uint16_t reg,
        uint8_t *data,
        uint16_t length
    );

    int32_t (*WriteReg)(
        void *handle,
        uint16_t reg,
        uint8_t *data,
        uint16_t length
    );

} IMX219_CTX_t;

int32_t IMX219_ReadReg(
    IMX219_CTX_t *ctx,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
);

int32_t IMX219_WriteReg(
    IMX219_CTX_t *ctx,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
);


#endif /* IMX219_REG_H */
