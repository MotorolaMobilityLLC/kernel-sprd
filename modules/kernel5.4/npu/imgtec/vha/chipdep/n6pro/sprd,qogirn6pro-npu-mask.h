
/*********************************** ai_apb MASKS, phy_addr=0x27000000 ************************************/
/* MASK for REG_AI_APB_APB_EB, [0x27000000]  */
#define MASK_AI_APB_NIC400_BUSMON_EB                                  0x00000040
#define MASK_AI_APB_AON_TO_OCM_PATH_EB                                0x00000020
#define MASK_AI_APB_AXI_PMON_EB                                       0x00000010
#define MASK_AI_APB_OCM_EB                                            0x00000008
#define MASK_AI_APB_DVFS_EB                                           0x00000004
#define MASK_AI_APB_MTX_APBREG_EB                                     0x00000002
#define MASK_AI_APB_POWERVR_EB                                        0x00000001
/* MASK for REG_AI_APB_APB_RST, [0x27000004]  */
#define MASK_AI_APB_POWERVR_RST_STATUS                                0x00000008
#define MASK_AI_APB_POWERVR_SOFT_RST                                  0x00000004
#define MASK_AI_APB_OCM_SOFT_RST                                      0x00000002
#define MASK_AI_APB_DVFS_SOFT_RST                                     0x00000001
/* MASK for REG_AI_APB_GEN_CLK_CFG, [0x27000008]  */
#define MASK_AI_APB_POWERVR_TMP_RESERVE                               0x00000001
/* MASK for REG_AI_APB_POWERVR_INT_STATUS, [0x2700000C]  */
#define MASK_AI_APB_FINISH_IRQ                                        0x00000007
/* MASK for REG_AI_APB_LPC_CFG_MTX_M0, [0x27000010]  */
#define MASK_AI_APB_PU_NUM_LPC_CFG_MTX_M0                             0x1FE00000
#define MASK_AI_APB_LP_EB_CFG_MTX_M0                                  0x00010000
#define MASK_AI_APB_LP_NUM_CFG_MTX_M0                                 0x0000FFFF
/* MASK for REG_AI_APB_LPC_CFG_MTX_S0, [0x27000014]  */
#define MASK_AI_APB_PU_NUM_LPC_CFG_MTX_S0                             0x1FE00000
#define MASK_AI_APB_LP_EB_CFG_MTX_S0                                  0x00010000
#define MASK_AI_APB_LP_NUM_CFG_MTX_S0                                 0x0000FFFF
/* MASK for REG_AI_APB_LPC_CFG_MTX_S1, [0x27000018]  */
#define MASK_AI_APB_PU_NUM_LPC_CFG_MTX_S1                             0x1FE00000
#define MASK_AI_APB_LP_EB_CFG_MTX_S1                                  0x00010000
#define MASK_AI_APB_LP_NUM_CFG_MTX_S1                                 0x0000FFFF
/* MASK for REG_AI_APB_LPC_CFG_MTX_S2, [0x2700001C]  */
#define MASK_AI_APB_PU_NUM_LPC_CFG_MTX_S2                             0x1FE00000
#define MASK_AI_APB_LP_EB_CFG_MTX_S2                                  0x00010000
#define MASK_AI_APB_LP_NUM_CFG_MTX_S2                                 0x0000FFFF
/* MASK for REG_AI_APB_LPC_MAIN_MTX_M0, [0x27000020]  */
#define MASK_AI_APB_PU_NUM_LPC_MAIN_MTX_M0                            0x1FE00000
#define MASK_AI_APB_LP_EB_MAIN_MTX_M0                                 0x00010000
#define MASK_AI_APB_LP_NUM_MAIN_MTX_M0                                0x0000FFFF
/* MASK for REG_AI_APB_LPC_MAIN_MTX_M1, [0x27000024]  */
#define MASK_AI_APB_PU_NUM_LPC_MAIN_MTX_M1                            0x1FE00000
#define MASK_AI_APB_LP_EB_MAIN_MTX_M1                                 0x00010000
#define MASK_AI_APB_LP_NUM_MAIN_MTX_M1                                0x0000FFFF
/* MASK for REG_AI_APB_LPC_MAIN_MTX_S0, [0x27000028]  */
#define MASK_AI_APB_PU_NUM_LPC_MAIN_MTX_S0                            0x1FE00000
#define MASK_AI_APB_LP_EB_MAIN_MTX_S0                                 0x00010000
#define MASK_AI_APB_LP_NUM_MAIN_MTX_S0                                0x0000FFFF
/* MASK for REG_AI_APB_LPC_MAIN_MTX_S1, [0x2700002C]  */
#define MASK_AI_APB_PU_NUM_LPC_MAIN_MTX_S1                            0x1FE00000
#define MASK_AI_APB_LP_EB_MAIN_MTX_S1                                 0x00010000
#define MASK_AI_APB_LP_NUM_MAIN_MTX_S1                                0x0000FFFF
/* MASK for REG_AI_APB_LPC_MAIN_MTX_MAIN, [0x27000030]  */
#define MASK_AI_APB_PU_NUM_LPC_MAIN_MTX_MAIN                          0x1FE00000
#define MASK_AI_APB_LP_EB_MAIN_MTX_MAIN                               0x00010000
#define MASK_AI_APB_LP_NUM_MAIN_MTX_MAIN                              0x0000FFFF
/* MASK for REG_AI_APB_LPC_AB_TO_DDR, [0x27000034]  */
#define MASK_AI_APB_PU_NUM_AB_TO_DDR                                  0x1FE00000
#define MASK_AI_APB_LP_EB_AB_TO_DDR                                   0x00010000
#define MASK_AI_APB_LP_NUM_AB_TO_DDR                                  0x0000FFFF
/* MASK for REG_AI_APB_ASYNC_BRIDGE_TO_DDR, [0x27000038]  */
#define MASK_AI_APB_RST_SUBSYS_AB_TO_DDR                              0x00000004
#define MASK_AI_APB_BRIDGE_TRANS_IDLE_AB_TO_DDR                       0x00000002
#define MASK_AI_APB_AXI_DETECTOR_OVERFLOW_AB_TO_DDR                   0x00000001
/* MASK for REG_AI_APB_AI_QOS_CTRL, [0x2700003C]  */
#define MASK_AI_APB_QOS_POWERVR_SEL_SW_AI_MAIN_MTX                    0x00010000
#define MASK_AI_APB_AWQOS_POWERVR_SW_AI_MAIN_MTX                      0x0000F000
#define MASK_AI_APB_ARQOS_POWERVR_SW_AI_MAIN_MTX                      0x00000F00
#define MASK_AI_APB_AWQOS_THRESHOLD_AI_MAIN_MTX                       0x000000F0
#define MASK_AI_APB_ARQOS_THRESHOLD_AI_MAIN_MTX                       0x0000000F
/* MASK for REG_AI_APB_LPC_CTRL, [0x27000040]  */
#define MASK_AI_APB_AXI_LP_CTRL_DISABLE                               0x00000001
/* MASK for REG_AI_APB_USER_GATE_FORCE_OFF, [0x27000044]  */
#define MASK_AI_APB_DJTAG_TCK_FR_FORCE_OFF                            0x00000080
#define MASK_AI_APB_TZPC_PFR_FORCE_OFF                                0x00000040
#define MASK_AI_APB_DVFS_FR_FORCE_OFF                                 0x00000020
#define MASK_AI_APB_OCM_FR_FORCE_OFF                                  0x00000010
#define MASK_AI_APB_CFG_MTX_FR_FORCE_OFF                              0x00000008
#define MASK_AI_APB_MAIN_MTX_FR_FORCE_OFF                             0x00000004
#define MASK_AI_APB_POWERVR_BUSMON_FORCE_OFF                          0x00000002
#define MASK_AI_APB_POWERVR_NNA_FORCE_OFF                             0x00000001
/* MASK for REG_AI_APB_USER_GATE_AUTO_GATE_EN, [0x27000048]  */
#define MASK_AI_APB_DJTAG_TCK_FR_AUTO_GATE_EN                         0x00000080
#define MASK_AI_APB_TZPC_PFR_AUTO_GATE_EN                             0x00000040
#define MASK_AI_APB_DVFS_FR_AUTO_GATE_EN                              0x00000020
#define MASK_AI_APB_OCM_FR_AUTO_GATE_EN                               0x00000010
#define MASK_AI_APB_CFG_MTX_FR_AUTO_GATE_EN                           0x00000008
#define MASK_AI_APB_MAIN_MTX_FR_AUTO_GATE_EN                          0x00000004
#define MASK_AI_APB_POWERVR_BUSMON_AUTO_GATE_EN                       0x00000002
#define MASK_AI_APB_POWERVR_NNA_AUTO_GATE_EN                          0x00000001
/* MASK for REG_AI_APB_AXI_BUSMON_CTRL, [0x2700004C]  */
#define MASK_AI_APB_BUSMON_PERFORM_ACK                                0x00000040
#define MASK_AI_APB_BUSMON_PERFORM_REQ                                0x00000020
#define MASK_AI_APB_BUSMON_F_DN_ACK                                   0x00000010
#define MASK_AI_APB_BUSMON_F_DN_REQ                                   0x00000008
#define MASK_AI_APB_BUSMON_F_UP_ACK                                   0x00000004
#define MASK_AI_APB_BUSMON_F_UP_REQ                                   0x00000002
#define MASK_AI_APB_BUSMON_CNT_START                                  0x00000001
/* MASK for REG_AI_APB_ASYNC_BRIDGE_DEBUG_BUS_R, [0x27000050]  */
#define MASK_AI_APB_BRIDGE_DEBUG_SIGNAL_R                             0xFFFFFFFF
/* MASK for REG_AI_APB_ASYNC_BRIDGE_DEBUG_BUS_W, [0x27000054]  */
#define MASK_AI_APB_BRIDGE_DEBUG_SIGNAL_W                             0xFFFFFFFF

