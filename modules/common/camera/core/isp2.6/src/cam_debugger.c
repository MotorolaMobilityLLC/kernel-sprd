/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <sprd_mm.h>

#include "cam_debugger.h"
#include "dcam_reg.h"
#include "dcam_path.h"
#include "isp_core.h"
#include "isp_cfg.h"
#include "isp_reg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_DEBUG: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define DCAM_DEBUG
#define WORK_MODE_SLEN  2
#define LBUF_LEN_SLEN   8

uint32_t g_dcam_bypass[DCAM_HW_CONTEXT_MAX] = {0};
struct cam_dbg_dump g_dbg_dump;
int s_dbg_work_mode = ISP_CFG_MODE;
uint64_t g_isp_bypass[ISP_CONTEXT_SW_NUM] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int g_dbg_iommu_mode = IOMMU_AUTO;
int g_dbg_set_iommu_mode = IOMMU_AUTO;
uint32_t g_pyr_dec_online_bypass = 0;
uint32_t g_pyr_dec_offline_bypass = 0;
uint32_t g_dcam_raw_src = PROCESS_RAW_SRC_SEL;
uint32_t g_dbg_dumpswitch = 0;
uint32_t g_dbg_recovery = 0;
uint32_t g_dbg_fbc_control = 0;
uint32_t contr_cap_eof = 1;
extern atomic_t s_dcam_opened[DCAM_SW_CONTEXT_MAX];
extern struct isp_pipe_dev *s_isp_dev;
extern uint32_t s_dbg_linebuf_len;

static struct dentry *debugfs_base;
static uint32_t debug_ctx_id[4] = {0, 1, 2, 3};
static struct dentry *s_p_dentry;

/* dcam debugfs start */
#ifdef DCAM_DEBUG

/*
 * dcam sub block bypass
 * How: echo 4in1:1 > bypass_dcam0
 */
static ssize_t camdebugger_dcam_bypass_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	struct seq_file *p = (struct seq_file *)filp->private_data;
	struct cam_debug_bypass *debug_bypass = NULL;
	struct cam_hw_info *ops = NULL;
	struct cam_hw_bypass_data data;
	uint32_t type;
	uint32_t idx = 0;
	uint32_t bypass_cnt = 0;
	char buf[256];
	uint32_t val = 1; /* default bypass */
	uint32_t i;
	char name[16];
	int bypass_all = 0;

	debug_bypass = (struct cam_debug_bypass *)p->private;
	idx = debug_bypass->idx;
	ops = debug_bypass->hw;
	if (atomic_read(&s_dcam_opened[idx]) <= 0) {
		pr_info("dcam%d Hardware not enable\n", idx);
		return count;
	}
	memset(buf, 0x00, sizeof(buf));
	i = count;
	if (i >= sizeof(buf))
		i = sizeof(buf) - 1; /* last one for \0 */
	if (copy_from_user(buf, buffer, i)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}
	buf[i] = '\0';
	/* get name */
	for (i = 0; i < sizeof(name) - 1; i++) {
		if (' ' == buf[i] || ':' == buf[i] ||
			',' == buf[i])
			break;
		if (buf[i] >= 'A' && buf[i] <= 'Z')
			buf[i] += ('a' - 'A');
		name[i] = buf[i];
	}
	name[i] = '\0';
	/* get val */
	for (; i < sizeof(buf); i++) {
		if (buf[i] >= '0' && buf[i] <= '9') {
			val = buf[i] - '0';
			break;
		}
	}
	val = val != 0 ? 1 : 0;
	/* find */
	if (strcmp(name, "all") == 0)
		bypass_all = 1;

	type = DCAM_BYPASS_TYPE;
	bypass_cnt = ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_COUNT_GET, &type);
	data.type = DCAM_BYPASS_TYPE;

	for (i = 0; i < bypass_cnt; i++) {
		data.i = i;
		ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_DATA_GET, &data);
		if (data.tag == NULL)
			continue;

		if (data.tag->p == NULL)
			continue;
		if (strcmp(data.tag->p, name) == 0 || bypass_all){
			printk("set dcam%d addr 0x%x, bit %d val %d\n",
				idx, data.tag->addr, data.tag->bpos, val);
			g_dcam_bypass[idx] &= (~(1 << i));
			g_dcam_bypass[idx] |= (val << i);
			msleep(20); /* If PM writing,wait little time */
			DCAM_REG_MWR(idx, data.tag->addr, 1 << data.tag->bpos,
				val << data.tag->bpos);
			/* afl need rgb2y work */
			if (strcmp(name, "afl") == 0)
				DCAM_REG_MWR(idx, ISP_AFL_PARAM0,
					BIT_0, val);
			if (!bypass_all)
				break;
		}
	}
	/* not opreate */
	if ((!bypass_all) && i >= bypass_cnt)
		pr_info("Not operate, dcam%d,name:%s val:%d\n",
			idx, name, val);

	return count;
}

