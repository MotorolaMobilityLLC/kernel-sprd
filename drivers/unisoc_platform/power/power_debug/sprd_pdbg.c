// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */
#include <linux/cpu_pm.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqnr.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/soc/sprd/sprd_pdbg.h>
#include <linux/spinlock.h>
#include <linux/sprd_sip_svc.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define SPRD_REG_INFO_PER_MAX (128)
#define SPRD_LOG_BUF_MAX      (SPRD_REG_INFO_PER_MAX*PDBG_INFO_MAX)
#define ERROR_MAGIC          (0xAA99887766554433UL)
#define PDBG_IGNORE_MAGIC    (0xEEDDCCBBAA998877)
#define WAKEUP_NAME_NUM      (2)
#define PDBG_INFO_NUM        (4)
#define IRQ_DOMAIN_RETRY_CNT        (10)
#define DATA_INVALID    (0xFFFFFFFFU)

#define SLP_FMT       "[SLP_STATE] deep: 0x%llx, light: 0x%llx\n"
#define EB_FMT        "[EB_INFO  ] ap1: 0x%llx, ap2: 0x%llx, aon1: 0x%llx, aon2: 0x%llx\n"
#define PD_FMT        "[PD_INFO  ] 0x%llx\n"
#define LPC_FMT       "[LPC_INFO ] 0x%llx\n"
#define DEEP_CNT_FMT  "[DEEP_CNT ] "
#define LIGHT_CNT_FMT "[LIGHT_CNT] "

static int sprd_pdbg_log_force;
module_param(sprd_pdbg_log_force, int, 0644);
MODULE_PARM_DESC(sprd_pdbg_log_force, "sprd pdbg log force out (default: 0)");
#define SPRD_PDBG_ERR(fmt, ...)  pr_err("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__)
#define SPRD_PDBG_WARN(fmt, ...) pr_warn("[%s]"pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__)
#define SPRD_PDBG_INFO(fmt, ...)							\
	do {										\
		if (!!sprd_pdbg_log_force)						\
			pr_info("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__);	\
		else									\
			pr_err("[%s] "pr_fmt(fmt), "SPRD_PDBG", ##__VA_ARGS__);		\
	} while (0)									\

enum {
	PDBG_PHASE0,
	PDBG_PHASE1,
	PDBG_PHASE_MAX
};

enum {
	SIP_SVC_PWR_V0,
	SIP_SVC_PWR_V1,
};

struct power_debug {
	struct task_struct *task;
	struct device *dev;
	struct sprd_sip_svc_pwr_ops *power_ops;
	struct notifier_block pm_notifier_block;
	struct notifier_block cpu_pm_notifier_block;
	struct list_head ws_irq_domain_list;
	rwlock_t rw_lock;
	char *wakeup_source[WAKEUP_NAME_NUM];
	u64 r_value[PDBG_INFO_NUM+1];
	u64 r_value_h[PDBG_INFO_NUM+1];
	const char *irq_domain_names[SPRD_PDBG_WS_DOMAIN_ID_MAX];
	u32 scan_interval;
	ktime_t last_monotime; /* monotonic time before last suspend */
	ktime_t curr_monotime; /* monotonic time after last suspend */
	ktime_t last_stime; /* monotonic boottime offset before last suspend */
	ktime_t curr_stime; /* monotonic boottime offset after last suspend */
	bool module_log_enable;
	bool is_32b_machine;
	char log_buf[SPRD_LOG_BUF_MAX];
};

struct ws_irq_domain {
	int domain_id;
	void *priv_data;
	struct list_head list;
};

static struct power_debug *sprd_pdbg_get_instance(void)
{
	static struct power_debug *pdbg;

	if (pdbg)
		return pdbg;

	pdbg = kzalloc(sizeof(struct power_debug), GFP_KERNEL);

	return pdbg;
}

static struct ws_irq_domain *sprd_pdbg_ws_irq_domain_get(int irq_domain_id)
{
	struct ws_irq_domain *pos, *tmp;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	read_lock(&pdbg->rw_lock);
	list_for_each_entry_safe(pos, tmp, &pdbg->ws_irq_domain_list, list) {
		if (pos->domain_id != irq_domain_id)
			continue;
		read_unlock(&pdbg->rw_lock);
		return pos;
	}
	read_unlock(&pdbg->rw_lock);

	return NULL;
}

static void sprd_pdbg_ws_get_info(struct ws_irq_domain *ws_irq_domain, u32 hwirq,
				  char *ws_info, int *buf_cnt)
{
	int virq;
	struct irq_desc *desc;
	struct irq_domain *irq_domain = (struct irq_domain *)(ws_irq_domain->priv_data);

	virq = irq_find_mapping(irq_domain, hwirq);
	desc = irq_to_desc(virq);

	*buf_cnt += scnprintf(ws_info + *buf_cnt, SPRD_LOG_BUF_MAX - *buf_cnt,
			     " | [%d]", virq);

	if (desc == NULL) {
		*buf_cnt += scnprintf(ws_info + *buf_cnt, SPRD_LOG_BUF_MAX - *buf_cnt,
				     "| stray irq");
		return;
	}

	if (desc->action && desc->action->name)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, SPRD_LOG_BUF_MAX - *buf_cnt,
				     " | action: %s", desc->action->name);

	if (desc->action && desc->action->handler)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, SPRD_LOG_BUF_MAX - *buf_cnt,
				     " | handler: %ps", desc->action->handler);

	if (desc->action && desc->action->thread_fn)
		*buf_cnt += scnprintf(ws_info + *buf_cnt, SPRD_LOG_BUF_MAX - *buf_cnt,
				     " | thread_fn: %ps", desc->action->thread_fn);
}

