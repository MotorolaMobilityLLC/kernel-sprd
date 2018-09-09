#ifndef _PAM_IPA_CORE_H_
#define _PAM_IPA_CORE_H_

#include <linux/sipa.h>
#include <linux/skbuff.h>

#define PAM_AKB_BUF_SIZE	1664
#define PAM_FREE_FIFO_SIZE	1024

#define PAM_IPA_GET_LOW32(val) \
			((u32)(val & 0x00000000FFFFFFFF))
#define PAM_IPA_GET_HIGH32(val) \
			((u32)((val >> 32) & 0x00000000FFFFFFFF))
#define PAM_IPA_STI_64BIT(l_val, h_val) \
			((u64)(l_val | ((u64)h_val << 32)))

struct pam_ipa_hal_proc_tag {
	u32 (*init_pcie_ul_fifo_base)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_dl_fifo_base)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_ul_fifo_base)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_dl_fifo_base)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_ul_fifo_sts_addr)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_dl_fifo_sts_addr)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_dl_fifo_sts_addr)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_ul_fifo_sts_addr)(
		void __iomem *reg_base,
		u32 free_addrl, u32 free_addrh,
		u32 filled_addrl, u32 filled_addrh);
	u32 (*set_ddr_mapping)(
		void __iomem *reg_base,
		u32 offset_l, u32 offset_h);
	u32 (*set_pcie_rc_base)(
		void __iomem *reg_base,
		u32 offset_l, u32 offset_h);
	u32 (*start)(void __iomem *reg_base);
	u32 (*stop)(void __iomem *reg_base);
	u32 (*resume)(void __iomem *reg_base, u32 flag);

	u64 (*get_ddr_mapping)(void);
	u64 (*get_pcie_rc_base)(void);
};

struct pam_ipa_cfg_tag {
	void __iomem *reg_base;
	struct resource pam_ipa_res;

	u32 connect;
	u64 pcie_offset;
	u64 pcie_rc_base;

	struct sipa_connect_params
			pam_local_param;
	struct sipa_connect_params
			pam_remote_param;

	struct sipa_to_pam_info local_cfg;
	struct sipa_to_pam_info remote_cfg;

	struct sk_buff *skb1;
	struct sk_buff *skb2;
	struct sk_buff *skb3;
	struct sk_buff *skb4;
	dma_addr_t dma_addr_buf[PAM_FREE_FIFO_SIZE];

	struct pam_ipa_hal_proc_tag hal_ops;
};

extern u32 pam_ipa_init_api(struct pam_ipa_hal_proc_tag *ops);
extern u32 pam_ipa_init(struct pam_ipa_cfg_tag *cfg);

#endif