static int camdebugger_dcam_bypass_read(struct seq_file *s, void *unused)
{
	uint32_t addr, val;
	struct cam_debug_bypass *debug_bypass = NULL;
	struct cam_hw_info *ops = NULL;
	struct cam_hw_bypass_data data;
	uint32_t type;
	uint32_t idx = 0;
	uint32_t bypass_cnt = 0;
	uint32_t i = 0;

	if (!s) {
		pr_err("fail to get valid param\n");
		return -EFAULT;
	}

	debug_bypass = (struct cam_debug_bypass *)s->private;
	idx = debug_bypass->idx;
	ops = debug_bypass->hw;
	data.type = DCAM_BYPASS_TYPE;
	type = DCAM_BYPASS_TYPE;
	bypass_cnt = ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_COUNT_GET, &type);
	seq_printf(s, "-----dcam%d-----\n", idx);
	if (atomic_read(&s_dcam_opened[idx]) <= 0) {
		seq_puts(s, "Hardware not enable\n");
	} else {
		for (i = 0; i < bypass_cnt; i++) {
			data.i = i;
			ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_DATA_GET, &data);
			if (data.tag == NULL)
				continue;
			if (data.tag->p == NULL)
				continue;
			addr = data.tag->addr;
			val = DCAM_REG_RD(idx, addr) & (1 << data.tag->bpos);
			if (val)
				seq_printf(s, "%s:bit%d=1 bypass\n",
					data.tag->p, data.tag->bpos);
			else
				seq_printf(s, "%s:bit%d=0  work\n",
					data.tag->p, data.tag->bpos);
		}
		seq_puts(s, "\nall:1 #to bypass all\n\n");
		seq_puts(s, "Example: make 4in1 module bypass(1) or work(0)\n");
		seq_puts(s, "bypass: echo 4in1:1 > bypass_dcam0\n");
		seq_puts(s, "work: echo 4in1:0 > bypass_dcam0\n\n");
	}
	return 0;
}

static int camdebugger_dcam_bypass_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, camdebugger_dcam_bypass_read, inode->i_private);
}

static const struct file_operations bypass_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_dcam_bypass_open,
	.read = seq_read,
	.write = camdebugger_dcam_bypass_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* read dcamx register once */
static int camdebugger_dcam_reg_show(struct seq_file *s,
	void *unused)
{
	uint32_t idx = *(uint32_t *)s->private;
	uint32_t addr;
	const uint32_t addr_end[] = {0x400, 0x400, 0x110, 0x110, 0x100, 0x100};


	if (idx >= 3 && idx < 5) {
		seq_printf(s, "-----dcam axi%d and fetch----\n", idx - 3);
		if (atomic_read(&s_dcam_opened[idx]) <= 0) {
			seq_puts(s, "Hardware not enable\n");
			return 0;
		}

		for (addr = 0; addr < addr_end[idx]; addr += 4)
			seq_printf(s, "0x%04x: 0x%08x\n",
				addr,  REG_RD(g_dcam_aximbase[idx - 3] + addr));

		seq_puts(s, "--------------------\n");
	} else {
		seq_printf(s, "-----dcam%d----------\n", idx);
		if (atomic_read(&s_dcam_opened[idx]) <= 0) {
			seq_puts(s, "Hardware not enable\n");

			return 0;
		}
		for (addr = 0; addr < addr_end[idx]; addr += 4)
			seq_printf(s, "0x%04x: 0x%08x\n",
				addr, DCAM_REG_RD(idx, addr));

		seq_puts(s, "--------------------\n");
	}

	return 0;
}

static int camdebugger_dcam_reg_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, camdebugger_dcam_reg_show, inode->i_private);
}

static const struct file_operations dcam_reg_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_dcam_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static char zoom_mode_strings[6][8] = {
	"bypass", "bin2", "bin4", "rds", "adapt", "scaler"};

static ssize_t camdebugger_zoom_mode_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[256];
	const char *desc = "0: bypass, 1: bin2, 2: bin4, 3: rds, 4: adapt, 5: scaler";

	if (g_camctrl.dcam_zoom_mode >= ZOOM_DEBUG_DEFAULT)
		snprintf(buf, sizeof(buf), "%d(%s)\n%s\n", g_camctrl.dcam_zoom_mode,
			zoom_mode_strings[g_camctrl.dcam_zoom_mode - ZOOM_DEBUG_DEFAULT], desc);
	else
		snprintf(buf, sizeof(buf), "%d(%s)\n%s\n", g_camctrl.dcam_zoom_mode,
			zoom_mode_strings[g_camctrl.dcam_zoom_mode], desc);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_zoom_mode_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int val;

	if (count > 2)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[1] = '\0';
	val = simple_strtol(msg, &last, 0);
	if (val == 0)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_DEFAULT;
	else if (val == 1)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_BINNING2;
	else if (val == 2)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_BINNING4;
	else if (val == 3)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_RDS;
	else if (val == 4)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_ADAPTIVE;
	else if (val == 5)
		g_camctrl.dcam_zoom_mode = ZOOM_DEBUG_SCALER;
	else
		pr_err("fail to get valid zoom mode: %d\n", val);

	if (g_camctrl.dcam_zoom_mode >= ZOOM_DEBUG_DEFAULT)
		pr_info("set zoom mode %d(%s)\n", g_camctrl.dcam_zoom_mode,
			zoom_mode_strings[g_camctrl.dcam_zoom_mode - ZOOM_DEBUG_DEFAULT]);
	else
		pr_info("set zoom mode %d(%s)\n", g_camctrl.dcam_zoom_mode,
			zoom_mode_strings[g_camctrl.dcam_zoom_mode]);
	return count;
}

static const struct file_operations zoom_mode_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_zoom_mode_show,
	.write = camdebugger_zoom_mode_write,
};

static ssize_t camdebugger_rds_limit_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", g_camctrl.dcam_rds_limit);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_rds_limit_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int val;

	if (count > 3)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[2] = '\0';
	val = simple_strtol(msg, &last, 0);
	if (val >= 10 && val <= 40)
		g_camctrl.dcam_rds_limit = val;
	else
		pr_err("fail to get valid rds limit: %d\n", val);

	pr_info("set rds limit %d\n", g_camctrl.dcam_rds_limit);
	return count;
}

static const struct file_operations rds_limit_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_rds_limit_show,
	.write = camdebugger_rds_limit_write,
};