static int sprd_pdbg_ws_parse(u32 major, u32 irq_domain_id, u32 hwirq, char *ws_info)
{
	int buf_cnt = 0, intc_num, intc_bit;
	struct ws_irq_domain *ws_irq_domain;

	intc_num = (major >> 16) & 0xFFFF;
	intc_bit = major & 0xFFFF;

	buf_cnt += scnprintf(ws_info + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt, "[%d:%d:%d:%d]",
			    intc_num, intc_bit, irq_domain_id, hwirq);

	if ((irq_domain_id != DATA_INVALID) && (hwirq != DATA_INVALID)) {

		ws_irq_domain = sprd_pdbg_ws_irq_domain_get(irq_domain_id);

		if (!ws_irq_domain) {
			SPRD_PDBG_ERR("%s: ws irq domain match error\n", __func__);
			return 0;
		}

		sprd_pdbg_ws_get_info(ws_irq_domain, hwirq, ws_info, &buf_cnt);
	}
	return 0;
}

static int sprd_pdbg_regs_get_once(struct power_debug *pdbg, u32 info_type)
{
	u64 ret;
	int i;

	if (!pdbg) {
		SPRD_PDBG_ERR("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	ret = pdbg->power_ops->get_pdbg_info(info_type, PDBG_PHASE0, &pdbg->r_value[0],
					     &pdbg->r_value[1], &pdbg->r_value[2],
					     &pdbg->r_value[3]);
	if (ret == ERROR_MAGIC) {
		SPRD_PDBG_ERR("Get pdbg info: %d error\n", info_type);
		return -EINVAL;
	}

	if (pdbg->is_32b_machine && info_type < PDBG_WS) {
		ret = pdbg->power_ops->get_pdbg_info(info_type, PDBG_PHASE1, &pdbg->r_value_h[0],
						     &pdbg->r_value_h[1], &pdbg->r_value_h[2],
						     &pdbg->r_value_h[3]);
		if (ret == ERROR_MAGIC) {
			SPRD_PDBG_ERR("Get pdbg info: %d error\n", info_type);
			return -EINVAL;
		}
		for (i = 0; i < PDBG_INFO_NUM; i++)
			pdbg->r_value[i] = (pdbg->r_value[i] | (pdbg->r_value_h[i] << 32));
	}

	return 0;
}

static int sprd_pdbg_ws_show(struct power_debug *pdbg)
{
	u64 major, domain_id, hwirq;
	int ret = 0;
	char *ws_irq_info = pdbg->log_buf;

	if (!pdbg) {
		SPRD_PDBG_ERR("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_WS)) {
		major = pdbg->r_value[0];
		domain_id = pdbg->r_value[1];
		hwirq =  pdbg->r_value[2];
	} else {
		return -EINVAL;
	}

	/**
	 * The interface is called in the process of entering the suspend, and
	 * if the entry into the suspend fails, the wake-up source
	 * cannot be obtained.
	 */
	if (!major) {
		SPRD_PDBG_WARN("The system has not yet entered sleep mode\n");
		return 0;
	}

	ret = sprd_pdbg_ws_parse((u32)major, (u32)domain_id, (u32)hwirq, ws_irq_info);
	SPRD_PDBG_INFO("%s\n", ws_irq_info);

	return ret;
}

static inline void sprd_pdbg_regs_msg_print(char *regs_msg, int *buf_cnt, bool print_out)
{
	if (print_out) {
		SPRD_PDBG_INFO("%s", regs_msg);
		*buf_cnt = 0;
	}
}

static int sprd_pdbg_regs_get(struct power_debug *pdbg, char *regs_msg, bool print_out)
{
	u64 slp_deep, slp_light, eb_ap1, eb_ap2, eb_aon1, eb_aon2, pd, lpc;
	int buf_cnt = 0, cnt_num, cnt_low, cnt_high, cnt, i;
	char *pval;

	if (!pdbg) {
		SPRD_PDBG_ERR("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_SLP)) {
		slp_deep = pdbg->r_value[0];
		slp_light = pdbg->r_value[1];
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
				    SLP_FMT, slp_deep, slp_light);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_EB)) {
		eb_ap1 = pdbg->r_value[0];
		eb_ap2 = pdbg->r_value[1];
		eb_aon1 = pdbg->r_value[2];
		eb_aon2 = pdbg->r_value[3];
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
				    EB_FMT, eb_ap1, eb_ap2, eb_aon1, eb_aon2);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_PD)) {
		pd = pdbg->r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
				    PD_FMT, pd);
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_DCNT)) {
		pval = (char *)&pdbg->r_value[4];
		pval -= 2;
		cnt_num = *pval;
		pval = (char *)&pdbg->r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
				    DEEP_CNT_FMT);
		for (i = 0; i < cnt_num; i++) {
			cnt_low = *(pval + 2*i);
			cnt_high = *(pval + 2*i + 1);
			cnt = (cnt_low | (cnt_high << 8));
			buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
					    "%5d, ", cnt);
		}
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt, "\n");
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_LCNT)) {
		pval = (char *)&pdbg->r_value[4];
		pval -= 2;
		cnt_num = *pval;
		pval = (char *)&pdbg->r_value[0];
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
				    LIGHT_CNT_FMT);
		for (i = 0; i < cnt_num; i++) {
			cnt_low = *(pval + 2*i);
			cnt_high = *(pval + 2*i + 1);
			cnt = (cnt_low | (cnt_high << 8));
			buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
					    "%5d, ", cnt);
		}
		buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt, "\n");
		sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
	}

	if (!sprd_pdbg_regs_get_once(pdbg, PDBG_R_LPC)) {
		lpc = pdbg->r_value[0];
		if (lpc != PDBG_IGNORE_MAGIC) {
			buf_cnt += scnprintf(regs_msg + buf_cnt, SPRD_LOG_BUF_MAX - buf_cnt,
					    LPC_FMT, lpc);
			sprd_pdbg_regs_msg_print(regs_msg, &buf_cnt, print_out);
		}
	}

	return 0;
}

