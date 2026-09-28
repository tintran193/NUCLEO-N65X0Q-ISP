#include "imx219.h"
#include <stdio.h>

int32_t IMX219_ReadID(
    IMX219_CTX_t *ctx,
    uint16_t *id
)
{
    uint8_t id_msb;
    uint8_t id_lsb;

    int32_t status;

    /*
     * Read 0x0000
     */

    status =
        IMX219_ReadReg(
            ctx,
            IMX219_REG_MODEL_ID_MSB,
            &id_msb,
            1
        );


    if (status != 0)
    {
        return status;
    }


    /*
     * Read 0x0001
     */

    status =
        IMX219_ReadReg(
            ctx,
            IMX219_REG_MODEL_ID_LSB,
            &id_lsb,
            1
        );

    if (status != 0)
    {
        return status;
    }


    /*
     * Combine:
     *
     * 0x02
     * 0x19
     *
     * 0x0219
     */

    *id =
        ((uint16_t)id_msb << 8) | id_lsb;
    return 0;
}

//int32_t IMX219_WriteTable(
//    IMX219_CTX_t *ctx,
//    const IMX219_Reg_t *table,
//    uint32_t size
//)
//{
//    int32_t status;
//
//
//    for (
//        uint32_t i = 0;
//
//        i < size;
//
//        i++
//    )
//    {
//        uint8_t value;
//
//
//        value =
//            table[i].value;
//
//
//        status =
//            IMX219_WriteReg(
//                ctx,
//
//                table[i].address,
//
//                &value,
//
//                1
//            );
//
//
//        if (status != 0)
//        {
//            printf(
//                "IMX219 write failed: "
//                "REG=0x%04X "
//                "DATA=0x%02X\r\n",
//
//                table[i].address,
//
//                table[i].value
//            );
//
//
//            return status;
//        }
//    }
//
//
//    return 0;
//}
int32_t IMX219_WriteTable(
    IMX219_CTX_t *ctx,
    const IMX219_Reg_t *table,
    uint32_t size
)
{
    int32_t status;

    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t value;

        value = table[i].value;

        printf(
            "WRITE REG=0x%04X DATA=0x%02X\r\n",
            table[i].address,
            value
        );

        status =
            IMX219_WriteReg(
                ctx,
                table[i].address,
                &value,
                1
            );

        if (status != 0)
        {
            printf(
                "WRITE FAILED: REG=0x%04X\r\n",
                table[i].address
            );

            return status;
        }
    }

    return 0;
}
static const IMX219_Reg_t imx219_common_regs[] =
{
	/* Mode Select */
    {0x0100,0x00},
	/* To Access Addresses 3000-5fff, send the following commands */

    {0x30EB,0x05},
	{0x30EB,0x0C},
    {0x300A,0xFF},
    {0x300B,0xFF},
    {0x30EB,0x05},
    {0x30EB,0x09},

	/* PLL Clock Table */

    {0x0301,0x05},
    {0x0303,0x01},
    {0x0304,0x03},
    {0x0305,0x03},
    {0x0306,0x00},
    {0x0307,0x39},
    {0x030B,0x01},
    {0x030C,0x00},
    /* Bug 18 (see WORKLOG.md): this was hand-edited to 0x1C ("div 4"),
     * cutting PLL_OP_MPY from 114 to 28 -- i.e. the MIPI output PLL, which
     * sets the actual CSI-2 per-lane bit rate (bit_rate_Mbps = INCK_MHz /
     * PREPLLCK_OP_DIV / OPSYCK_DIV * PLL_OP_MPY = 24/3/1*114 = 912 Mbps at
     * the correct value, vs only ~224 Mbps at 0x1C). ../Camera_N6_AI_Test
     * (confirmed working on this exact IMX219 module, same 640x480 RAW10
     * 2-lane 30fps config, same PLL_VT/PREPLLCK settings otherwise
     * identical to this table) uses 0x72 with DCMIPP_CSI_PHY_BT_900 and
     * only has color/FPS issues, no striping -- this is almost certainly
     * the root cause of the exact, deterministic period-32-row corruption
     * chased through Bugs 15-17: running the D-PHY link at ~4x below its
     * intended bit rate is a physical-layer misconfiguration, not
     * something any DCMIPP register (IPPlug, ISP) can compensate for.
     * Restored to the reference project's confirmed-working value; see
     * main.c's matching PHYBitrate change (PHY_BT_220 -> PHY_BT_900). */
    {0x030D,0x72},

	/* Undocumented registers */
	{0x455E,0x00},
	{0x471E,0x4B},
	{0x4767,0x0F},
	{0x4750,0x14},
	{0x4540,0x00},
	{0x47B4,0x14},
	{0x4713,0x30},
	{0x478B,0x10},
	{0x478F,0x10},
	{0x4793,0x10},
	{0x4797,0x0E},
	{0x479B,0x0E},

    /*
     * CSI-2: 2 lanes
     */

    {0x0114,0x01},

    /*
     * MIPI global timing: automatic
     */

    {0x0128, 0x00},

    /* =========================================================
     * Frame timing
     * ========================================================= */

    /* Bug 18 continued (see WORKLOG.md): FRM_LENGTH_LINES here was 0x06E3
     * (1763) -- exactly HALF of ../Camera_N6_AI_Test's confirmed-working
     * 0x0DC6 (3526) for this same 640x480 2-lane 30fps config. A shorter
     * frame length means less vertical blanking time between frames, which
     * lines up with the same "someone halved/quartered sensor timing
     * without adjusting everything else consistently" pattern as the
     * 0x030D PLL edit above. Restored to match the reference exactly. */
    /* Frame length lines = 0x0DC6 = 3526 */
    {0x0160, 0x0D},
    {0x0161, 0xC6},

    /* Line length pixels = 0x0D78 = 3448 */
    {0x0162, 0x0D},
    {0x0163, 0x78},
    /*
     * External clock: 24 MHz
     */

    {0x012A,0x18},
    {0x012B,0x00}
};