static ssize_t camdebugger_userparam_set(const char __user *buffer, size_t count, uint32_t *val)
{
	int ret = 0;
	char msg[8];

	if (count > 2)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[count] = '\0';
	ret = kstrtouint(msg, 10, val);
	if (ret < 0) {
		pr_err("fail to convert '%s', ret %d\n", msg, ret);
		return ret;
	}
	return ret;
}

static int camdebugger_fbc_control_read(struct seq_file *s, void *unused)
{
	const char *desc = "bit 0:bin 1:full 2:raw\n";
	char buf[48];

	snprintf(buf, sizeof(buf), "%u\n\n%s\n", g_dbg_fbc_control, desc);

	seq_printf(s, "\nUsage:\n");
	seq_printf(s, "         echo val > fbc_contrl\n");
	seq_printf(s, "The different bits represent fbc switch in different path\n");
	seq_printf(s, "\nExample:\n");
	seq_printf(s, "         echo 6 > fbc_contrl   // bit 110, bypass full and raw path fbc\n");
	seq_printf(s, "         echo 3 > fbc_contrl   // bit 011, bypass bin and full path fbc\n");

	return 0;
}

static int camdebugger_fbc_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, camdebugger_fbc_control_read, inode->i_private);
}

static ssize_t camdebugger_fbc_control_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	ret = camdebugger_userparam_set(buffer,count,&g_dbg_fbc_control);
	if (ret < 0) {
		pr_err("set fbc_control fail\n");
		return ret;
	}
	pr_info("set fbc_control %u\n", g_dbg_fbc_control);
	return count;
}

static const struct file_operations fbc_control_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_fbc_control_open,
	.read = seq_read,
	.write = camdebugger_fbc_control_write,
};

static ssize_t camdebugger_recovery_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	const char *desc = "0: open, 1: bypass recovery\n";
	char buf[48];

	snprintf(buf, sizeof(buf), "%u\n\n%s\n", g_dbg_recovery, desc);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_recovery_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	ret = camdebugger_userparam_set(buffer,count,&g_dbg_recovery);
	if (ret < 0) {
		pr_err("set recovery fail");
		return ret;
	}
	pr_info("set recovery %u\n", g_dbg_recovery);
	return count;
}

static const struct file_operations recovery_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_recovery_show,
	.write = camdebugger_recovery_write,
};

static ssize_t camdebugger_dumpswitch_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	const char *desc = "0: disable, 1: bin path and raw path\n";
	char buf[48];

	snprintf(buf, sizeof(buf), "%u\n\n%s\n", g_dbg_dumpswitch, desc);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_dumpswitch_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	ret = camdebugger_userparam_set(buffer,count,&g_dbg_dumpswitch);
	if (ret < 0) {
		pr_err("set fbc_control fail");
		return ret;
	}
	pr_info("set pathswitch %u\n", g_dbg_dumpswitch);
	return count;
}

static const struct file_operations dumpswitch_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_dumpswitch_show,
	.write = camdebugger_dumpswitch_write,
};

static ssize_t camdebugger_dump_raw_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	const char *desc = "0:disable, 1:bin&full, 2:full, 3:bin, 4:rec, 5:offline_dec, 6:pdaf, 7:bin & raw, 8:high res";
	char buf[256];
	char *str = "Example: echo 0 > dump_raw_en";

	snprintf(buf, sizeof(buf), "%u\n\n%s\n\n%s\n\n", g_dbg_dump.dump_en, desc, str);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_dump_raw_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	uint32_t val;

	if (count > 2)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[count] = '\0';
	ret = kstrtouint(msg, 10, &val);
	if (ret < 0) {
		pr_err("fail to convert '%s', ret %d", msg, ret);
		return ret;
	}

	g_dbg_dump.dump_en = val;
	pr_info("set dump_raw_en %u\n", g_dbg_dump.dump_en);

	return count;
}

static const struct file_operations dump_raw_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_dump_raw_show,
	.write = camdebugger_dump_raw_write,
};

static ssize_t camdebugger_dump_count_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int val;
	int i = 0;
	struct cam_dbg_dump *dbg = &g_dbg_dump;
	unsigned long flag = 0;

	if ((dbg->dump_en == 0) || count > 3)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[3] = '\0';
	val = simple_strtol(msg, &last, 0);

	/* for preview dcam raw dump frame count. */
	/* dump thread will be trigged when this value is set. */
	/* valid value: 1 ~ 99 */
	/* if dump thread is ongoing, new setting will not be accepted. */
	/* capture raw dump will be triggered when catpure starts. */
	spin_lock_irqsave(&dbg->dump_lock, flag);
	dbg->dump_count = 0;
	if (val >= 200 || val == 0) {
		pr_err("fail to get dump_raw_count %d\n", val);
	} else if (dbg->dump_ongoing == 0) {
		dbg->dump_count = val;
		while(i < DCAM_ID_MAX) {
			if (dbg->dump_start[i]) {
				complete(dbg->dump_start[i]);
				pr_debug("set dump_raw_count %d\n", dbg->dump_count);
				continue;
			}
			i++;
		}
	}
	spin_unlock_irqrestore(&dbg->dump_lock, flag);

	return count;
}

static const struct file_operations dump_count_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = camdebugger_dump_count_write,
};

static ssize_t camdebugger_csi_switch_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	pr_info("msg : %s\n", msg);

	return count;
}

static const struct file_operations csi_switch_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = camdebugger_csi_switch_write,
};

