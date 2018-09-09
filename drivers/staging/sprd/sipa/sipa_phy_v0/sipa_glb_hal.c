#include "../sipa_hal_priv.h"
#include "sipa_glb_phy.h"

static u32 sipa_hal_set_work_mode(
	void __iomem *reg_base,
	u32 is_bypass)
{
	u32 ret = 0;

	ret = ipa_phy_set_work_mode(
			  reg_base, is_bypass);

	return ret;
}


static u32 sipa_hal_enable_cp_through_pcie(
	void __iomem *reg_base,
	u32 enable)
{
	u32 ret = 0;

	ret = ipa_phy_enable_cp_through_pcie(
			  reg_base, enable);

	return ret;
}

static u32 sipa_hal_get_interrupt_status(
	void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_int_status(reg_base);

	return ret;
}

static u32 sipa_hal_ctrl_ipa_action(
	void __iomem *reg_base,
	u32 enable)
{
	u32 ret = 0;

	ret = ipa_phy_ctrl_ipa_action(
			  reg_base, enable);

	return ret;
}

static u32 sipa_hal_get_hw_ready_to_check_sts(
	void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_hw_ready_to_check_sts(
			  reg_base);

	return ret;
}

static u32 sipa_hal_hash_table_switch(
	void __iomem *reg_base,
	u32 addr_l, u32 addr_h,
	u32 len)
{
	u32 ret = 0;

	ret = ipa_phy_hash_table_switch(reg_base,
									addr_l, addr_h, len);

	return ret;
}

static u32 sipa_hal_get_hash_table(
	void __iomem *reg_base,
	u32 *addr_l, u32 *addr_h,
	u32 *len)
{
	u32 ret = 0;

	ret = ipa_phy_get_hash_table(reg_base,
								 addr_l, addr_h, len);

	return ret;
}

static u32 sipa_hal_map_interrupt_src_en(
	void __iomem *reg_base,
	u32 enable, u32 mask)
{
	u32 ret = 0;

	ret = ipa_phy_map_interrupt_src_en(reg_base,
									   enable, mask);

	return ret;
}

static u32 sipa_hal_clear_internal_fifo(
	void __iomem *reg_base,
	u32 clr_bit)
{
	u32 ret = 0;

	ret = ipa_phy_clear_internal_fifo(
			  reg_base, clr_bit);

	return ret;
}

static u32 sipa_hal_set_flow_ctrl_to_src_blk(
	void __iomem *reg_base,
	u32 dst, u32 src)
{
	u32 ret = 0;
	u32 *dst_ptr = NULL;

	dst_ptr = (u32 *)((u64 *)(u64)dst);

	ret = ipa_phy_set_flow_ctrl_to_src_blk(reg_base,
										   dst_ptr, src);

	return ret;
}

static u32 sipa_hal_enable_def_flowctrl_to_src_blk(
	void __iomem *reg_base)
{
	u32 ret = 0;

	ret = sipa_hal_set_flow_ctrl_to_src_blk(reg_base,
											IPA_WIFI_WIAP_DL_FLOWCTL_SRC,
											(u32)WIAP_DL_SRC_BLK_MAP_UL);
	ret = sipa_hal_set_flow_ctrl_to_src_blk(reg_base,
											IPA_USB_SDIO_FLOWCTL_SRC,
											(u32)USB_DL_SRC_BLK_MAP_DL);

	return ret;
}

static u32 sipa_hal_get_flow_ctrl_to_src_sts(
	void __iomem *reg_base,
	u32 dst, u32 src)
{
	u32 ret = 0;
	u32 *dst_ptr = NULL;

	dst_ptr = (u32 *)((u64 *)(u64)dst);

	ret = ipa_phy_get_flow_ctrl_to_src_sts(
			  reg_base, dst_ptr, src);

	return ret;
}

static u32 sipa_hal_set_axi_mst_chn_priority(
	void __iomem *reg_base,
	u32 chan, u32 prio)
{
	u32 ret = 0;

	ret = ipa_phy_set_axi_mst_chn_priority(
			  reg_base, chan, prio);

	return ret;
}

static u32 sipa_hal_get_timestamp(
	void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_timestamp(reg_base);

	return ret;
}

static u32 sipa_hal_set_force_to_ap_flag(
	void __iomem *reg_base,
	u32 enable, u32 bit)
{
	u32 ret = 0;

	ret = ipa_phy_set_force_to_ap_flag(reg_base, enable, bit);

	return ret;
}

u32 sipa_glb_ops_init(
	struct sipa_hal_global_ops *ops)
{
	ops->set_mode					=
		sipa_hal_set_work_mode;
	ops->clear_internal_fifo		=
		sipa_hal_clear_internal_fifo;
	ops->ctrl_ipa_action			=
		sipa_hal_ctrl_ipa_action;
	ops->get_flow_ctrl_to_src_sts	=
		sipa_hal_get_flow_ctrl_to_src_sts;
	ops->get_hw_ready_to_check_sts	=
		sipa_hal_get_hw_ready_to_check_sts;
	ops->get_int_status				=
		sipa_hal_get_interrupt_status;
	ops->hash_table_switch			=
		sipa_hal_hash_table_switch;
	ops->get_hash_table				=
		sipa_hal_get_hash_table;
	ops->map_interrupt_src_en		=
		sipa_hal_map_interrupt_src_en;
	ops->set_axi_mst_chn_priority	=
		sipa_hal_set_axi_mst_chn_priority;
	ops->set_flow_ctrl_to_src_blk	=
		sipa_hal_set_flow_ctrl_to_src_blk;
	ops->get_timestamp				=
		sipa_hal_get_timestamp;
	ops->set_force_to_ap			=
		sipa_hal_set_force_to_ap_flag;
	ops->enable_cp_through_pcie		=
		sipa_hal_enable_cp_through_pcie;

	ops->enable_def_flowctrl_to_src_blk =
		sipa_hal_enable_def_flowctrl_to_src_blk;

	return TRUE;
}