/*********************************** ai_clk MASKS, phy_addr=0x27004000 ************************************/
/* MASK for REG_AI_CLK_CGM_POWERVR_DIV_CFG, [0x27004024]  */
#define MASK_AI_CLK_CGM_POWERVR_DIV                                   0x00000007
/* MASK for REG_AI_CLK_CGM_POWERVR_SEL_CFG, [0x27004028]  */
#define MASK_AI_CLK_CGM_POWERVR_SEL                                   0x00000003
/* MASK for REG_AI_CLK_CGM_MAIN_MTX_DIV_CFG, [0x27004030]  */
#define MASK_AI_CLK_CGM_MAIN_MTX_DIV                                  0x00000007
/* MASK for REG_AI_CLK_CGM_MAIN_MTX_SEL_CFG, [0x27004034]  */
#define MASK_AI_CLK_CGM_MAIN_MTX_SEL                                  0x00000003
/* MASK for REG_AI_CLK_CGM_CFG_MTX_DIV_CFG, [0x2700403C]  */
#define MASK_AI_CLK_CGM_CFG_MTX_DIV                                   0x00000007
/* MASK for REG_AI_CLK_CGM_CFG_MTX_SEL_CFG, [0x27004040]  */
#define MASK_AI_CLK_CGM_CFG_MTX_SEL                                   0x00000003
/* MASK for REG_AI_CLK_CGM_OCM_DIV_CFG, [0x27004048]  */
#define MASK_AI_CLK_CGM_OCM_DIV                                       0x00000007
/* MASK for REG_AI_CLK_CGM_OCM_SEL_CFG, [0x2700404C]  */
#define MASK_AI_CLK_CGM_OCM_SEL                                       0x00000003
/* MASK for REG_AI_CLK_CGM_DVFS_SEL_CFG, [0x27004058]  */
#define MASK_AI_CLK_CGM_DVFS_SEL                                      0x00000003