static void sprd_pdbg_regs_info_show(struct power_debug *pdbg)
{
	char *regs_msg = pdbg->log_buf;

	sprd_pdbg_regs_get(pdbg, regs_msg, true);
}

static void sprd_pdbg_kernel_active_ws_show(void)
{
	struct power_debug *pdbg = sprd_pdbg_get_instance();
	char *active_ws = pdbg->log_buf;

	pm_get_active_wakeup_sources(active_ws, SPRD_LOG_BUF_MAX);
	SPRD_PDBG_INFO("%s\n", active_ws);
}

static int sprd_pdbg_irq_domain_release(void)
{
	struct ws_irq_domain *pos, *tmp;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	write_lock(&pdbg->rw_lock);
	list_for_each_entry_safe(pos, tmp, &pdbg->ws_irq_domain_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
	write_unlock(&pdbg->rw_lock);

	return 0;
}

static int sprd_pdbg_irq_domain_add(int irq_domain_id, void *priv_data)
{
	struct ws_irq_domain *pos, *tmp;
	struct ws_irq_domain *pw;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	read_lock(&pdbg->rw_lock);
	list_for_each_entry_safe(pos, tmp, &pdbg->ws_irq_domain_list, list) {
		if (pos->domain_id != irq_domain_id)
			continue;
		SPRD_PDBG_ERR("%s: ws irq domain(%d) exist\n", __func__, irq_domain_id);
		read_unlock(&pdbg->rw_lock);
		return -EEXIST;
	}
	read_unlock(&pdbg->rw_lock);

	pw = kzalloc(sizeof(struct ws_irq_domain), GFP_KERNEL);
	if (!pw) {
		SPRD_PDBG_ERR("%s: ws irq domain alloc error\n", __func__);
		return -ENOMEM;
	}

	pw->domain_id = irq_domain_id;
	pw->priv_data = priv_data;

	write_lock(&pdbg->rw_lock);
	list_add_tail(&pw->list, &pdbg->ws_irq_domain_list);
	write_unlock(&pdbg->rw_lock);

	return 0;
}

static void sprd_pdbg_get_dt_irq_domain_names(struct power_debug *pdbg)
{
	int i;
	char *irq_domain_names[SPRD_PDBG_WS_DOMAIN_ID_MAX] = {
		"sprd,pdbg-irq-domain-gic",
		"sprd,pdbg-irq-domain-gpio",
		"sprd,pdbg-irq-domain-ana",
		"sprd,pdbg-irq-domain-ana-eic",
		"sprd,pdbg-irq-domain-ap-eic-dbnc",
		"sprd,pdbg-irq-domain-ap-eic-latch",
		"sprd,pdbg-irq-domain-ap-eic-async",
		"sprd,pdbg-irq-domain-ap-eic-sync"
	};
	struct device_node *node = pdbg->dev->of_node;

	for (i = 0; i < SPRD_PDBG_WS_DOMAIN_ID_MAX; i++) {
		if (of_property_read_string(node, irq_domain_names[i],
					    &pdbg->irq_domain_names[i]))
			pdbg->irq_domain_names[i] = NULL;
		else
			SPRD_PDBG_INFO("dt found %s[%s]\n", irq_domain_names[i],
				       pdbg->irq_domain_names[i]);
	}
}

static int sprd_pdbg_irq_domain_init(void)
{
	int i, irq;
	struct irq_desc *desc;
	struct irq_chip *chip;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	sprd_pdbg_get_dt_irq_domain_names(pdbg);

	for_each_irq_desc(irq, desc) {
		chip = irq_desc_get_chip(desc);
		if (!chip)
			continue;
		for (i = 0; i < SPRD_PDBG_WS_DOMAIN_ID_MAX; i++) {
			if (!pdbg->irq_domain_names[i])
				continue;
			if (!strcmp(pdbg->irq_domain_names[i], chip->name)) {
				sprd_pdbg_irq_domain_add(i, desc->irq_data.domain);
				SPRD_PDBG_INFO("match irq domain[%d:%d][%s]\n",
					      irq, i, pdbg->irq_domain_names[i]);
				pdbg->irq_domain_names[i] = NULL;
				break;
			}
		}
	}

	return 0;
}

static int sprd_pdbg_cpu_pm_notifier(struct notifier_block *self,
	unsigned long cmd, void *v)
{

	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg)
		return NOTIFY_DONE;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		SPRD_PDBG_INFO("#---------PDBG DEEP SLEEP START---------#\n");
		sprd_pdbg_regs_info_show(pdbg);
		pdbg->module_log_enable = true;
		SPRD_PDBG_INFO("#---------PDBG DEEP SLEEP END-----------#\n");
		break;
	case CPU_CLUSTER_PM_EXIT:
		SPRD_PDBG_INFO("#---------PDBG WAKEUP SCENE START---------#\n");
		sprd_pdbg_ws_show(pdbg);
		SPRD_PDBG_INFO("#---------PDBG WAKEUP SCENE END-----------#\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int sprd_pdbg_pm_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct power_debug *pdbg = sprd_pdbg_get_instance();
	struct timespec64 sleep_time;
	struct timespec64 total_time;
	struct timespec64 suspend_resume_time;
	u64 suspend_time_ms;

	if (!pdbg)
		return NOTIFY_DONE;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		/* monotonic time since boot */
		pdbg->last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		pdbg->last_stime = ktime_get_boottime();
		break;
	case PM_POST_SUSPEND:
		pdbg->module_log_enable = false;

		/* monotonic time since boot */
		pdbg->curr_monotime = ktime_get();
		 /* monotonic time since boot including the time spent in suspend */
		pdbg->curr_stime = ktime_get_boottime();
		total_time = ktime_to_timespec64(ktime_sub(pdbg->curr_stime, pdbg->last_stime));
		suspend_resume_time =
			ktime_to_timespec64(ktime_sub(pdbg->curr_monotime, pdbg->last_monotime));
		sleep_time = timespec64_sub(total_time, suspend_resume_time);
		suspend_time_ms = timespec64_to_ns(&sleep_time);
		do_div(suspend_time_ms, 1000000);
		SPRD_PDBG_INFO("kernel suspend %llums\n", suspend_time_ms);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int sprd_pdbg_thread(void *data)
{
	struct power_debug *pdbg = (struct power_debug *)data;

	while (pdbg->task) {

		if (kthread_should_stop())
			break;

		SPRD_PDBG_INFO("#---------PDBG LIGHT SLEEP START---------#\n");

		sprd_pdbg_regs_info_show(pdbg);
		sprd_pdbg_kernel_active_ws_show();

		SPRD_PDBG_INFO("#---------PDBG LIGHT SLEEP END-----------#\n");

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(pdbg->scan_interval * (long)HZ);
	}

	return 0;
}

/**
 * sprd_pdbg_start_monitor - start the log output mechanism
 * @pdbg: the pointer of the pdbg core structure of this driver
 */
static int sprd_pdbg_start_monitor(struct power_debug *pdbg)
{
	struct task_struct *ptask;

	if (!pdbg)
		return -EINVAL;

	if (!pdbg->task) {
		ptask = kthread_create(sprd_pdbg_thread, pdbg, "sprd-pdbg-thread");
		if (IS_ERR(ptask)) {
			SPRD_PDBG_ERR("Unable to start kernel thread.\n");
			return PTR_ERR(ptask);
		}
		pdbg->task = ptask;
		wake_up_process(ptask);

		cpu_pm_register_notifier(&pdbg->cpu_pm_notifier_block);
		register_pm_notifier(&pdbg->pm_notifier_block);
	}

	return 0;
}

/**
 * sprd_pdbg_stop_monitor - stop the log output mechanism
 * @pdbg: the pointer of the pdbg core structure of this driver
 */
static void sprd_pdbg_stop_monitor(struct power_debug *pdbg)
{
	if (!pdbg)
		return;

	if (pdbg->task) {
		kthread_stop(pdbg->task);
		pdbg->task = NULL;

		cpu_pm_unregister_notifier(&pdbg->cpu_pm_notifier_block);
		unregister_pm_notifier(&pdbg->pm_notifier_block);
	}
}

static void sprd_pdbg_release(struct power_debug *pdbg)
{
	sprd_pdbg_stop_monitor(pdbg);
	kfree(pdbg);
	pdbg = NULL;
}

void sprd_pdbg_msg_print(const char *format, ...)
{

	struct va_format vaf;
	va_list args;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg || !pdbg->module_log_enable)
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	SPRD_PDBG_INFO("%pV", &vaf);
	va_end(args);
}
EXPORT_SYMBOL_GPL(sprd_pdbg_msg_print);

static int sprd_pdbg_regs_info_read(struct seq_file *m, void *v)
{
	struct power_debug *pdbg = sprd_pdbg_get_instance();
	char *regs_msg = pdbg->log_buf;

	sprd_pdbg_regs_get(pdbg, regs_msg, false);
	seq_printf(m, "%s", regs_msg);

	return 0;
}

static int sprd_pdbg_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_pdbg_regs_info_read, NULL);
}