static ssize_t camdebugger_pyr_dec_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	const char *desc = "0: work, 1: bybass";
	char buf[64];
	char *str = "Example: echo 1 > pyr_dec_bypass";

	snprintf(buf, sizeof(buf), "%u\n\n%s\n\n%s\n\n", g_pyr_dec_online_bypass, desc ,str);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_pyr_dec_bypass(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	uint32_t val;

	if (count > 2)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[count] = '\0';
	ret = kstrtouint(msg, 10, &val);
	if (ret < 0) {
		pr_err("fail to convert '%s', ret %d", msg, ret);
		return ret;
	}

	g_pyr_dec_online_bypass = val;
	pr_info("set pyr_dec_online_bypass %u\n", g_pyr_dec_online_bypass);

	return count;
}

static const struct file_operations pyr_dec_online_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_pyr_dec_show,
	.write = camdebugger_pyr_dec_bypass,
};

static ssize_t camdebugger_dcam_raw_src_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[16] = {0};

	if (g_dcam_raw_src == ORI_RAW_SRC_SEL)
		snprintf(buf, sizeof(buf), "%s\n", "sensor");
	else if (g_dcam_raw_src == PROCESS_RAW_SRC_SEL)
		snprintf(buf, sizeof(buf), "%s\n", "rrgb");
	else if (g_dcam_raw_src == LSC_RAW_SRC_SEL)
		snprintf(buf, sizeof(buf), "%s\n", "lsc");
	else if (g_dcam_raw_src == BPC_RAW_SRC_SEL)
		snprintf(buf, sizeof(buf), "%s\n", "bpc");
	else if (g_dcam_raw_src == NLM_RAW_SRC_SEL)
		snprintf(buf, sizeof(buf), "%s\n", "nlm");
	else
		snprintf(buf, sizeof(buf), "%s\n", "wrong");

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t camdebugger_dcam_raw_src_set(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8] = {0};
	uint32_t val = 0;

	if (count > 7)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[count - 1] = '\0';
	if (strcmp(msg, "sensor") == 0)
		val = ORI_RAW_SRC_SEL;
	else if (strcmp(msg, "rrgb") == 0)
		val = PROCESS_RAW_SRC_SEL;
	else if (strcmp(msg, "lsc") == 0)
		val = LSC_RAW_SRC_SEL;
	else if (strcmp(msg, "bpc") == 0)
		val = BPC_RAW_SRC_SEL;
	else if (strcmp(msg, "nlm") == 0)
		val = NLM_RAW_SRC_SEL;
	else
		val = PROCESS_RAW_SRC_SEL;

	g_dcam_raw_src = val;
	pr_info("set dcam raw src %u\n", g_dcam_raw_src);

	return count;
}

static const struct file_operations dcam_raw_src_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_dcam_raw_src_read,
	.write = camdebugger_dcam_raw_src_set,
};

static ssize_t camdebugger_dcamint_eof_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%d\n", contr_cap_eof);
	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_dcamint_eof_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int x;
	if (count > 8)
		return -EINVAL;
	ret = copy_from_user(msg, (void __user *)buffer, count);
	for (x = 0; x < count-1; x++) {
		if (!(('0' <= msg[x]) && (msg[x] <= '9'))) {
			pr_err("fail to set the msg\n");
			return -EFAULT;
		}
	}
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}
	contr_cap_eof = simple_strtol(msg, &last, 0);
	pr_info("set contr_cap_eof value %d\n", contr_cap_eof);
	return count;
}

static const struct file_operations dcam_eof_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_dcamint_eof_read,
	.write = camdebugger_dcamint_eof_write,
};

/* /sys/kernel/debug/sprd_dcam/
 * dcam0_reg, dcam1_reg, dcam2_reg, dcam_axi_reg
 * dcam0/1/2_reg,dcam_axi_reg: cat .....(no echo > )
 * reg_dcam: echo 0xxxx > reg_dcam, then cat reg_dcam
 */
static int camdebugger_dcam_init(struct camera_debugger *debugger)
{
	/* folder in /sys/kernel/debug/ */
	const char tb_folder[] = {"sprd_dcam"};
	struct dentry *pd;
	static int tb_dcam_id[] = {0, 1, 2, 3};
	static struct cam_debug_bypass dcam_debug_bypass[2];
	int ret = 0;

	if (!debugger) {
		pr_err("fail to get valid para\n");
		return -EFAULT;
	}

	dcam_debug_bypass[0].idx = tb_dcam_id[0];
	dcam_debug_bypass[0].hw = debugger->hw;
	dcam_debug_bypass[1].idx = tb_dcam_id[1];
	dcam_debug_bypass[1].hw = debugger->hw;

	s_p_dentry = debugfs_create_dir(tb_folder, NULL);
	pd = s_p_dentry;
	if (pd == NULL)
		return -ENOMEM;
	/* sub block bypass */
	if (!debugfs_create_file("bypass_dcam0", 0660,
		pd, &dcam_debug_bypass[0], &bypass_ops))
		ret |= BIT(1);
	if (!debugfs_create_file("bypass_dcam1", 0660,
		pd, &dcam_debug_bypass[1], &bypass_ops))
		ret |= BIT(2);
	/* the same response Function, parameter differ */
	if (!debugfs_create_file("reg_dcam0", 0440,
		pd, &tb_dcam_id[0], &dcam_reg_ops))
		ret |= BIT(3);
	if (!debugfs_create_file("reg_dcam1", 0440,
		pd, &tb_dcam_id[1], &dcam_reg_ops))
		ret |= BIT(4);
	if (!debugfs_create_file("reg_dcam2", 0440,
		pd, &tb_dcam_id[2], &dcam_reg_ops))
		ret |= BIT(5);
	if (!debugfs_create_file("reg_fetch", 0440,
		pd, &tb_dcam_id[3], &dcam_reg_ops))
		ret |= BIT(6);
	if (!debugfs_create_file("zoom_mode", 0664,
		pd, NULL, &zoom_mode_ops))
		ret |= BIT(7);
	if (!debugfs_create_file("zoom_rds_limit", 0664,
		pd, NULL, &rds_limit_ops))
		ret |= BIT(8);

	if (!debugfs_create_file("dump_raw_en", 0664,
		pd, NULL, &dump_raw_ops))
		ret |= BIT(9);
	if (!debugfs_create_file("dump_count", 0664,
		pd, NULL, &dump_count_ops))
		ret |= BIT(10);
	if (!debugfs_create_file("csi_switch", 0664,
		pd, debugger, &csi_switch_ops))
		ret |= BIT(11);
	if (!debugfs_create_file("pyr_dec_bypass", 0664,
		pd, debugger, &pyr_dec_online_ops))
		ret |= BIT(12);
	if (!debugfs_create_file("dcam_raw_src", 0664,
		pd, debugger, &dcam_raw_src_ops))
		ret |= BIT(13);
	if (!debugfs_create_file("dump_switch", 0664,
		pd, NULL, &dumpswitch_ops))
		ret |= BIT(14);
	if (!debugfs_create_file("fbc_control", 0664,
		pd, NULL, &fbc_control_ops))
		ret |= BIT(15);
	if (!debugfs_create_file("dcam_cap_eof", 0664,
		pd, debugger, &dcam_eof_ops))
		ret |= BIT(16);
	if (!debugfs_create_file("recovery_bypass", 0664,
		pd, debugger, &recovery_ops))
		ret |= BIT(17);

	if (ret)
		ret = -ENOMEM;
	pr_info("dcam debugfs init ok\n");

	return ret;
}

