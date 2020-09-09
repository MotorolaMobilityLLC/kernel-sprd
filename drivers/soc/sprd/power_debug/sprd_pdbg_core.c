/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/cpu_pm.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sprd_sip_svc.h>
#include "sprd_pdbg.h"

#define SPRD_PDBG_PROC_DIR "sprd-power-debug"
#define SPRD_PDBG_ENABLE "enable"
#define SPRD_PDBG_INTERVAL "interval"

static struct power_debug *pgdb_entry;
static struct proc_dir_entry *sprd_pdbg_proc_dir;

/**
 * sprd_pgdb_print_pdm_info - output the power state of each power domain
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pgdb_print_pdm_info(struct power_debug *pdebug_entry)
{
	u32 pwr_status, bit_index, bit_index_j, subsys_state;
	u32 i, j;
	struct pdm_info *ppdm_info;
	struct power_debug_desc *pdesc = pdebug_entry->pdesc;
	struct power_debug_cfg *pcfg = pdebug_entry->pcfg;

	if (!pdebug_entry->pdbg_enable || !pdesc || !pcfg)
		return;

	if (!pcfg->pmu_apb || !pdesc->pmu_pdm_num || !pdesc->ppdm_info)
		return;

	pr_info("###--PMU submodule power states--###\n");
	for (i = 0; i < pdesc->pmu_pdm_num; i++) {
		ppdm_info = &pdesc->ppdm_info[i];

		regmap_read(pcfg->pmu_apb, ppdm_info->addr_offset,
			&pwr_status);

		pr_info(" ##--reg offset:0x%04x value:0x%08x:\n",
			ppdm_info->addr_offset, pwr_status);

		for (j = 0; j < MAX_STATES_NUM_PER_REG; j++) {
			bit_index_j = ppdm_info->bit_index[j];
			bit_index = bit_index_j +
				ppdm_info->pwd_bit_width;
			if (bit_index > BIT_NUM_IN_PER_REG)
				break;
			subsys_state = pwr_status >> bit_index_j &
				((1 << ppdm_info->pwd_bit_width) - 1);
			if (ppdm_info->pdm_name[j]) {
				pr_info("  #--%s STATE:0x%X\n",
					ppdm_info->pdm_name[j],
					subsys_state);
			}
		}
	}
}

/**
 * sprd_pgdb_print_check_reg - output the register value of the indicated
 *     register bank if the register value is not expected
 * @pregmap: the regmap indicate the specific register bank
 * @reg_num: the register number in the register in the register table
 * @pentry_tbl: the pointer of the register table which want to be checked
 * @module_name: the register bank name
 */
#define OPT_FMT " ##--offset:0x%04x value:0x%08x mask:0x%08x exp_val:0x%08x\n"
static void sprd_pgdb_print_check_reg(struct regmap *pregmap, u32 reg_num,
	struct reg_check *pentry_tbl, const char *module_name)
{
	u32 index, reg_value;
	struct reg_check *pentry;

	if (!pregmap || !reg_num || !pentry_tbl)
		return;

	pr_info("###--PMU %s register check--###\n", module_name);
	for (index = 0; index < reg_num; index++) {
		pentry = &pentry_tbl[index];
		regmap_read(pregmap, pentry->addr_offset, &reg_value);

		if ((reg_value & pentry->value_mask) != pentry->expect_value) {
			pr_info(OPT_FMT, pentry->addr_offset, reg_value,
				pentry->value_mask, pentry->expect_value);
		}
	}
}

