#include "imx219_reg.h"

int32_t IMX219_ReadReg(
    IMX219_CTX_t *ctx,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
){
    return ctx->ReadReg(
        ctx->handle,
        reg,
        data,
        length
    );
}

int32_t IMX219_WriteReg(
    IMX219_CTX_t *ctx,
    uint16_t reg,
    uint8_t *data,
    uint16_t length
){
    return ctx->WriteReg(
        ctx->handle,
        reg,
        data,
        length
    );
}
