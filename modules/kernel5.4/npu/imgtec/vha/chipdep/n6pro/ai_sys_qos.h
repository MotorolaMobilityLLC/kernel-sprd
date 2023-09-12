#ifndef __AI_SYS_QOS_H_
#define __AI_SYS_QOS_H_

typedef struct _QOS_REG_STRUCT
{
	const char    *reg_name;
	uint32_t      base_addr;
	uint32_t      mask_value;
	uint32_t      set_value;

} QOS_REG_T;

QOS_REG_T nic400_ai_main_mtx_m0_qos_list[] = {

	{ "REGU_OT_CTRL_EN",                0x2700C000, 0x00000001, 0x00000001},
	{ "REGU_OT_CTRL_AW_CFG",            0x2700C004, 0xffffffff, 0x0a080808},
	{ "REGU_OT_CTRL_AR_CFG",            0x2700C008, 0x3f3f3f3f, 0x3f3f3f3f},
	{ "REGU_OT_CTRL_Ax_CFG",            0x2700C00C, 0x3f3fffff, 0x3f3f1010},
	{ "REGU_LAT_W_CFG",                 0x2700C014, 0xffffffff, 0x00000000},
	{ "REGU_LAT_R_CFG",                 0x2700C018, 0xffffffff, 0x00000000},
	{ "REGU_AXQOS_GEN_EN",              0x2700C060, 0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",             0x2700C064, 0x3fff3fff, 0x06660666},
	{ "REGU_URG_CNT_CFG",               0x2700C068, 0x00000701, 0x00000001},
	{ "end",                            0x00000000, 0x00000000, 0x00000000}
};

QOS_REG_T ai_apb_rf_qos_list[] = {

	{ "LPC_MAIN_MTX_M0",                0x27000020, 0x00010000, 0x00000000},
	{ "LPC_MAIN_MTX_M0",                0x27000020, 0x00010000, 0x00010000},
	{ "LPC_MAIN_MTX_M1",                0x27000024, 0x00010000, 0x00000000},
	{ "LPC_MAIN_MTX_M1",                0x27000024, 0x00010000, 0x00010000},
	{ "end",                            0x00000000, 0x00000000, 0x00000000}
};

#endif