static int camdebugger_dcam_deinit(void)
{
	if (s_p_dentry)
		debugfs_remove_recursive(s_p_dentry);
	s_p_dentry = NULL;
	return 0;
}
#else
static int camdebugger_dcam_init(struct camera_debugger *debugger)
{
	memset(g_dcam_bypass, 0x00, sizeof(g_dcam_bypass));
	return 0;
}

static int camdebugger_dcam_deinit(void)
{
	return 0;
}
#endif
/* dcam debugfs end */

/* isp debug fs starts */
#define DBG_REGISTER
#ifdef DBG_REGISTER
static int camdebugger_reg_buf_show(struct seq_file *s, void *unused)
{
	isp_cfg_ctx_reg_buf_debug_show((void *)s);
	return 0;
}

static int camdebugger_reg_buf_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, camdebugger_reg_buf_show, inode->i_private);
}

static const struct file_operations reg_buf_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_reg_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int camdebugger_isp_bypass_read(struct seq_file *s,
	void *unused)
{
	uint32_t addr, val;
	struct cam_debug_bypass *debug_bypass = NULL;
	struct cam_hw_info *ops = NULL;
	struct cam_hw_bypass_data data;
	uint32_t type;
	uint32_t idx = 0;
	uint32_t bypass_cnt = 0;
	uint32_t i = 0;

	if (!s) {
		pr_err("fail to get valid input para\n");
		return -EFAULT;
	}

	debug_bypass = (struct cam_debug_bypass *)s->private;
	idx = debug_bypass->idx;
	ops = debug_bypass->hw;
	if (!s_isp_dev) { /* isp not working */
		seq_printf(s, "isp hardware not working, can't read\n");
		return 0;
	}
	seq_printf(s, "===========isp context %d=============\n", idx);
	type = ISP_BYPASS_TYPE;
	bypass_cnt = ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_COUNT_GET, &type);
	data.type = ISP_BYPASS_TYPE;
	for (i = 0; i < bypass_cnt; i++) {
		data.i = i;
		ops->isp_ioctl(ops, ISP_HW_CFG_BYPASS_DATA_GET, &data);
		if (data.tag == NULL) {
			continue;
		}
		if (data.tag->p == NULL)
			continue;

		addr = data.tag->addr;
		val = ISP_REG_RD(idx, addr) & (1 << data.tag->bpos);
		if (val)
			seq_printf(s, "%s:%d  bypass\n", data.tag->p, val);
		else
			seq_printf(s, "%s:%d  work\n", data.tag->p, val);
	}
	seq_puts(s, "\nall:1 //bypass all except preview path\n");
	seq_puts(s, "\nltm:1 //(ltm-hist,ltm-map)\n");
	seq_puts(s, "\nnr3:1 //or 3dnr:1(all of 3dnr block)\n");

	return 0;
}

