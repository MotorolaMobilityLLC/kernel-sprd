/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_PW_DOMAIN_H_
#define _CAM_PW_DOMAIN_H_

#define REG_PMU_APB_PD_MM_SYS_CFG                (0x001C)
#define BIT_PMU_APB_PD_MM_SYS_AUTO_SHUTDOWN_EN   BIT(24)
#define BIT_PMU_APB_PD_MM_SYS_FORCE_SHUTDOWN     BIT(25)
#define REG_PMU_APB_PD_STATE                     (0x00BC)

#ifndef FPGA_BRINGUP
#define REG_AON_APB_AON_CHIP_ID0                 0
#define REG_AON_APB_AON_CHIP_ID1                 0
#define BIT_AON_APB_CLK_MM_AHB_EB                0
#define REG_PMU_APB_PD_MM_TOP_CFG                0
#define BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN   0
#define BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN     0
#define BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN     0
#define REG_PMU_APB_PWR_STATUS0_DBG              0
#define BIT_AON_APB_CLK_MM_EMC_EB                0
#define BIT_AON_APB_CLK_SENSOR2_EB               0
#define BIT_AON_APB_CLK_DCAM_IF_EB               0
#define BIT_AON_APB_CLK_ISP_EB                   0
#define BIT_AON_APB_CLK_JPG_EB                   0
#define BIT_AON_APB_CLK_CPP_EB                   0
#define BIT_AON_APB_CLK_SENSOR0_EB               0
#define BIT_AON_APB_CLK_SENSOR1_EB               0
#define BIT_AON_APB_CLK_MM_VSP_EMC_EB            0
#define BIT_AON_APB_CLK_MM_VSP_AHB_EB            0
#define BIT_AON_APB_CLK_VSP_EB                   0
#define REG_AON_APB_AON_CLK_TOP_CFG              0
#define REG_PMU_APB_BUS_STATUS0                  0

#endif
int sprd_cam_pw_domain_init(struct platform_device *pdev);
int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);

#endif /* _CAM_PW_DOMAIN_H_ */