/*********************************** ai_mtx MASKS, phy_addr=0x2700C000 ************************************/
/* MASK for REG_AI_MTX_REGU_OT_CTRL_EN, [0x2700C000]  */
#define MASK_AI_MTX_OT_CTRL_EN                                        0x00000001
/* MASK for REG_AI_MTX_REGU_OT_CTRL_AW_CFG, [0x2700C004]  */
#define MASK_AI_MTX_URG_13_AW_INTVL_MODE                              0xC0000000
#define MASK_AI_MTX_URG_13_MAX_AW_OT                                  0x3F000000
#define MASK_AI_MTX_URG_00_AW_INTVL_MODE                              0x00C00000
#define MASK_AI_MTX_URG_00_MAX_AW_OT                                  0x003F0000
#define MASK_AI_MTX_URG_01_AW_INTVL_MODE                              0x0000C000
#define MASK_AI_MTX_URG_01_MAX_AW_OT                                  0x00003F00
#define MASK_AI_MTX_URG_03_AW_INTVL_MODE                              0x000000C0
#define MASK_AI_MTX_URG_03_MAX_AW_OT                                  0x0000003F
/* MASK for REG_AI_MTX_REGU_OT_CTRL_AR_CFG, [0x2700C008]  */
#define MASK_AI_MTX_URG_13_MAX_AR_OT                                  0x3F000000
#define MASK_AI_MTX_URG_00_MAX_AR_OT                                  0x003F0000
#define MASK_AI_MTX_URG_01_MAX_AR_OT                                  0x00003F00
#define MASK_AI_MTX_URG_03_MAX_AR_OT                                  0x0000003F
/* MASK for REG_AI_MTX_REGU_OT_CTRL_AX_CFG, [0x2700C00C]  */
#define MASK_AI_MTX_URG_3X_MAX_AR_OT                                  0x3F000000
#define MASK_AI_MTX_URG_1X_MAX_AR_OT                                  0x003F0000
#define MASK_AI_MTX_URG_3X_AW_INTVL_MODE                              0x0000C000
#define MASK_AI_MTX_URG_3X_MAX_AW_OT                                  0x00003F00
#define MASK_AI_MTX_URG_1X_AW_INTVL_MODE                              0x000000C0
#define MASK_AI_MTX_URG_1X_MAX_AW_OT                                  0x0000003F
/* MASK for REG_AI_MTX_REGU_LAT_EN, [0x2700C010]  */
#define MASK_AI_MTX_REGU_LAT_EN_R                                     0x00000002
#define MASK_AI_MTX_REGU_LAT_EN_W                                     0x00000001
/* MASK for REG_AI_MTX_REGU_LAT_W_CFG, [0x2700C014]  */
#define MASK_AI_MTX_LAT_WINDOW_LENGTH_W                               0xFFFF0000
#define MASK_AI_MTX_LAT_BLK_REQ_LAT_W                                 0x0000F000
#define MASK_AI_MTX_LAT_CMD_NOT_CYCLE_W                               0x00000800
#define MASK_AI_MTX_LAT_INCR_OT_DISABLE_W                             0x00000400
#define MASK_AI_MTX_LAT_BW_LIKE_MODE_W                                0x00000200
#define MASK_AI_MTX_LAT_ULTRA_DISABLE_W                               0x00000100
#define MASK_AI_MTX_LAT_AVG_LAT_EXP_W                                 0x000000FF
/* MASK for REG_AI_MTX_REGU_LAT_R_CFG, [0x2700C018]  */
#define MASK_AI_MTX_LAT_WINDOW_LENGTH_R                               0xFFFF0000
#define MASK_AI_MTX_LAT_BLK_REQ_LAT_R                                 0x0000F000
#define MASK_AI_MTX_LAT_CMD_NOT_CYCLE_R                               0x00000800
#define MASK_AI_MTX_LAT_INCR_OT_DISABLE_R                             0x00000400
#define MASK_AI_MTX_LAT_BW_LIKE_MODE_R                                0x00000200
#define MASK_AI_MTX_LAT_ULTRA_DISABLE_R                               0x00000100
#define MASK_AI_MTX_LAT_AVG_LAT_EXP_R                                 0x000000FF
/* MASK for REG_AI_MTX_REGU_LAT_STATUS, [0x2700C01C]  */
#define MASK_AI_MTX_WIN_LAT_ACT_OVF_W                                 0x00020000
#define MASK_AI_MTX_WIN_LAT_EXP_OVF_W                                 0x00010000
#define MASK_AI_MTX_WIN_LAT_ACT_OVF_R                                 0x00000002
#define MASK_AI_MTX_WIN_LAT_EXP_OVF_R                                 0x00000001
/* MASK for REG_AI_MTX_REGU_BW_NRT_EN, [0x2700C040]  */
#define MASK_AI_MTX_REGU_BW_NRT_R                                     0x00000002
#define MASK_AI_MTX_REGU_BW_NRT_W                                     0x00000001
/* MASK for REG_AI_MTX_REGU_BW_NRT_W_CFG_0, [0x2700C044]  */
#define MASK_AI_MTX_WIN_MAX_BW_EXP_W                                  0xFFFF0000
#define MASK_AI_MTX_WIN_MIN_BW_EXP_W                                  0x0000FFFF
/* MASK for REG_AI_MTX_REGU_BW_NRT_W_CFG_1, [0x2700C048]  */
#define MASK_AI_MTX_BW_NRT_IDLE_WIN_DISABLE_W                         0x04000000
#define MASK_AI_MTX_BW_NRT_INCR_OT_DISABLE_W                          0x02000000
#define MASK_AI_MTX_BW_NRT_ULTRA_DISABLE_W                            0x01000000
#define MASK_AI_MTX_BW_NRT_OT_LMT_W                                   0x003F0000
#define MASK_AI_MTX_BW_NRT_WINDOW_LENGTH_W                            0x0000FFFF
/* MASK for REG_AI_MTX_REGU_BW_NRT_R_CFG_0, [0x2700C04C]  */
#define MASK_AI_MTX_WIN_MAX_BW_EXP_R                                  0xFFFF0000
#define MASK_AI_MTX_WIN_MIN_BW_EXP_R                                  0x0000FFFF
/* MASK for REG_AI_MTX_REGU_BW_NRT_R_CFG_1, [0x2700C050]  */
#define MASK_AI_MTX_BW_NRT_IDLE_WIN_DISABLE_R                         0x04000000
#define MASK_AI_MTX_BW_NRT_INCR_OT_DISABLE_R                          0x02000000
#define MASK_AI_MTX_BW_NRT_ULTRA_DISABLE_R                            0x01000000
#define MASK_AI_MTX_BW_NRT_OT_LMT_R                                   0x003F0000
#define MASK_AI_MTX_BW_NRT_WINDOW_LENGTH_R                            0x0000FFFF
/* MASK for REG_AI_MTX_REGU_BW_NRT_STATUS, [0x2700C054]  */
#define MASK_AI_MTX_WIN_BW_ACT_OVF_W                                  0x00010000
#define MASK_AI_MTX_WIN_BW_ACT_OVF_R                                  0x00000001
/* MASK for REG_AI_MTX_REGU_AXQOS_GEN_EN, [0x2700C060]  */
#define MASK_AI_MTX_URGENCY_FEEDTHR                                   0x80000000
#define MASK_AI_MTX_GEN_EN_R                                          0x00000002
#define MASK_AI_MTX_GEN_EN_W                                          0x00000001
/* MASK for REG_AI_MTX_REGU_AXQOS_GEN_CFG, [0x2700C064]  */
#define MASK_AI_MTX_AWURGENCY                                         0x30000000
#define MASK_AI_MTX_AWQOS_ULTRA                                       0x0F000000
#define MASK_AI_MTX_AWQOS_HIGH                                        0x00F00000
#define MASK_AI_MTX_AWQOS_NORM                                        0x000F0000
#define MASK_AI_MTX_ARURGENCY                                         0x00003000
#define MASK_AI_MTX_ARQOS_ULTRA                                       0x00000F00
#define MASK_AI_MTX_ARQOS_HIGH                                        0x000000F0
#define MASK_AI_MTX_ARQOS_NORM                                        0x0000000F
/* MASK for REG_AI_MTX_REGU_URG_CNT_CFG, [0x2700C068]  */
#define MASK_AI_MTX_CNT_SEL                                           0x00000700
#define MASK_AI_MTX_CNT_EN                                            0x00000001
/* MASK for REG_AI_MTX_REGU_URG_CNT_VALUE, [0x2700C06C]  */
#define MASK_AI_MTX_AX_CNT                                            0xFFFFFFFF

