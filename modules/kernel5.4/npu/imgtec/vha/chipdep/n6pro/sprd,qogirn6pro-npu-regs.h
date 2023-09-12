
/*********************************** ai_apb REGS, phy_addr=0x27000000 ************************************/
#define REG_AI_APB_APB_EB                                             0x0000
#define REG_AI_APB_APB_RST                                            0x0004
#define REG_AI_APB_GEN_CLK_CFG                                        0x0008
#define REG_AI_APB_POWERVR_INT_STATUS                                 0x000C
#define REG_AI_APB_LPC_CFG_MTX_M0                                     0x0010
#define REG_AI_APB_LPC_CFG_MTX_S0                                     0x0014
#define REG_AI_APB_LPC_CFG_MTX_S1                                     0x0018
#define REG_AI_APB_LPC_CFG_MTX_S2                                     0x001C
#define REG_AI_APB_LPC_MAIN_MTX_M0                                    0x0020
#define REG_AI_APB_LPC_MAIN_MTX_M1                                    0x0024
#define REG_AI_APB_LPC_MAIN_MTX_S0                                    0x0028
#define REG_AI_APB_LPC_MAIN_MTX_S1                                    0x002C
#define REG_AI_APB_LPC_MAIN_MTX_MAIN                                  0x0030
#define REG_AI_APB_LPC_AB_TO_DDR                                      0x0034
#define REG_AI_APB_ASYNC_BRIDGE_TO_DDR                                0x0038
#define REG_AI_APB_AI_QOS_CTRL                                        0x003C
#define REG_AI_APB_LPC_CTRL                                           0x0040
#define REG_AI_APB_USER_GATE_FORCE_OFF                                0x0044
#define REG_AI_APB_USER_GATE_AUTO_GATE_EN                             0x0048
#define REG_AI_APB_AXI_BUSMON_CTRL                                    0x004C
#define REG_AI_APB_ASYNC_BRIDGE_DEBUG_BUS_R                           0x0050
#define REG_AI_APB_ASYNC_BRIDGE_DEBUG_BUS_W                           0x0054

/*********************************** ai_clk REGS, phy_addr=0x27004000 ************************************/
#define REG_AI_CLK_CGM_POWERVR_DIV_CFG                                0x0024
#define REG_AI_CLK_CGM_POWERVR_SEL_CFG                                0x0028
#define REG_AI_CLK_CGM_MAIN_MTX_DIV_CFG                               0x0030
#define REG_AI_CLK_CGM_MAIN_MTX_SEL_CFG                               0x0034
#define REG_AI_CLK_CGM_CFG_MTX_DIV_CFG                                0x003C
#define REG_AI_CLK_CGM_CFG_MTX_SEL_CFG                                0x0040
#define REG_AI_CLK_CGM_OCM_DIV_CFG                                    0x0048
#define REG_AI_CLK_CGM_OCM_SEL_CFG                                    0x004C
#define REG_AI_CLK_CGM_DVFS_SEL_CFG                                   0x0058

/*********************************** ai_mtx REGS, phy_addr=0x2700C000 ************************************/
#define REG_AI_MTX_REGU_OT_CTRL_EN                                    0x0000
#define REG_AI_MTX_REGU_OT_CTRL_AW_CFG                                0x0004
#define REG_AI_MTX_REGU_OT_CTRL_AR_CFG                                0x0008
#define REG_AI_MTX_REGU_OT_CTRL_AX_CFG                                0x000C
#define REG_AI_MTX_REGU_LAT_EN                                        0x0010
#define REG_AI_MTX_REGU_LAT_W_CFG                                     0x0014
#define REG_AI_MTX_REGU_LAT_R_CFG                                     0x0018
#define REG_AI_MTX_REGU_LAT_STATUS                                    0x001C
#define REG_AI_MTX_REGU_BW_NRT_EN                                     0x0040
#define REG_AI_MTX_REGU_BW_NRT_W_CFG_0                                0x0044
#define REG_AI_MTX_REGU_BW_NRT_W_CFG_1                                0x0048
#define REG_AI_MTX_REGU_BW_NRT_R_CFG_0                                0x004C
#define REG_AI_MTX_REGU_BW_NRT_R_CFG_1                                0x0050
#define REG_AI_MTX_REGU_BW_NRT_STATUS                                 0x0054
#define REG_AI_MTX_REGU_AXQOS_GEN_EN                                  0x0060
#define REG_AI_MTX_REGU_AXQOS_GEN_CFG                                 0x0064
#define REG_AI_MTX_REGU_URG_CNT_CFG                                   0x0068
#define REG_AI_MTX_REGU_URG_CNT_VALUE                                 0x006C

/*********************************** ai_busmon REGS, phy_addr=0x27010000 ************************************/
#define REG_AI_BUSMON_CHN_CFG                                         0x0000
#define REG_AI_BUSMON_PEAK_WIN_LEN                                    0x0004
#define REG_AI_BUSMON_F_DN_RBW_SET                                    0x0008
#define REG_AI_BUSMON_F_DN_RLATENCY_SET                               0x000C
#define REG_AI_BUSMON_F_DN_WBW_SET                                    0x0010
#define REG_AI_BUSMON_F_DN_WLATENCY_SET                               0x0014
#define REG_AI_BUSMON_F_UP_RBW_SET                                    0x0018
#define REG_AI_BUSMON_F_UP_RLATENCY_SET                               0x001C
#define REG_AI_BUSMON_F_UP_WBW_SET                                    0x0020
#define REG_AI_BUSMON_F_UP_WLATENCY_SET                               0x0024
#define REG_AI_BUSMON_RTRANS_IN_WIN                                   0x0028
#define REG_AI_BUSMON_RBW_IN_WIN                                      0x002C
#define REG_AI_BUSMON_RLATENCY_IN_WIN                                 0x0030
#define REG_AI_BUSMON_WTRANS_IN_WIN                                   0x0034
#define REG_AI_BUSMON_WBW_IN_WIN                                      0x0038
#define REG_AI_BUSMON_WLATENCY_IN_WIN                                 0x003C
#define REG_AI_BUSMON_PEAKBW_IN_WIN                                   0x0040
