/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
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

/* no need
 * int sprd_cam_pw_domain_init(struct platform_device *pdev);
 * int sprd_cam_pw_domain_deinit(void);
 */
int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);

/* chip id0, id1
 * sharkl5:id1:"SHAR",id0:"KL5",
 */
uint32_t sprd_chip_id0(void);
uint32_t sprd_chip_id1(void);

#endif /* _CAM_PW_DOMAIN_H_ */


/* Note, (sharkl5)for bringup, remove after bringup
 * 0x327E0024.b24b25 pmu_apb_mm_sys_cfg
 * 0x327E010C.b23:b16 PWR_STATUS3_DBG.PD_MM_TOP_STATE

 * 0x327D0000.b9 [aon]APB_EB0.MM_EB

 * 0x327D0004.b12 [aon]APB_EB1.ANA_EB(TBD)

 * 0x62200000 MM_AHB,0:cpp,1:jpg,2:dcam,3:isp,4:csi2,5:csi1
 *                   6:csi0,7:ckg,8:isp_ahb,9:dvfs,10:fd
 * 0x62200008 MM_GEN_CLK_CFG,0:sensor2,1:sensor1;2:sensor0,
 *                   3:mipi_csi2,4:mipi_csi1,5:mipi_csi0,
 *                   6:dcam_axi,7:isp_axi,8:cphy_cfg
 */