/**
 * sprd_pgdb_print_reg_check - output the register value if it's value is not
 *     expected
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pgdb_print_reg_check(struct power_debug *pdebug_entry)
{
	struct power_debug_desc *pdesc = pdebug_entry->pdesc;
	struct power_debug_cfg *pcfg = pdebug_entry->pcfg;

	if (!pdebug_entry->pdbg_enable || !pdesc || !pcfg)
		return;

	sprd_pgdb_print_check_reg(pcfg->ap_ahb,
				pdesc->ap_ahb_reg_num,
				pdesc->ap_ahb_reg, "ap-ahb");
	sprd_pgdb_print_check_reg(pcfg->ap_apb,
				pdesc->ap_apb_reg_num,
				pdesc->ap_apb_reg, "ap-apb");
	sprd_pgdb_print_check_reg(pcfg->pmu_apb,
				pdesc->pmu_apb_reg_num,
				pdesc->pmu_apb_reg, "pmu-apb");
	sprd_pgdb_print_check_reg(pcfg->aon_apb,
				pdesc->aon_apb_reg_num,
				pdesc->aon_apb_reg, "aon-apb");
	sprd_pgdb_print_check_reg(pcfg->aon_sec,
				pdesc->aon_sec_reg_num,
				pdesc->aon_sec_reg, "aon-sec");
}

/**
 * sprd_pgdb_print_intc_state - output the value of the interrupt state register
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pgdb_print_intc_state(struct power_debug *pdebug_entry)
{
	u32 reg_value;
	int ret;
	int i;
	struct intc_info *pintc_info;
	struct power_debug_desc *pdesc = pdebug_entry->pdesc;
	struct power_debug_cfg *pcfg = pdebug_entry->pcfg;

	if (!pdebug_entry->pdbg_enable || !pdesc || !pcfg)
		return;

	if (!pdesc->ap_intc_num || !pdesc->pintc_info)
		return;

	for (i = 0; i < pdesc->ap_intc_num; i++) {
		pintc_info = &(pdesc->pintc_info[i]);

		ret = regmap_read(pcfg->ap_intc[i],
				pintc_info->addr_offset, &reg_value);
		if (ret)
			dev_err(pdebug_entry->dev,
				"Failed to get intc mask reg value.\n");
		else
			pr_info("##--Status of intc%d :0x%08x\n", i, reg_value);
	}
}

/**
 * sprd_pgdb_print_wakeup_source - output the wakeup interrupt name
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pgdb_print_wakeup_source(struct power_debug *pdebug_entry)
{
	u32 i, j, reg_value;
	int ret;
	struct intc_info *pintc_info;
	struct power_debug_desc *pdesc = pdebug_entry->pdesc;
	struct power_debug_cfg *pcfg = pdebug_entry->pcfg;

	if (!pdebug_entry->pdbg_enable || !pdesc || !pcfg)
		return;

	if (!pdesc->ap_intc_num || !pdesc->pintc_info)
		return;

	for (i = 0; i < pdesc->ap_intc_num; i++) {
		pintc_info = &pdesc->pintc_info[i];

		ret = regmap_read(pcfg->ap_intc[i],
				pintc_info->addr_offset, &reg_value);
		if (ret) {
			dev_err(pdebug_entry->dev,
				"Failed to get intc mask reg value.\n");
			continue;
		}

		if (!reg_value)
			continue;

		for (j = 0; j < BIT_NUM_IN_PER_REG; j++) {
			if (!(reg_value & BIT(j)))
				continue;

			pr_info("#--Wake up by %d(%s:%s)!\n",
				i * BIT_NUM_IN_PER_REG + j, "AP_INTC",
				pintc_info->pint_name[j]);
			if (pdesc->irq_mask[i] & BIT(j) &&
					pdesc->log_2nd_irq_source)
				pdesc->log_2nd_irq_source(pdebug_entry,
				i * BIT_NUM_IN_PER_REG + j);
		}
	}
}

/**
 * sprd_pgdb_notifier - Notification call back function
 */