/*********************************** ai_busmon MASKS, phy_addr=0x27010000 ************************************/
/* MASK for REG_AI_BUSMON_CHN_CFG, [0x27010000]  */
#define MASK_AI_BUSMON_INT_CLR                                        0x80000000
#define MASK_AI_BUSMON_PERFORM_INT_MASK_STS                           0x40000000
#define MASK_AI_BUSMON_F_DN_INT_MASK_STS                              0x20000000
#define MASK_AI_BUSMON_F_UP_INT_MASK_STS                              0x10000000
#define MASK_AI_BUSMON_PERFORM_INT_RAW                                0x04000000
#define MASK_AI_BUSMON_F_DN_INT_RAW                                   0x02000000
#define MASK_AI_BUSMON_F_UP_INT_RAW                                   0x01000000
#define MASK_AI_BUSMON_PERFORM_INT_EN                                 0x00400000
#define MASK_AI_BUSMON_F_DN_INT_EN                                    0x00200000
#define MASK_AI_BUSMON_F_UP_INT_EN                                    0x00100000
#define MASK_AI_BUSMON_PEAK_CNT_CLR                                   0x00000200
#define MASK_AI_BUSMON_PERFOR_REQ_EN                                  0x00000100
#define MASK_AI_BUSMON_F_DN_REQ_EN                                    0x00000080
#define MASK_AI_BUSMON_F_UP_REQ_EN                                    0x00000040
#define MASK_AI_BUSMON_RLATENCY_EN                                    0x00000020
#define MASK_AI_BUSMON_RBW_EN                                         0x00000010
#define MASK_AI_BUSMON_WLATENCY_EN                                    0x00000008
#define MASK_AI_BUSMON_WBW_EN                                         0x00000004
#define MASK_AI_BUSMON_AUTO_MODE_EN                                   0x00000002
#define MASK_AI_BUSMON_CHN_EN                                         0x00000001
/* MASK for REG_AI_BUSMON_PEAK_WIN_LEN, [0x27010004]  */
#define MASK_AI_BUSMON_PEAK_WIN_LEN                                   0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_DN_RBW_SET, [0x27010008]  */
#define MASK_AI_BUSMON_F_DN_RBW_SET                                   0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_DN_RLATENCY_SET, [0x2701000C]  */
#define MASK_AI_BUSMON_F_DN_RLATENCY_SET                              0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_DN_WBW_SET, [0x27010010]  */
#define MASK_AI_BUSMON_F_DN_WBW_SET                                   0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_DN_WLATENCY_SET, [0x27010014]  */
#define MASK_AI_BUSMON_F_DN_WLATENCY_SET                              0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_UP_RBW_SET, [0x27010018]  */
#define MASK_AI_BUSMON_F_UP_RBW_SET                                   0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_UP_RLATENCY_SET, [0x2701001C]  */
#define MASK_AI_BUSMON_F_UP_RLATENCY_SET                              0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_UP_WBW_SET, [0x27010020]  */
#define MASK_AI_BUSMON_F_UP_WBW_SET                                   0xFFFFFFFF
/* MASK for REG_AI_BUSMON_F_UP_WLATENCY_SET, [0x27010024]  */
#define MASK_AI_BUSMON_F_UP_WLATENCY_SET                              0xFFFFFFFF
/* MASK for REG_AI_BUSMON_RTRANS_IN_WIN, [0x27010028]  */
#define MASK_AI_BUSMON_RTRANS_IN_WIN                                  0xFFFFFFFF
/* MASK for REG_AI_BUSMON_RBW_IN_WIN, [0x2701002C]  */
#define MASK_AI_BUSMON_RBW_IN_WIN                                     0xFFFFFFFF
/* MASK for REG_AI_BUSMON_RLATENCY_IN_WIN, [0x27010030]  */
#define MASK_AI_BUSMON_RLATENCY_IN_WIN                                0xFFFFFFFF
/* MASK for REG_AI_BUSMON_WTRANS_IN_WIN, [0x27010034]  */
#define MASK_AI_BUSMON_WTRANS_IN_WIN                                  0xFFFFFFFF
/* MASK for REG_AI_BUSMON_WBW_IN_WIN, [0x27010038]  */
#define MASK_AI_BUSMON_WBW_IN_WIN                                     0xFFFFFFFF
/* MASK for REG_AI_BUSMON_WLATENCY_IN_WIN, [0x2701003C]  */
#define MASK_AI_BUSMON_WLATENCY_IN_WIN                                0xFFFFFFFF
/* MASK for REG_AI_BUSMON_PEAKBW_IN_WIN, [0x27010040]  */
#define MASK_AI_BUSMON_PEAKBW_IN_WIN                                  0xFFFFFFFF