static const IMX219_Reg_t imx219_640x480_regs[] =
{
    /*
     * X start = 1000 = 0x03E8
     */

    {0x0164,0x03},
    {0x0165,0xE8},

    /*
     * X end = 2279 = 0x08E7
     */

    {0x0166,0x08},
    {0x0167,0xE7},

    /*
     * Y start = 752 = 0x02F0
     */

    {0x0168,0x02},
    {0x0169,0xF0},

    /*
     * Y end = 1711 = 0x06AF
     */

    {0x016A,0x06},
    {0x016B,0xAF},

    /*
     * Width = 640 = 0x0280
     */

    {0x016C,0x02},
    {0x016D,0x80},

    /*
     * Height = 480 = 0x01E0
     */

    {0x016E,0x01},
    {0x016F,0xE0},

    /* =========================================================
     * Binning / scaling
     * ========================================================= */

    {0x0170, 0x01},
    {0x0171, 0x01},

    /* Bug 18 continued (see WORKLOG.md): BINNING_MODE_H_A/V_A were 0x01/0x01
     * here, with a stripped trailing comment ("//") matching the same
     * tamper pattern as the 0x030D PLL edit and the halved FRM_LENGTH_LINES
     * above. ../Camera_N6_AI_Test (confirmed working, identical sensor
     * window/output size) uses 0x03/0x03 for 2x2 analog binning. Restored
     * to match -- a different binning submode here changes the sensor's
     * internal analog readout timing, not just software-visible output. */
    {0x0174, 0x03}, /* BINNING_MODE_H_A = 2x2 */
    {0x0175, 0x03}, /* BINNING_MODE_V_A = 2x2 */

	{0x0624, 0x06},
	{0x0625, 0x68},

	{0x0626, 0x04},
	{0x0627, 0xD0}



};

static const IMX219_Reg_t imx219_raw10_regs[] =
{
    /*
     * CSI_data_format = 0x0A0A
     */

    {IMX219_REG_CSI_FORMAT_MSB,0x0A},
    {IMX219_REG_CSI_FORMAT_LSB,0x0A},
	{0x0309,0x0A}
};

static int32_t IMX219_Configure640x480(
    IMX219_CTX_t *ctx
)
{
    int32_t status;
    /*
     * 1. Common configuration
     */

    status =
        IMX219_WriteTable(
            ctx,
            imx219_common_regs,
            sizeof(imx219_common_regs)/sizeof(imx219_common_regs[0])
        );

    if (status != 0)
    {
        return status;
    }

    /*
     * 2. 640×480
     */

    status =
        IMX219_WriteTable(
            ctx,
            imx219_640x480_regs,
            sizeof(imx219_640x480_regs)/sizeof(imx219_640x480_regs[0])
        );

    if (status != 0)
    {
        return status;
    }

    /*
     * 3. RAW10
     */

    status =
        IMX219_WriteTable(
            ctx,
            imx219_raw10_regs,
            sizeof(imx219_raw10_regs)/sizeof(imx219_raw10_regs[0])
        );

    if (status != 0)
    {
        return status;
    }

    /*
     * Bug 21 (see WORKLOG.md): this explicit write ran AFTER
     * imx219_common_regs (which sets FRM_LENGTH_LINES via the table --
     * see Bug 18) and unconditionally overwrote it back to 0x06E3=1763,
     * silently undoing Bug 18's fix on every single init this whole time.
     * Both this and the table happened to agree before Bug 18 (both wrong
     * at 1763), which is exactly why nothing caught the duplication then.
     * Updated to match the table's corrected value.
     *
     * 4. Frame length = 0x0DC6 (3526, see ../Camera_N6_AI_Test)
     */

    uint8_t frame_length_msb = 0x0D;
    uint8_t frame_length_lsb = 0xC6;


    status =
        IMX219_WriteReg(
            ctx,
            IMX219_REG_FRAME_LENGTH_MSB,
            &frame_length_msb,
            1
        );


    if (status != 0)
    {
        return status;
    }


    status =
        IMX219_WriteReg(
            ctx,
            IMX219_REG_FRAME_LENGTH_LSB,
            &frame_length_lsb,
            1
        );


    if (status != 0)
    {
        return status;
    }


    /*
     * 5. Exposure = 0x0640
     */

    uint8_t exposure_msb = 0x06;
    uint8_t exposure_lsb = 0x40;

    status =
        IMX219_WriteReg(
            ctx,
            IMX219_REG_EXPOSURE_MSB,
            &exposure_msb,
            1
        );


    if (status != 0)
    {
        return status;
    }


    status =
        IMX219_WriteReg(
            ctx,
            IMX219_REG_EXPOSURE_LSB,
            &exposure_lsb,
            1
        );


    if (status != 0)
    {
        return status;
    }


    /*
     * 6. Analog gain
     */

    uint8_t gain = 0x00;

    return IMX219_WriteReg(
        ctx,
        IMX219_REG_ANALOG_GAIN,
        &gain,
        1
    );
}

