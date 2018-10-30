#ifndef _VSP_COMMON_H
#define _VSP_COMMON_H

#include <linux/semaphore.h>
#include <uapi/video/sprd_vsp.h>

extern struct regmap *gpr_aon_apb;
extern struct regmap *gpr_mm_ahb;
extern struct regmap *gpr_pmu_apb;
extern struct regmap *gpr_com_pmu_apb;
extern unsigned int codec_instance_count[VSP_CODEC_INSTANCE_COUNT_MAX];

struct vsp_fh {
	int is_vsp_aquired;
	int is_clock_enabled;

	wait_queue_head_t wait_queue_work;
	int condition_work;
	int vsp_int_status;
	unsigned int codec_id;
};

struct sprd_vsp_cfg_data {
	unsigned int version;
	unsigned int max_freq_level;
	unsigned int softreset_reg_offset;
	unsigned int reset_mask;
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
	struct clk *clk_vsp_ckg;
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
};

struct clock_name_map_t {
	unsigned long freq;
	char *name;
	struct clk *clk_parent;
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
int vsp_get_iova(struct vsp_dev_t *vsp_hw_dev,
		 struct vsp_iommu_map_data *mapdata, void __user *arg);
int vsp_free_iova(struct vsp_dev_t *vsp_hw_dev,
		  struct vsp_iommu_map_data *ummapdata);
#endif