static ssize_t camdebugger_isp_bypass_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	struct seq_file *p = (struct seq_file *)filp->private_data;
	struct cam_debug_bypass *debug_bypass = NULL;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_bypass_data data;
	struct isp_hw_ltm_3dnr_param parm;
	uint32_t type;
	uint32_t idx = 0;
	uint32_t bypass_cnt = 0;
	char buf[256];
	uint32_t val = 2;
	uint32_t i;
	char name[16 + 1];
	uint32_t bypass_all = 0;
	uint64_t bypass_val = 1;

	debug_bypass = (struct cam_debug_bypass *)p->private;
	idx = debug_bypass->idx;
	hw = debug_bypass->hw;
	if (!s_isp_dev) { /* isp not working */
		pr_warn("warning: isp hardware not working, can't write\n");
		return count;
	}
	memset(buf, 0x00, sizeof(buf));
	i = count;
	if (i >= sizeof(buf))
		i = sizeof(buf) - 1; /* last one for \0 */
	if (copy_from_user(buf, buffer, i)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}
	buf[i] = '\0';
	/* get name */
	for (i = 0; i < sizeof(name) - 1; i++) {
		if (' ' == buf[i] || ':' == buf[i] ||
			',' == buf[i])
			break;
		/* to lowwer */
		if (buf[i] >= 'A' && buf[i] <= 'Z')
			buf[i] += ('a' - 'A');
		name[i] = buf[i];
	}
	name[i] = '\0';
	/* get val */
	for (; i < sizeof(buf); i++) {
		if (buf[i] >= '0' && buf[i] <= '9') {
			val = buf[i] - '0';
			break;
		}
	}
	val = val != 0 ? 1 : 0;
	/* find */
	if (strcmp(name, "all") == 0) {
		bypass_all = 1;
	} else { /* check special: ltm, nr3 */
		if (strcmp(name, "ltm") == 0) {
			parm.idx = idx;
			parm.val = val;
			hw->isp_ioctl(hw, ISP_HW_CFG_LTM_PARAM, &parm);
			bypass_val = (bypass_val << _EISP_LTM);
			g_isp_bypass[idx] &= (~bypass_val);
			if (val)
				g_isp_bypass[idx] |= bypass_val;
			return count;
		} else if (strcmp(name, "nr3") == 0 ||
			strcmp(name, "3dnr") == 0) {
			bypass_val = (bypass_val << _EISP_NR3);
			g_isp_bypass[idx] &= (~bypass_val);
			if (val)
				g_isp_bypass[idx] |= bypass_val;
			parm.idx = idx;
			parm.val = val;
			hw->isp_ioctl(hw, ISP_HW_CFG_3DNR_PARAM, &parm);
			/* ISP_HREG_MWR(ISP_FBC_3DNR_STORE_PARAM, BIT_0, val); */
			return count;
		}
	}
	type = ISP_BYPASS_TYPE;
	bypass_cnt = hw->isp_ioctl(hw, ISP_HW_CFG_BYPASS_COUNT_GET, &type);
	data.type = ISP_BYPASS_TYPE;
	for (i = 0; i < bypass_cnt; i++) {
		data.i = i;
		hw->isp_ioctl(hw, ISP_HW_CFG_BYPASS_DATA_GET, &data);
		if (data.tag == NULL) {
			continue;
		}
		if (data.tag->p == NULL)
			continue;
		if (strcmp(data.tag->p, name) == 0 || bypass_all) {
			pr_debug("set isp addr 0x%x, bit %d val %d\n",
				data.tag->addr, data.tag->bpos, val);
			if (i < _EISP_TOTAL) {
				bypass_val = val;
				bypass_val = bypass_val << i;
				g_isp_bypass[idx] &= (~bypass_val);
				g_isp_bypass[idx] |= bypass_val;
			}
			if (bypass_all && (data.tag->all == 0))
				continue;
			ISP_REG_MWR(idx, data.tag->addr, 1 << data.tag->bpos,
				val << data.tag->bpos);

			if (!bypass_all)
				break;
		}
	}

	return count;
}

static int camdebugger_isp_bypass_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, camdebugger_isp_bypass_read, inode->i_private);
}

static const struct file_operations isp_bypass_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_isp_bypass_open,
	.read = seq_read,
	.write = camdebugger_isp_bypass_write,
};

static uint8_t work_mode_string[2][16] = {
	"ISP_CFG_MODE", "ISP_AP_MODE"
};

static ssize_t camdebugger_work_mode_show(struct file *filp,
		char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[64];
	const char *desc = "0: ISP_CFG_MODE, 1: ISP_AP_MODE";

	snprintf(buf, sizeof(buf), "%d(%s)\n%s\n\n", s_dbg_work_mode,
		work_mode_string[s_dbg_work_mode&1], desc);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_work_mode_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int val;

	if (count > WORK_MODE_SLEN)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[WORK_MODE_SLEN-1] = '\0';
	val = simple_strtol(msg, &last, 0);
	if (val == 0)
		s_dbg_work_mode = ISP_CFG_MODE;
	else if (val == 1)
		s_dbg_work_mode = ISP_AP_MODE;
	else
		pr_err("fail to get valid work mode: %d\n", val);

	return count;
}

static const struct file_operations work_mode_ops = {
	.owner =	THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_work_mode_show,
	.write = camdebugger_work_mode_write,
};

static uint8_t iommu_mode_string[4][32] = {
	"IOMMU_AUTO",
	"IOMMU_OFF",
	"IOMMU_ON_RESERVED",
	"IOMMU_ON"
};

static ssize_t camdebugger_iommu_mode_show(struct file *filp,
		char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "cur: %d(%s), next: %d(%s)\n",
		g_dbg_iommu_mode,
		iommu_mode_string[g_dbg_iommu_mode&3],
		g_dbg_set_iommu_mode,
		iommu_mode_string[g_dbg_set_iommu_mode&3]);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_iommu_mode_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	char *last;
	int val;

	if (count > WORK_MODE_SLEN)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[WORK_MODE_SLEN-1] = '\0';
	val = simple_strtol(msg, &last, 0);
	if (val == 0)
		g_dbg_set_iommu_mode = IOMMU_AUTO;
	else if (val == 1)
		g_dbg_set_iommu_mode = IOMMU_OFF;
	else if (val == 2)
		g_dbg_set_iommu_mode = IOMMU_ON_RESERVED;
	else if (val == 3)
		g_dbg_set_iommu_mode = IOMMU_ON;
	else
		pr_err("fail to get valid work mode: %d\n", val);

	pr_info("set_iommu_mode : %d(%s)\n",
		g_dbg_set_iommu_mode,
		iommu_mode_string[g_dbg_set_iommu_mode&3]);
	return count;
}