static int32_t IMX219_VerifyConfiguration(
    IMX219_CTX_t *ctx
)
{
    uint8_t value;

    /*
     * Verify mode
     */

    if (
        IMX219_ReadReg(
            ctx,
            IMX219_REG_MODE_SELECT,
            &value,
            1
        ) != 0
    )
    {
        return -1;
    }

    printf(
        "MODE_SELECT = 0x%02X\r\n",
        value
    );

    if (
        value != IMX219_MODE_STANDBY
    )
    {
        return -1;
    }


    /*
     * Verify CSI lanes
     */

    if (
        IMX219_ReadReg(
            ctx,
            IMX219_REG_CSI_LANE_MODE,
            &value,
            1
        ) != 0
    )
    {
        return -1;
    }


    printf(
        "CSI lane mode = 0x%02X\r\n",
        value
    );


    if (
        value != IMX219_CSI_2_LANE_MODE
    )
    {
        return -1;
    }


    /*
     * Verify RAW10 MSB
     */

    if (
        IMX219_ReadReg(
            ctx,
            IMX219_REG_CSI_FORMAT_MSB,
            &value,
            1
        ) != 0
    )
    {
        return -1;
    }


    printf(
        "CSI format MSB = 0x%02X\r\n",
        value
    );

    if (
        value != IMX219_RAW10
    )
    {
        return -1;
    }

    /*
     * Verify RAW10 LSB
     */

    if (
        IMX219_ReadReg(
            ctx,
            IMX219_REG_CSI_FORMAT_LSB,
            &value,
            1
        ) != 0
    )
    {
        return -1;
    }


    printf(
        "CSI format LSB = 0x%02X\r\n",
        value
    );


    if (
        value != IMX219_RAW10
    )
    {
        return -1;
    }

    return 0;
}

int32_t IMX219_Start(
    IMX219_CTX_t *ctx
)
{
    uint8_t value;

    value = IMX219_MODE_STREAMING;

    return IMX219_WriteReg(
        ctx,
        IMX219_REG_MODE_SELECT,
        &value,
        1
    );
}

int32_t IMX219_Stop(
    IMX219_CTX_t *ctx
)
{
    uint8_t value;

    value = IMX219_MODE_STANDBY;

    return IMX219_WriteReg(
        ctx,
        IMX219_REG_MODE_SELECT,
        &value,
        1
    );
}

int32_t IMX219_Init(
    IMX219_CTX_t *ctx
)
{
    uint16_t id;
    int32_t status;

    printf(
        "IMX219: reading ID\r\n"
    );

    status =
        IMX219_ReadID(
            ctx,
            &id
        );

    if (status != 0)
    {
        printf(
            "IMX219: ID read failed\r\n"
        );
        return status;
    }

    printf(
        "IMX219 ID = 0x%04X\r\n",
        id
    );


    if (
        id != IMX219_MODEL_ID
    )
    {
        printf(
            "IMX219: wrong sensor ID\r\n"
        );
        return -1;
    }


    /*
     * Keep sensor in standby
     */

    status =
        IMX219_Stop(
            ctx
        );


    if (status != 0)
    {
        return status;
    }


    printf(
        "IMX219: configuring 640x480 RAW10\r\n"
    );


    status =
        IMX219_Configure640x480(
            ctx
        );


    if (status != 0)
    {
        return status;
    }


    printf(
        "IMX219: verifying configuration\r\n"
    );


    status =
        IMX219_VerifyConfiguration(
            ctx
        );


    if (status != 0)
    {
        printf(
            "IMX219: verification failed\r\n"
        );

        return status;
    }

    printf(
        "IMX219: configuration OK\r\n"
    );

    return 0;
}


int32_t IMX219_SetExposure(IMX219_CTX_t *ctx, uint16_t exposure)
{
    uint8_t msb = (exposure >> 8) & 0xFF;
    uint8_t lsb = exposure & 0xFF;

    if (IMX219_WriteReg(ctx, IMX219_REG_EXPOSURE_MSB, &msb, 1) != 0)
        return -1;

    if (IMX219_WriteReg(ctx, IMX219_REG_EXPOSURE_LSB, &lsb, 1) != 0)
        return -1;

    return 0;
}

int32_t IMX219_SetGain(IMX219_CTX_t *ctx, uint8_t gain)
{
    return IMX219_WriteReg(ctx,
                           IMX219_REG_ANALOG_GAIN,
                           &gain,
                           1);
}
