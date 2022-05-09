/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc UMS512 VSP driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#ifndef _VSP_COMMON_H
#define _VSP_COMMON_H

#include <linux/semaphore.h>
#include <uapi/video/sprd_vsp.h>
#include <linux/dma-buf.h>
#include <linux/mutex.h>

#define VSP_MINOR MISC_DYNAMIC_MINOR
#define VSP_AQUIRE_TIMEOUT_MS	500
#define VSP_INIT_TIMEOUT_MS	200
#define DEFAULT_FREQ_DIV	0x0

#define ARM_INT_STS_OFF		0x10
#define ARM_INT_MASK_OFF	0x14
#define ARM_INT_CLR_OFF		0x18
#define ARM_INT_RAW_OFF		0x1c

#define VSP_INT_STS_OFF		0x0
#define VSP_INT_MASK_OFF	0x04
#define VSP_INT_CLR_OFF		0x08
#define VSP_INT_RAW_OFF		0x0c
#define VSP_AXI_STS_OFF		0x1c

#define VSP_MMU_INT_STS_OFF	0x60
#define VSP_MMU_INT_MASK_OFF	0x6c
#define VSP_MMU_INT_CLR_OFF	0x64
#define VSP_MMU_INT_RAW_OFF	0x68

#define BSM_BUF_OVF_ERR		BIT(0)
#define VLD_ERR			BIT(4)
#define TIMEOUT_ERR		BIT(5)
#define MMU_RD_WR_ERR		0xff

struct vsp_fh {
	int is_vsp_acquired;
	int is_clock_enabled;

	wait_queue_head_t wait_queue_work;
	int condition_work;
	int vsp_int_status;
	int codec_id;
};

struct sprd_vsp_cfg_data {
	unsigned int version;
	unsigned int max_freq_level;
	unsigned int qos_reg_offset;
};

struct vsp_dev_t {
	unsigned int freq_div;
	unsigned int scene_mode;

	struct semaphore vsp_mutex;
	struct sprd_vsp_cfg_data *vsp_cfg_data;
	struct clk *vsp_clk;
	struct clk *vsp_parent_clk;
	struct clk *vsp_parent_df_clk;
	struct clk *ahb_parent_clk;
	struct clk *ahb_parent_df_clk;
	struct clk *emc_parent_clk;
	struct clk *clk_mm_eb;
	struct clk *clk_vsp_mq_ahb_eb;
	struct clk *clk_axi_gate_vsp;
	struct clk *clk_ahb_gate_vsp_eb;
	struct clk *clk_ahb_vsp;
	struct clk *clk_emc_vsp;
	struct clk *clk_vsp_ahb_mmu_eb;

	unsigned int irq;
	unsigned int version;

	struct vsp_fh *vsp_fp;
	struct device_node *dev_np;
	struct device *vsp_dev;
	bool light_sleep_en;
	bool iommu_exist_flag;
	bool vsp_qos_exist_flag;

	struct mutex map_lock;
	struct list_head map_list;
};

struct clock_name_map_t {
	unsigned long freq;
	char *name;
	struct clk *clk_parent;
};
enum {
	VSP_DOMAIN_EB,
	RESET
};
struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};
static char *tb_name[] = {
	"vsp-domain-eb-syscon",
	"reset-syscon"
};
extern struct register_gpr regs[ARRAY_SIZE(tb_name)];

struct vsp_qos_cfg {
	u8 awqos;
	u8 arqos_high;
	u8 arqos_low;
	unsigned int reg_offset;
};

struct clk *vsp_get_clk_src_name(struct clock_name_map_t clock_name_map[],
				unsigned int freq_level,
				unsigned int max_freq_level);
int find_vsp_freq_level(struct clock_name_map_t clock_name_map[],
			unsigned long freq,
			unsigned int max_freq_level);
int vsp_clk_enable(struct vsp_dev_t *vsp_hw_dev);
void vsp_clk_disable(struct vsp_dev_t *vsp_hw_dev);
int vsp_get_mm_clk(struct vsp_dev_t *vsp_hw_dev);
#ifdef CONFIG_COMPAT
long compat_vsp_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg);
#endif
int vsp_get_iova(void *inst_ptr, struct vsp_dev_t *vsp_hw_dev,
		 struct vsp_iommu_map_data *mapdata, void __user *arg);
int vsp_free_iova(void *inst_ptr, struct vsp_dev_t *vsp_hw_dev,
		  struct vsp_iommu_map_data *ummapdata);
void non_free_bufs_check(void *inst_ptr, struct vsp_dev_t *vsp_hw_dev);

int vsp_handle_interrupt(struct vsp_dev_t *vsp_hw_dev, int *status,
	void __iomem *sprd_vsp_base, void __iomem *vsp_glb_reg_base);
void vsp_clr_interrupt_mask(struct vsp_dev_t *vsp_hw_dev,
	void __iomem *sprd_vsp_base, void __iomem *vsp_glb_reg_base);
int vsp_get_dmabuf(int fd, struct dma_buf **dmabuf,
	void **buf, size_t *size);
#endif