static const struct file_operations iommu_mode_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_iommu_mode_show,
	.write = camdebugger_iommu_mode_write,
};

static ssize_t camdebugger_lbuf_len_show(struct file *filp,
		char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", s_dbg_linebuf_len);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_lbuf_len_write(struct file *filp,
		const char __user *buffer,
		size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	int val;

	if (count > LBUF_LEN_SLEN)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[LBUF_LEN_SLEN - 1] = '\0';
	val = simple_strtol(msg, NULL, 0);
	s_dbg_linebuf_len = val;
	pr_info("set line buf len %d.  %s\n", val, msg);

	return count;
}

static const struct file_operations lbuf_len_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_lbuf_len_show,
	.write = camdebugger_lbuf_len_write,
};

static ssize_t camdebugger_isp_pyr_dec_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	const char *desc = "0: work, 1: bybass";
	char buf[60];
	char *str = "Example: echo 1 > offline_dec_bypass";

	snprintf(buf, sizeof(buf), "%u\n\n%s\n\n%s\n\n", g_pyr_dec_offline_bypass, desc ,str);

	return simple_read_from_buffer(
		buffer, count, ppos,
		buf, strlen(buf));
}

static ssize_t camdebugger_isp_pyr_dec_bypass(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char msg[8];
	uint32_t val;

	if (count > 2)
		return -EINVAL;

	ret = copy_from_user(msg, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	msg[count] = '\0';
	ret = kstrtouint(msg, 10, &val);
	if (ret < 0) {
		pr_err("fail to convert '%s', ret %d\n", msg, ret);
		return ret;
	}

	g_pyr_dec_offline_bypass = val;
	pr_debug("set pyr_dec_offline_bypass %u\n", g_pyr_dec_offline_bypass);

	return count;
}

static const struct file_operations pyr_dec_offline_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = camdebugger_isp_pyr_dec_show,
	.write = camdebugger_isp_pyr_dec_bypass,
};

static int camdebugger_fbc_ctrl_read(struct seq_file *s, void *unused)
{
	struct compression_override *override = NULL;
	int i = 0;

	override = (struct compression_override *)s->private;

	for (i = 0; i < CAM_ID_MAX; i++, override++) {
		seq_printf(s, "\n===== CAM%d override %u =====\n",
			i, override->enable);
		seq_printf(s, "          dcam   3dnr   isp\n");
		seq_printf(s, "preview      %u      %u     %u\n",
			override->override[CH_PRE][FBC_DCAM],
			override->override[CH_PRE][FBC_3DNR],
			override->override[CH_PRE][FBC_ISP]);
		seq_printf(s, "capture      %u      %u     %u\n",
			override->override[CH_CAP][FBC_DCAM],
			override->override[CH_CAP][FBC_3DNR],
			override->override[CH_CAP][FBC_ISP]);
		seq_printf(s, "video        %u      %u     %u\n",
			override->override[CH_VID][FBC_DCAM],
			override->override[CH_VID][FBC_3DNR],
			override->override[CH_VID][FBC_ISP]);
	}

	seq_printf(s, "\nUsage:\n");
	seq_printf(s, "         echo ID override 1|0 > fbc_ctrl\n");
	seq_printf(s, "         echo ID pre|cap|vid 3dnr|isp 0|1 > fbc_ctrl\n");
	seq_printf(s, "         echo ID pre|cap|vid dcam 0|1 > fbc_ctrl\n");
	seq_printf(s, "         echo 0:DCAM_FBC_DISABLE\n");
	seq_printf(s, "         echo 1:DCAM_FBC_FULL_10_BIT\n");
	seq_printf(s, "         echo 3:DCAM_FBC_BIN_10_BIT\n");
	seq_printf(s, "         echo 5:DCAM_FBC_FULL_14_BIT\n");
	seq_printf(s, "         echo 7:DCAM_FBC_BIN_14_BIT\n");

	seq_printf(s, "\nExample:\n");
	seq_printf(s, "         echo 0 override 1 > fbc_ctrl   // enable fbc control override\n");
	seq_printf(s, "         echo 1 cap dcam 1 > fbc_ctrl   // enable dcam fbc for cam1 capture channel\n");
	seq_printf(s, "         echo 2 vid isp 0 > fbc_ctrl    // disable isp fbc for cam2 video channel\n");
	seq_printf(s, "         echo 3 pre 3dnr 1 > fbc_ctrl   // enable 3dnr fbc for cam3 preview channel\n");

	return 0;
}

static int camdebugger_fbc_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, camdebugger_fbc_ctrl_read, inode->i_private);
}