static const struct proc_ops sprd_pdbg_proc_fops = {
	.proc_open		= sprd_pdbg_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int sprd_pdbg_proc_init(void)
{
	struct proc_dir_entry *dir;
	struct proc_dir_entry *fle;

	dir = proc_mkdir("sprd-pdbg", NULL);
	if (!dir) {
		SPRD_PDBG_ERR("Proc dir create failed\n");
		return -EINVAL;
	}

	fle = proc_create("regs_info", 0444, dir, &sprd_pdbg_proc_fops);
	if (!fle) {
		SPRD_PDBG_ERR("Proc file create failed\n");
		remove_proc_entry("sprd-pdbg", NULL);
		return -EINVAL;
	}

	return 0;
}

static int sprd_pdbg_probe(struct platform_device *pdev)
{
	int result;
	struct sprd_sip_svc_handle *sip;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg) {
		SPRD_PDBG_ERR("fail to get pdbg instance.\n");
		return -ENODEV;
	}

	pdbg->scan_interval = 30;
	pdbg->dev = &pdev->dev;
	pdbg->task = NULL;
	pdbg->module_log_enable = false;
	pdbg->cpu_pm_notifier_block.notifier_call = sprd_pdbg_cpu_pm_notifier;
	pdbg->pm_notifier_block.notifier_call = sprd_pdbg_pm_notifier;
	INIT_LIST_HEAD(&pdbg->ws_irq_domain_list);
	rwlock_init(&pdbg->rw_lock);
	sip = sprd_sip_svc_get_handle();
	pdbg->power_ops = &sip->pwr_ops;
	pdbg->is_32b_machine = (sizeof(unsigned long) < sizeof(u64));

	/* must init after irq domain driver modules load. so should care about modules.load*/
	sprd_pdbg_irq_domain_init();

	sprd_pdbg_proc_init();

	result = sprd_pdbg_start_monitor(pdbg);
	if (result) {
		kfree(pdbg);
		return -ENODEV;
	}

	return 0;
}

/**
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_pdbg_remove(struct platform_device *pdev)
{
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	sprd_pdbg_irq_domain_release();
	sprd_pdbg_release(pdbg);

	return 0;
}


static const struct of_device_id sprd_pdbg_of_match[] = {
{
	.compatible = "sprd,debuglog",
},
{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_of_match);

static struct platform_driver sprd_pddb_driver = {
	.probe = sprd_pdbg_probe,
	.remove = sprd_pdbg_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_pdbg_of_match,
	},
};

module_platform_driver(sprd_pddb_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd power debug driver");
