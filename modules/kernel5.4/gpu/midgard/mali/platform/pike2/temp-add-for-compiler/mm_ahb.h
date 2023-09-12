/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-10-12 09:49:25
 *
 */


#ifndef MM_AHB_H
#define MM_AHB_H



#define REG_MM_AHB_AHB_EB                (0x0000)
#define REG_MM_AHB_AHB_RST               (0x0004)
#define REG_MM_AHB_GEN_CKG_CFG           (0x0008)
#define REG_MM_AHB_MIPI_CSI2_CTRL        (0x000C)
#define REG_MM_AHB_ISP_QOS_CTRL          (0x0010)
#define REG_MM_AHB_CAM_MTX_LPC_CTRL      (0x0014)
#define REG_MM_AHB_VSP_MTX_LPC_CTRL_M0   (0x0018)
#define REG_MM_AHB_VSP_MTX_LPC_CTRL_M1   (0x001C)
#define REG_MM_AHB_VSP_MTX_LPC_CTRL_S0   (0x0020)
#define REG_MM_AHB_MM_MTX_LPC_STAT       (0x0024)
#define REG_MM_AHB_MM_LIGHT_SLEEP_CTRL   (0x0028)
#define REG_MM_AHB_CAM_MTX_LPC_CTRL_M0   (0x002C)
#define REG_MM_AHB_CAM_MTX_LPC_CTRL_M1   (0x0030)
#define REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV  (0x0034)
#define REG_MM_AHB_MM_DUMMY_REG_0        (0x0038)
#define REG_MM_AHB_MM_DUMMY_REG_1        (0x003C)

/* REG_MM_AHB_AHB_EB */

#define BIT_MM_AHB_VSP_MTX_QOS_AHB_EB        BIT(6)
#define BIT_MM_AHB_CKG_EB                    BIT(5)
#define BIT_MM_AHB_JPG_EB                    BIT(4)
#define BIT_MM_AHB_CSI_EB                    BIT(3)
#define BIT_MM_AHB_VSP_EB                    BIT(2)
#define BIT_MM_AHB_ISP_EB                    BIT(1)
#define BIT_MM_AHB_DCAM_EB                   BIT(0)

/* REG_MM_AHB_AHB_RST */

#define BIT_MM_AHB_AXI_VSP_MTX_SOFT_RST      BIT(12)
#define BIT_MM_AHB_AXI_CAM_MTX_SOFT_RST      BIT(11)
#define BIT_MM_AHB_AHB_CKG_SOFT_RST          BIT(10)
#define BIT_MM_AHB_APB_CSI_SOFT_RST          BIT(9)
#define BIT_MM_AHB_JPG_SOFT_RST              BIT(8)
#define BIT_MM_AHB_VSP_SOFT_RST              BIT(7)
#define BIT_MM_AHB_ISP_CFG_SOFT_RST          BIT(6)
#define BIT_MM_AHB_ISP_LOG_SOFT_RST          BIT(5)
#define BIT_MM_AHB_DCAM_ROT_SOFT_RST         BIT(4)
#define BIT_MM_AHB_DCAM_CAM2_SOFT_RST        BIT(3)
#define BIT_MM_AHB_DCAM_CAM1_SOFT_RST        BIT(2)
#define BIT_MM_AHB_DCAM_CAM0_SOFT_RST        BIT(1)
#define BIT_MM_AHB_DCAM_SOFT_RST             BIT(0)

/* REG_MM_AHB_GEN_CKG_CFG */

#define BIT_MM_AHB_MIPI_CSI_CKG_EN           BIT(4)
#define BIT_MM_AHB_DCAM_AXI_CKG_EN           BIT(3)
#define BIT_MM_AHB_ISP_AXI_CKG_EN            BIT(2)
#define BIT_MM_AHB_SENSOR0_CKG_EN            BIT(1)
#define BIT_MM_AHB_CPHY_CFG_CKG_EN           BIT(0)

/* REG_MM_AHB_MIPI_CSI2_CTRL */

#define BIT_MM_AHB_MIPI_CPHY_SEL             BIT(0)

/* REG_MM_AHB_ISP_QOS_CTRL */

#define BIT_MM_AHB_ISP_ARQOS(x)              (((x) & 0xF) << 4)
#define BIT_MM_AHB_ISP_AWQOS(x)              (((x) & 0xF))

/* REG_MM_AHB_CAM_MTX_LPC_CTRL */

#define BIT_MM_AHB_CAM_MTX_LPC_FORCE         BIT(17)
#define BIT_MM_AHB_CAM_MTX_LPC_EB            BIT(16)
#define BIT_MM_AHB_CAM_MTX_LPC_NUM(x)        (((x) & 0xFFFF))

/* REG_MM_AHB_VSP_MTX_LPC_CTRL_M0 */