static ssize_t camdebugger_fbc_ctrl_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	struct seq_file *p = (struct seq_file *)filp->private_data;
	struct compression_override *override =
		(struct compression_override *)p->private;
	char buf[32] = { 0 }, *s, *c;
	unsigned int id = 0, ch, type = 0, en = 0;
	int ret;

	if (count > 32 || count < 1) {
		pr_err("fail to get command count\n");
		return -EINVAL;
	}

	ret = copy_from_user(buf, (void __user *)buffer, count);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EINVAL;
	}
	buf[count - 1] = '\0';

	/* camera id */
	s = buf;
	c = strchr(s, ' ');
	if (!c) {
		pr_err("fail to get camera id\n");
		return -EINVAL;
	}
	*c = '\0';

	if (kstrtouint(s, 10, &id) < 0) {
		pr_err("fail to parse camera id '%s'\n", s);
		return -EINVAL;
	}

	if (id >= CAM_ID_MAX) {
		pr_err("fail to get valid camera id %u\n", id);
		return -EINVAL;
	}

	/* commands or path */
	s = c + 1;
	c = strchr(s, ' ');
	if (!c) {
		pr_err("fail to get path or cmd\n");
		return -EINVAL;
	}
	*c = '\0';

	if (!strncmp(s, "override", 8)) {
		ch = CH_MAX;
	} else if (!strncmp(s, "pre", 3)) {
		ch = CH_PRE;
	} else if (!strncmp(s, "cap", 3)) {
		ch = CH_CAP;
	} else if (!strncmp(s, "vid", 3)) {
		ch = CH_VID;
	} else {
		pr_err("fail to parse path or cmd '%s'\n", s);
		return -EINVAL;
	}

	if (ch != CH_MAX) {
		/* type if needed */
		s = c + 1;
		c = strchr(s, ' ');
		if (!c) {
			pr_err("fail to get fbc type\n");
			return -EINVAL;
		}
		*c = '\0';

		if (!strncmp(s, "dcam", 4)) {
			type = FBC_DCAM;
		} else if (!strncmp(s, "3dnr", 4)) {
			type = FBC_3DNR;
		} else if (!strncmp(s, "isp", 3)) {
			type = FBC_ISP;
		} else {
			pr_err("fail to get fbc type '%s'\n", s);
			return -EINVAL;
		}
	}

	/* enable */
	s = c + 1;
	if (kstrtouint(s, 10, &en) < 0) {
		pr_err("fail to parse enable '%s'\n", s);
		return -EINVAL;
	}

	/* set */
	if (ch == CH_MAX) {
		pr_info("CAM%u override enable %u\n", id, en);
		override[id].enable = en;
	} else {
		pr_info("CAM%u compression override %u %u %u\n",
			id, ch, type, en);
		override[id].override[ch][type] = en;
	}

	return count;
}

static const struct file_operations fbc_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = camdebugger_fbc_ctrl_open,
	.read = seq_read,
	.write = camdebugger_fbc_ctrl_write,
};

static int camdebugger_isp_init(struct camera_debugger *debugger)
{
	struct dentry *entry = NULL;
	static struct cam_debug_bypass isp_debug_bypass[4];
	char dirname[32] = {0};
	int i = 0;

	if (!debugger) {
		pr_err("fail to get valid para\n");
		return -EFAULT;
	}

	for (i = 0; i < 4; i++) {
		isp_debug_bypass[i].idx = debug_ctx_id[i];
		isp_debug_bypass[i].hw = debugger->hw;
	}
	snprintf(dirname, sizeof(dirname), "sprd_isp");
	debugfs_base = debugfs_create_dir(dirname, NULL);
	if (!debugfs_base) {
		pr_err("fail to create debugfs dir\n");
		return -ENOMEM;
	}
	memset(g_isp_bypass, 0x00, sizeof(g_isp_bypass));
	if (!debugfs_create_file("work_mode", 0644,
		debugfs_base, NULL, &work_mode_ops))
		return -ENOMEM;

	if (!debugfs_create_file("iommu_mode", 0644,
		debugfs_base, NULL, &iommu_mode_ops))
		return -ENOMEM;

	if (!debugfs_create_file("line_buf_len", 0644,
		debugfs_base, NULL, &lbuf_len_ops))
		return -ENOMEM;

	if (!debugfs_create_file("offline_dec_bypass", 0664,
		debugfs_base, NULL, &pyr_dec_offline_ops))
		return -ENOMEM;

#ifdef DBG_REGISTER
	if (!debugfs_create_file("pre0_buf", 0444,
		debugfs_base, &isp_debug_bypass[0], &reg_buf_ops))
		return -ENOMEM;
	if (!debugfs_create_file("cap0_buf", 0444,
		debugfs_base, &isp_debug_bypass[1], &reg_buf_ops))
		return -ENOMEM;
	if (!debugfs_create_file("pre1_buf", 0444,
		debugfs_base, &isp_debug_bypass[2], &reg_buf_ops))
		return -ENOMEM;
	if (!debugfs_create_file("cap1_buf", 0444,
		debugfs_base, &isp_debug_bypass[3], &reg_buf_ops))
		return -ENOMEM;
#endif

	if (!debugfs_create_file("pre0_bypass", 0660,
		debugfs_base, &isp_debug_bypass[0], &isp_bypass_ops))
		return -ENOMEM;
	if (!debugfs_create_file("cap0_bypass", 0660,
		debugfs_base, &isp_debug_bypass[1], &isp_bypass_ops))
		return -ENOMEM;
	if (!debugfs_create_file("pre1_bypass", 0660,
		debugfs_base, &isp_debug_bypass[2], &isp_bypass_ops))
		return -ENOMEM;
	if (!debugfs_create_file("cap1_bypass", 0660,
		debugfs_base, &isp_debug_bypass[3], &isp_bypass_ops))
		return -ENOMEM;

	entry = debugfs_create_file("fbc_ctrl", 0660, debugfs_base,
		&debugger->compression[0], &fbc_ctrl_ops);
	if (IS_ERR_OR_NULL(entry))
		return -ENOMEM;

	return 0;
}

static int camdebugger_isp_deinit(void)
{
	if (debugfs_base)
		debugfs_remove_recursive(debugfs_base);
	debugfs_base = NULL;
	return 0;
}
/* isp debug fs end */

int cam_debugger_init(struct camera_debugger *debugger)
{
	int ret = 0;

	ret = camdebugger_dcam_init(debugger);
	if (ret)
		pr_err("fail to init dcam debugfs\n");
	ret = camdebugger_isp_init(debugger);
	if (ret)
		pr_err("fail to init isp debugfs\n");

	return ret;
}

int cam_debugger_deinit(void)
{
	camdebugger_dcam_deinit();
	camdebugger_isp_deinit();

	return 0;
}
