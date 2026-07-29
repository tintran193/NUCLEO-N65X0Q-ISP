#ifndef IMX219_H
#define IMX219_H

#include <stdint.h>

#include "imx219_reg.h"


/* ============================================================
 * I2C
 * ============================================================ */

#define IMX219_I2C_ADDR    (0x10U << 1)


/* ============================================================
 * Sensor ID
 * ============================================================ */

#define IMX219_REG_MODEL_ID_MSB    0x0000U
#define IMX219_REG_MODEL_ID_LSB    0x0001U

#define IMX219_MODEL_ID            0x0219U


/* ============================================================
 * Mode control
 * ============================================================ */

#define IMX219_REG_MODE_SELECT     0x0100U

#define IMX219_MODE_STANDBY        0x00U
#define IMX219_MODE_STREAMING      0x01U


/* ============================================================
 * CSI-2
 * ============================================================ */

#define IMX219_REG_CSI_LANE_MODE   0x0114U

#define IMX219_CSI_2_LANE_MODE     0x01U
#define IMX219_CSI_4_LANE_MODE     0x03U


/*
 * CSI data format
 *
 * RAW10:
 *
 *     0x0112 = 0x0A
 *     0x0113 = 0x0A
 *
 *     CSI_data_format = 0x0A0A
 */

#define IMX219_REG_CSI_FORMAT_MSB  0x018CU
#define IMX219_REG_CSI_FORMAT_LSB  0x018DU

#define IMX219_RAW10               0x0AU
#define IMX219_RAW8 			   0x08U


/* ============================================================
 * Frame timing
 * ============================================================ */

#define IMX219_REG_FRAME_LENGTH_MSB    0x0160U
#define IMX219_REG_FRAME_LENGTH_LSB    0x0161U

#define IMX219_REG_LINE_LENGTH_MSB     0x0162U
#define IMX219_REG_LINE_LENGTH_LSB     0x0163U


/* ============================================================
 * Exposure / gain
 * ============================================================ */

#define IMX219_REG_EXPOSURE_MSB        0x015AU
#define IMX219_REG_EXPOSURE_LSB        0x015BU

#define IMX219_REG_ANALOG_GAIN         0x0157U


/* ============================================================
 * Output resolution
 * ============================================================ */

#define IMX219_REG_X_OUTPUT_SIZE_MSB   0x016CU
#define IMX219_REG_X_OUTPUT_SIZE_LSB   0x016DU

#define IMX219_REG_Y_OUTPUT_SIZE_MSB   0x016EU
#define IMX219_REG_Y_OUTPUT_SIZE_LSB   0x016FU

/* ============================================================
 * Register Table Struct
 * ============================================================ */
typedef struct
{
    uint16_t address;
    uint8_t value;
} IMX219_Reg_t;

int32_t IMX219_WriteTable(
    IMX219_CTX_t *ctx,
    const IMX219_Reg_t *table,
    uint32_t size
);

/* ============================================================
 * Driver API
 * ============================================================ */

int32_t IMX219_Init(
    IMX219_CTX_t *ctx
);


int32_t IMX219_Start(
    IMX219_CTX_t *ctx
);


int32_t IMX219_Stop(
    IMX219_CTX_t *ctx
);


int32_t IMX219_ReadID(
    IMX219_CTX_t *ctx,
    uint16_t *id
);


#endif /* IMX219_H */