#define BIT_MM_AHB_VSP_MTX_M0_LPC_FORCE      BIT(17)
#define BIT_MM_AHB_VSP_MTX_M0_LPC_EB         BIT(16)
#define BIT_MM_AHB_VSP_MTX_M0_LPC_NUM(x)     (((x) & 0xFFFF))

/* REG_MM_AHB_VSP_MTX_LPC_CTRL_M1 */

#define BIT_MM_AHB_VSP_MTX_M1_LPC_FORCE      BIT(17)
#define BIT_MM_AHB_VSP_MTX_M1_LPC_EB         BIT(16)
#define BIT_MM_AHB_VSP_MTX_M1_LPC_NUM(x)     (((x) & 0xFFFF))

/* REG_MM_AHB_VSP_MTX_LPC_CTRL_S0 */

#define BIT_MM_AHB_VSP_MTX_S0_LPC_FORCE      BIT(17)
#define BIT_MM_AHB_VSP_MTX_S0_LPC_EB         BIT(16)
#define BIT_MM_AHB_VSP_MTX_S0_LPC_NUM(x)     (((x) & 0xFFFF))

/* REG_MM_AHB_MM_MTX_LPC_STAT */

#define BIT_MM_AHB_VSP_MTX_S0_FORCE_ACK      BIT(13)
#define BIT_MM_AHB_VSP_MTX_GPV_FORCE_ACK     BIT(12)
#define BIT_MM_AHB_VSP_MTX_M1_FORCE_ACK      BIT(11)
#define BIT_MM_AHB_VSP_MTX_M0_FORCE_ACK      BIT(10)
#define BIT_MM_AHB_CAM_MTX_LPC_FORCE_ACK     BIT(9)
#define BIT_MM_AHB_CAM_MTX_M1_FORCE_ACK      BIT(8)
#define BIT_MM_AHB_CAM_MTX_M0_FORCE_ACK      BIT(7)
#define BIT_MM_AHB_VSP_MTX_S0_LPC_STAT       BIT(6)
#define BIT_MM_AHB_VSP_MTX_GPV_LPC_STAT      BIT(5)
#define BIT_MM_AHB_VSP_MTX_M1_LPC_STAT       BIT(4)
#define BIT_MM_AHB_VSP_MTX_M0_LPC_STAT       BIT(3)
#define BIT_MM_AHB_CAM_MTX_M1_LPC_STAT       BIT(2)
#define BIT_MM_AHB_CAM_MTX_M0_LPC_STAT       BIT(1)
#define BIT_MM_AHB_CAM_MTX_LPC_STAT          BIT(0)

/* REG_MM_AHB_MM_LIGHT_SLEEP_CTRL */

#define BIT_MM_AHB_REG_CAM_MTX_AUTO_CTRL_EN  BIT(13)
#define BIT_MM_AHB_REG_CAM_MTX_LP_DISABLE    BIT(10)
#define BIT_MM_AHB_REG_CAM_MTX_FRC_LSLP_M0   BIT(9)
#define BIT_MM_AHB_REG_CAM_MTX_FRC_LSLP_M1   BIT(8)
#define BIT_MM_AHB_REG_VSP_MTX_AUTO_CTRL_EN  BIT(5)
#define BIT_MM_AHB_REG_VSP_MTX_LP_DISABLE    BIT(2)
#define BIT_MM_AHB_REG_VSP_MTX_FRC_LSLP_M0   BIT(1)
#define BIT_MM_AHB_REG_VSP_MTX_FRC_LSLP_M1   BIT(0)

/* REG_MM_AHB_CAM_MTX_LPC_CTRL_M0 */

#define BIT_MM_AHB_CAM_MTX_M0_LPC_FORCE      BIT(17)
#define BIT_MM_AHB_CAM_MTX_M0_LPC_EB         BIT(16)
#define BIT_MM_AHB_CAM_MTX_M0_LPC_NUM(x)     (((x) & 0xFFFF))

/* REG_MM_AHB_CAM_MTX_LPC_CTRL_M1 */

#define BIT_MM_AHB_CAM_MTX_M1_LPC_FORCE      BIT(17)
#define BIT_MM_AHB_CAM_MTX_M1_LPC_EB         BIT(16)
#define BIT_MM_AHB_CAM_MTX_M1_LPC_NUM(x)     (((x) & 0xFFFF))

/* REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV */

#define BIT_MM_AHB_VSP_MTX_GPV_LPC_FORCE     BIT(17)
#define BIT_MM_AHB_VSP_MTX_GPV_LPC_EB        BIT(16)
#define BIT_MM_AHB_VSP_MTX_GPV_LPC_NUM(x)    (((x) & 0xFFFF))

/* REG_MM_AHB_MM_DUMMY_REG_0 */

#define BIT_MM_AHB_MM_DUMMY_REG_0(x)         (((x) & 0xFFFFFFFF))

/* REG_MM_AHB_MM_DUMMY_REG_1 */

#define BIT_MM_AHB_MM_DUMMY_REG_1(x)         (((x) & 0xFFFFFFFF))


#endif /* MM_AHB_H */