static int sprd_pgdb_notifier(struct notifier_block *self,
	unsigned long cmd, void *v)
{
	if (!pgdb_entry)
		return NOTIFY_DONE;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		sprd_pgdb_print_reg_check(pgdb_entry);
		sprd_pgdb_print_intc_state(pgdb_entry);
		sprd_pgdb_print_pdm_info(pgdb_entry);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
		break;
	case CPU_CLUSTER_PM_EXIT:
		sprd_pgdb_print_wakeup_source(pgdb_entry);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**
 * Notifier object
 */
static struct notifier_block sprd_pgdb_notifier_block = {
	.notifier_call = sprd_pgdb_notifier,
};

/**
 * sprd_pgdb_thread - the log output thread function
 * @data: a parameter pointer
 */
static int sprd_pgdb_thread(void *data)
{
	struct power_debug *pdbg = (struct power_debug *)data;

	while (pdbg->task) {
		if (kthread_should_stop())
			break;

		sprd_pgdb_print_pdm_info(pdbg);

		dev_info(pdbg->dev, "PM: has wakeup events in progress:\n");
		pm_print_active_wakeup_sources();

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(pdbg->scan_interval * HZ);
	}

	return 0;
}

/**
 * sprd_pgdb_start_monitor - start the log output mechanism
 * @pdbg: the pointer of the pdbg core structure of this driver
 */
static int sprd_pgdb_start_monitor(struct power_debug *pdbg)
{
	struct task_struct *ptask;

	if (!pdbg)
		return -EINVAL;

	if (pdbg->pdbg_enable && !pdbg->task) {
		ptask = kthread_create(sprd_pgdb_thread,
					pdbg, "sprd-pgdb-thread");
		if (IS_ERR(ptask)) {
			dev_err(pdbg->dev,
				"Unable to start kernel thread.\n");
			return PTR_ERR(ptask);
		}
		pdbg->task = ptask;
		wake_up_process(ptask);

		cpu_pm_register_notifier(&sprd_pgdb_notifier_block);
	}

	return 0;
}

/**
 * sprd_pgdb_stop_monitor - stop the log output mechanism
 * @pdbg: the pointer of the pdbg core structure of this driver
 */
static void sprd_pgdb_stop_monitor(struct power_debug *pdbg)
{
	if (!pdbg)
		return;

	if (pdbg->pdbg_enable && pdbg->task) {
		kthread_stop(pdbg->task);
		pdbg->task = NULL;

		cpu_pm_unregister_notifier(&sprd_pgdb_notifier_block);
	}
}

static int sprd_pgdb_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int sprd_pgdb_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_pgdb_proc_show, NULL);
}

static ssize_t sprd_pgdb_enable_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	u32 value;

	if (!pgdb_entry || !count)
		return -EINVAL;

	ret = kstrtouint_from_user(buffer, count, 10, &value);
	if (ret < 0)
		return (ssize_t)ret;

	mutex_lock(&pgdb_entry->conf_mutex);
	if (value) {
		pgdb_entry->pdbg_enable = 1;
		sprd_pgdb_start_monitor(pgdb_entry);
	} else {
		sprd_pgdb_stop_monitor(pgdb_entry);
		pgdb_entry->pdbg_enable = 0;
	}
	mutex_unlock(&pgdb_entry->conf_mutex);

	return (ssize_t)count;
}

static ssize_t sprd_pgdb_enable_read(struct file *file,
		char __user *buf, size_t len, loff_t *off)
{
	char data[1];

	if (!pgdb_entry || !len)
		return -EINVAL;

	if (file->f_pos || *off)
		return 0;

	if (pgdb_entry->pdbg_enable)
		data[0] = '1';
	else
		data[0] = '0';

	if (copy_to_user(buf, data, 1))
		return -EFAULT;

	*off += 1;
	return 1;
}

static ssize_t sprd_pgdb_interval_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	u32 interval;

	if (!pgdb_entry || !count)
		return -EINVAL;

	ret = kstrtouint_from_user(buffer, count, 10, &interval);
	if (ret < 0)
		return (ssize_t)ret;

	pgdb_entry->scan_interval = clamp(interval, (u32)0x02, (u32)0x3E8);

	return (ssize_t)count;
}

static ssize_t sprd_pgdb_interval_read(struct file *file,
		char __user *buf, size_t len, loff_t *off)
{
	size_t len_copy, ret;
	char data[9];

	if (!pgdb_entry || !len)
		return -EINVAL;

	ret = (size_t)snprintf(data, 9, "%d", pgdb_entry->scan_interval);
	if (!ret)
		return ret;

	if ((file->f_pos + len) > ret)
		len_copy = ret - file->f_pos;
	else
		len_copy = len;

	ret = (size_t)copy_to_user(buf, data + file->f_pos, len_copy);
	len_copy = len_copy - ret;

	*off += len_copy;

	return len_copy;
}

static const struct file_operations pgdb_enable_proc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_pgdb_proc_open,
	.release = single_release,
	.read = sprd_pgdb_enable_read,
	.llseek = seq_lseek,
	.write = sprd_pgdb_enable_write,
};

static const struct file_operations pgdb_interval_proc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_pgdb_proc_open,
	.release = single_release,
	.read = sprd_pgdb_interval_read,
	.llseek = seq_lseek,
	.write = sprd_pgdb_interval_write,
};

struct power_debug *sprd_power_debug_register(struct device *dev,
				 struct power_debug_desc *pdesc,
				 struct power_debug_cfg *pcfg)
{
	int result;
	struct power_debug *pdbg;
	struct proc_dir_entry *file;
#if 0 // wait the sip service interface added
	struct sprd_sip_svc_handle *svc_handle;
#endif

	if (pgdb_entry)
		return NULL;

	pdbg = devm_kzalloc(dev, sizeof(struct power_debug), GFP_KERNEL);
	if (!pdbg)
		return NULL;

	pdbg->pdbg_enable = pcfg->pdbg_enable;
	pdbg->scan_interval = pcfg->scan_interval;
	pdbg->dev = dev;
	pdbg->pdesc = pdesc;
	pdbg->pcfg = pcfg;
	pdbg->task = NULL;

	result = sprd_pgdb_start_monitor(pdbg);
	if (result)
		goto start_monitor_error;

	sprd_pdbg_proc_dir = proc_mkdir(SPRD_PDBG_PROC_DIR, NULL);
	if (!sprd_pdbg_proc_dir) {
		dev_err(dev, "Proc FS create %s failed\n",
			SPRD_PDBG_PROC_DIR);
		goto proc_mkdir_error;
	}
	file = proc_create(SPRD_PDBG_ENABLE, 0777,
			sprd_pdbg_proc_dir, &pgdb_enable_proc_fops);
	if (!file) {
		dev_err(dev, "Proc FS create %s failed\n",
			SPRD_PDBG_ENABLE);
		goto proc_create_1_error;
	}
	file = proc_create(SPRD_PDBG_INTERVAL, 0777,
			sprd_pdbg_proc_dir, &pgdb_interval_proc_fops);
	if (!file) {
		dev_err(dev, "Proc FS create %s failed\n",
			SPRD_PDBG_INTERVAL);
		goto proc_create_2_error;
	}

#if 0 // wait the sip service interface added
	svc_handle = sprd_sip_svc_get_handle();
	if (svc_handle)
		pdbg->svc_handle = svc_handle;
#endif

	mutex_init(&pdbg->conf_mutex);
	pgdb_entry = pdbg;
	return pdbg;

proc_create_2_error:
	remove_proc_entry(SPRD_PDBG_ENABLE, sprd_pdbg_proc_dir);
proc_create_1_error:
	remove_proc_entry(SPRD_PDBG_PROC_DIR, NULL);
proc_mkdir_error:
	sprd_pgdb_stop_monitor(pdbg);
start_monitor_error:
	devm_kfree(dev, pdbg);
	return NULL;
}

void sprd_power_debug_unregister(struct power_debug *pdbg)
{
	if (pgdb_entry == pdbg) {
		remove_proc_entry(SPRD_PDBG_INTERVAL, sprd_pdbg_proc_dir);
		remove_proc_entry(SPRD_PDBG_ENABLE, sprd_pdbg_proc_dir);
		remove_proc_entry(SPRD_PDBG_PROC_DIR, NULL);

		sprd_pgdb_stop_monitor(pdbg);
		devm_kfree(pdbg->dev, pdbg);
		pgdb_entry = NULL;
	}
}

u32 sprd_pdb_read_pmic_register(u32 chip_id, u32 offset_addr)
{
#if 0 // wait the sip service interface added
	int result;
	u32 reg_val;

	if (!pgdb_entry || !pgdb_entry->svc_handle) {
		dev_err(pgdb_entry->dev, "return because svc_handle is NULL\n");
		return 0xFFFFFFFF;
	}

	result = pgdb_entry->svc_handle->pmic_ops.get_reg(chip_id,
				offset_addr, &reg_val);
	if (!result)
		return reg_val;

	dev_err(pgdb_entry->dev, "result=%d, reg_val=0x%x\n", result, reg_val);
#endif
	return 0xFFFFFFFF;
}
