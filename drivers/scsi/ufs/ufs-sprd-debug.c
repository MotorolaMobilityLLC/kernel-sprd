// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/proc_fs.h>
#include <linux/panic_notifier.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

#include "ufs.h"
#include "ufshcd.h"
#include "ufs-sprd.h"
#include "ufs-sprd-debug.h"

#ifdef CONFIG_SPRD_DEBUG
#define UFS_DBG_ACS_LVL 0660
#else
#define UFS_DBG_ACS_LVL 0440
#endif

static int cmd_record_index = -1;
static bool exceed_max_depth;
static spinlock_t ufs_debug_dump;

/* CMD info buffer */
static struct ufs_event_info uei[UFS_CMD_RECORD_DEPTH];
/* Minidump buffer */
static char *ufs_cmd_history_str;
static struct ufs_hba *hba_tmp;

static const char *ufs_event_str[UFS_MAX_EVENT] = {
	"SCSI Send     ",
	"SCSI Complete ",
	"DM Send       ",
	"DM Complete   ",
	"TM Send       ",
	"TM Complete   ",
	"UIC Send      ",
	"UIC Complete  ",
	"CLK GATE!     ",
	"Host RESET!!! ",
	"INTR ERROR!!! ",
	"Debug Trigger "
};

bool sprd_ufs_debug_is_supported(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (!host)
		return false;

	return host->debug_en;
}

void sprd_ufs_debug_err_dump(struct ufs_hba *hba)
{
#ifdef CONFIG_SPRD_DEBUG
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (!host)
		return;

	if (host->err_panic)
		panic("ufs encountered an error!!!\n");
#endif
}

void ufshcd_common_trace(struct ufs_hba *hba, enum ufs_event_list event, void *data)
{
	int idx;
	unsigned long flags;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (!sprd_ufs_debug_is_supported(hba) && event != UFS_TRACE_DEBUG_TRIGGER)
		return;

	if (data == NULL && event < UFS_TRACE_RESET_AND_RESTORE)
		return;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	if (++cmd_record_index >= UFS_CMD_RECORD_DEPTH) {
		cmd_record_index = 0;
		exceed_max_depth = true;
	}
	idx = cmd_record_index;

	spin_unlock_irqrestore(&ufs_debug_dump, flags);

	uei[idx].event = event;
	uei[idx].cpu = current->cpu;
	uei[idx].pid = current->pid;
	uei[idx].time = ktime_get();

	switch (event) {
	case UFS_TRACE_TM_SEND:
	case UFS_TRACE_TM_COMPLETED:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_tm_cmd_info));
		break;
	case UFS_TRACE_UIC_SEND:
	case UFS_TRACE_UIC_CMPL:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_uic_cmd_info));
		break;
	case UFS_TRACE_CLK_GATE:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_clk_dbg));
		break;
	case UFS_TRACE_DEBUG_TRIGGER:
		uei[idx].flag = host->debug_en;
		uei[idx].panic_f = host->err_panic;
		break;
	case UFS_TRACE_INT_ERROR:
		uei[idx].pkg.ie.errors = hba->errors;
		uei[idx].pkg.ie.uic_error = hba->uic_error;
		break;
	default:
		break;
	}
}

void ufshcd_transfer_event_trace(struct ufs_hba *hba,
				      enum ufs_event_list event,
				      unsigned int tag)
{
	int idx;
	unsigned long flags;
	u32 crypto;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);

	if (!sprd_ufs_debug_is_supported(hba))
		return;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	if (++cmd_record_index >= UFS_CMD_RECORD_DEPTH) {
		cmd_record_index = 0;
		exceed_max_depth = true;
	}
	idx = cmd_record_index;

	spin_unlock_irqrestore(&ufs_debug_dump, flags);

	uei[idx].event = event;
	uei[idx].cpu = current->cpu;
	uei[idx].pid = current->pid;

	if (lrbp->cmd) {
		uei[idx].pkg.ci.opcode = cmd->cmnd[0];
		uei[idx].pkg.ci.tag = tag;
		uei[idx].pkg.ci.lun = lrbp->lun;

		/* specific scsi cmd */
		if ((cmd->cmnd[0] == READ_10) || (cmd->cmnd[0] == WRITE_10)) {
			uei[idx].pkg.ci.lba = scsi_get_lba(cmd);
			uei[idx].pkg.ci.transfer_len =
				be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
			uei[idx].pkg.ci.fua = rq->cmd_flags & REQ_FUA ? true : false;
		} else if (cmd->cmnd[0] == UNMAP) {
			uei[idx].pkg.ci.lba = scsi_get_lba(cmd);
			uei[idx].pkg.ci.transfer_len = blk_rq_bytes(rq);
		} else {
			uei[idx].pkg.ci.lba = -1;
			uei[idx].pkg.ci.transfer_len = -1;
			if (event == UFS_TRACE_SEND) {
				uei[idx].pkg.ci.cmd_len = cmd->cmd_len;
				/* SCSI CDB info */
				memcpy(&uei[idx].pkg.ci.cmnd, cmd->cmnd,
					cmd->cmd_len <= UFS_CDB_SIZE ? cmd->cmd_len : UFS_CDB_SIZE);
			}
		}

		if (event == UFS_TRACE_COMPLETED) {
			uei[idx].time = lrbp->compl_time_stamp;
			uei[idx].pkg.ci.time_cost = lrbp->compl_time_stamp - lrbp->issue_time_stamp;
			/* response info */
			uei[idx].pkg.ci.ocs =
				le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
			uei[idx].pkg.ci.trans_type =
				be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_0) >> 24;
			uei[idx].pkg.ci.scsi_stat =
				be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_1) & MASK_SCSI_STATUS;
			uei[idx].pkg.ci.sd_size = min_t(int, UFS_SENSE_SIZE,
				be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len));
			if (uei[idx].pkg.ci.sd_size)
				memcpy(&uei[idx].pkg.ci.sense_data,
					&lrbp->ucd_rsp_ptr->sr.sense_data, UFS_SENSE_SIZE);
		} else {
			/* inline crypto info */
			crypto = le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_0) &
				UTP_REQ_DESC_CRYPTO_ENABLE_CMD;
			uei[idx].pkg.ci.crypto_en = crypto ? 1 : 0;
			uei[idx].pkg.ci.keyslot = crypto ? hba->lrb[tag].crypto_key_slot : 0;

			uei[idx].time = lrbp->issue_time_stamp;
		}
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
		   lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
		uei[idx].pkg.dmi.tag = tag;
		uei[idx].pkg.dmi.lun = lrbp->lun;

		if (event == UFS_TRACE_DEV_COMPLETED) {
			uei[idx].time = lrbp->compl_time_stamp;
			uei[idx].pkg.dmi.time_cost =
				lrbp->compl_time_stamp - lrbp->issue_time_stamp;
			/* response info */
			uei[idx].pkg.dmi.ocs =
				le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
			memcpy(&uei[idx].pkg.dmi.rsp, lrbp->ucd_rsp_ptr,
			       sizeof(struct utp_upiu_rsp));
		} else {
			uei[idx].time = lrbp->issue_time_stamp;
			memcpy(&uei[idx].pkg.dmi.req, lrbp->ucd_req_ptr,
			       sizeof(struct utp_upiu_req));
		}
	}
}

static void ufs_sprd_cmd_history_dump_trace(u32 dump_req, struct seq_file *m, bool dump)
{
	int ptr;
	int i = 0;
	int actual_dump_num;
	unsigned long flags;
	char *dump_pos = NULL;
	ktime_t ktime;
	ktime_t cur_time;
	char b[120];
	int k, n;
	int sb = (int)sizeof(b);
	int transaction_type;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	if (dump == true)
		dump_pos = (char *) ufs_cmd_history_str;

	if (!!hba_tmp)
		PRINT_SWITCH(m, dump_pos, "[UFS] ufs_hba=0x%lx\n\n", (unsigned long)hba_tmp);

	if (exceed_max_depth == true)
		actual_dump_num = UFS_CMD_RECORD_DEPTH;
	else if (cmd_record_index != -1)
		actual_dump_num = cmd_record_index + 1;
	else {
		pr_info("%s: NO UFS cmd was recorded\n", __func__);
		return;
	}

	if (dump_req)
		actual_dump_num = min_t(u32, dump_req, actual_dump_num);

	ptr = ((cmd_record_index + 1) / actual_dump_num) ?
		(cmd_record_index + 1 - actual_dump_num) :
		(cmd_record_index + 1 + UFS_CMD_RECORD_DEPTH - actual_dump_num);

	PRINT_SWITCH(m, dump_pos, "[UFS] CMD History: total_dump_num=%d\n",
		     actual_dump_num);

	for (; i < actual_dump_num; i++, ptr++, k = 0, n = 0) {
		if (ptr == UFS_CMD_RECORD_DEPTH)
			ptr = 0;

		PRINT_SWITCH(m, dump_pos, "[%lld.%09lld][T%4d@C%d][%s]:",
			     uei[ptr].time / NSEC_PER_SEC, uei[ptr].time % NSEC_PER_SEC,
			     uei[ptr].pid, uei[ptr].cpu, ufs_event_str[uei[ptr].event]);

		switch (uei[ptr].event) {
		case UFS_TRACE_SEND:
			/* CDB info */
			if (!((uei[ptr].pkg.ci.opcode == READ_10) ||
				(uei[ptr].pkg.ci.opcode == WRITE_10) ||
				(uei[ptr].pkg.ci.opcode == UNMAP)))
				for (; k < uei[ptr].pkg.ci.cmd_len && n < sb; ++k)
					n += scnprintf(b + n, sb - n, "%02x ",
						       (u32)uei[ptr].pkg.ci.cmnd[k]);

			PRINT_SWITCH(m, dump_pos,
			"opc:0x%2x,tag:%2d,lun:0x%2x,LBA:%10lld,len:%6d,ICE:%s,KS:%2d,FUA:%s,CDB:(%s)\n",
			uei[ptr].pkg.ci.opcode,
			uei[ptr].pkg.ci.tag,
			uei[ptr].pkg.ci.lun,
			(u64)uei[ptr].pkg.ci.lba,
			uei[ptr].pkg.ci.transfer_len,
			uei[ptr].pkg.ci.crypto_en ? "ON " : "OFF",
			uei[ptr].pkg.ci.keyslot,
			uei[ptr].pkg.ci.fua ? "ON " : "OFF",
			n ? b : "NO RECORD");
			break;
		case UFS_TRACE_COMPLETED:
			/* sense data info */
			if (uei[ptr].pkg.ci.ocs == SUCCESS &&
			    uei[ptr].pkg.ci.trans_type == UPIU_TRANSACTION_RESPONSE &&
			    (uei[ptr].pkg.ci.scsi_stat & ~(SAM_STAT_CHECK_CONDITION |
							   SAM_STAT_TASK_SET_FULL |
							   SAM_STAT_BUSY)) != 0 &&
			    uei[ptr].pkg.ci.sd_size)
				for (; k < uei[ptr].pkg.ci.sd_size && n < sb; ++k)
					n += scnprintf(b + n, sb - n, "%02x ",
					   (u32)uei[ptr].pkg.ci.sense_data[k]);

			PRINT_SWITCH(m, dump_pos,
"opc:0x%2x,tag:%2d,lun:0x%2x,LBA:%10lld,len:%6d,LAT:%lluns,OCS:0x%2x,TT:0x%2x,SS:0x%2x,SD:(%s)\n",
			uei[ptr].pkg.ci.opcode,
			uei[ptr].pkg.ci.tag,
			uei[ptr].pkg.ci.lun,
			(u64)uei[ptr].pkg.ci.lba,
			uei[ptr].pkg.ci.transfer_len,
			(u64)uei[ptr].pkg.ci.time_cost,
			uei[ptr].pkg.ci.ocs,
			uei[ptr].pkg.ci.trans_type, uei[ptr].pkg.ci.scsi_stat,
			uei[ptr].pkg.ci.sd_size ? b : "NO SENSEDATA");
			break;
		case UFS_TRACE_DEV_SEND:
			PRINT_SWITCH(m, dump_pos,
			"opc:0x%2x,tag:%2d,lun:0x%2x,idn:0x%x,idx:0x%x,sel:0x%x,LAT:%lluns\n",
			uei[ptr].pkg.dmi.req.qr.opcode,
			uei[ptr].pkg.dmi.tag,
			uei[ptr].pkg.dmi.lun,
			uei[ptr].pkg.dmi.req.qr.idn,
			uei[ptr].pkg.dmi.req.qr.index,
			uei[ptr].pkg.dmi.req.qr.selector,
			(u64)uei[ptr].pkg.dmi.time_cost);
			break;
		case UFS_TRACE_DEV_COMPLETED:
			transaction_type =
				be32_to_cpu(uei[ptr].pkg.dmi.rsp.header.dword_0) >> 24;
			PRINT_SWITCH(m, dump_pos,
"opc:0x%2x,tag:%2d,lun:0x%2x,idn:0x%x,idx:0x%x,sel:0x%x,LAT:%lluns,OCS:0x%2x,TT:0x%2x,query_rsp:%4d\n",
			uei[ptr].pkg.dmi.rsp.qr.opcode,
			uei[ptr].pkg.dmi.tag,
			uei[ptr].pkg.dmi.lun,
			uei[ptr].pkg.dmi.rsp.qr.idn,
			uei[ptr].pkg.dmi.rsp.qr.index,
			uei[ptr].pkg.dmi.rsp.qr.selector,
			(u64)uei[ptr].pkg.dmi.time_cost,
			uei[ptr].pkg.dmi.ocs,
			transaction_type,
			transaction_type == UPIU_TRANSACTION_QUERY_RSP ?
			(int)((be32_to_cpu(uei[ptr].pkg.dmi.rsp.header.dword_1) &
			      MASK_RSP_UPIU_RESULT) >> UPIU_RSP_CODE_OFFSET) : -1);
			break;
		case UFS_TRACE_TM_SEND:
			PRINT_SWITCH(m, dump_pos,
			"tm_func:0x%2x,param1:0x%8x,param2:0x%8x\n",
			uei[ptr].pkg.tmi.tm_func,
			uei[ptr].pkg.tmi.param1,
			uei[ptr].pkg.tmi.param2);
			break;
		case UFS_TRACE_TM_COMPLETED:
			PRINT_SWITCH(m, dump_pos,
			"tm_func:0x%2x,param1:0x%8x,param2:0x%8x,OCS:0x%2x\n",
			uei[ptr].pkg.tmi.tm_func,
			uei[ptr].pkg.tmi.param1,
			uei[ptr].pkg.tmi.param2,
			uei[ptr].pkg.tmi.ocs);
			break;
		case UFS_TRACE_UIC_SEND:
		case UFS_TRACE_UIC_CMPL:
			PRINT_SWITCH(m, dump_pos,
			"cmd:0x%2x,arg1:0x%x,arg2:0x%x,arg3:0x%x\n",
			uei[ptr].pkg.uci.cmd,
			uei[ptr].pkg.uci.argu1,
			uei[ptr].pkg.uci.argu2,
			uei[ptr].pkg.uci.argu3);
			break;
		case UFS_TRACE_CLK_GATE:
			PRINT_SWITCH(m, dump_pos, "status:%s, req_clk:%s\n",
			uei[ptr].pkg.cd.status ? "POST" : "PRE ",
			uei[ptr].pkg.cd.on ? "ON " : "OFF");
			break;
		case UFS_TRACE_RESET_AND_RESTORE:
			PRINT_SWITCH(m, dump_pos, "\n");
			break;
		case UFS_TRACE_DEBUG_TRIGGER:
			PRINT_SWITCH(m, dump_pos, "debug_on:%d, err_panic:%d\n",
			uei[ptr].flag,
			uei[ptr].panic_f);
			break;
		case UFS_TRACE_INT_ERROR:
			PRINT_SWITCH(m, dump_pos, "err:0x%08x, uic_err:0x%08x\n",
			uei[ptr].pkg.ie.errors,
			uei[ptr].pkg.ie.uic_error);
			break;
		default:
			break;
		}
	}
	ktime = ktime_get();
	cur_time = ktime_get_boottime();
	PRINT_SWITCH(m, dump_pos, "time:%lld.%09lld, current_time:%lld.%09lld\n",
		     ktime / NSEC_PER_SEC, ktime % NSEC_PER_SEC,
		     cur_time / NSEC_PER_SEC, cur_time % NSEC_PER_SEC);
	if (dump == 1)
		PRINT_SWITCH(m, dump_pos, "Dump buffer used:0x%x/(0x%x)\n",
			     (u32)(dump_pos - ufs_cmd_history_str), DUMP_BUFFER_S);

	spin_unlock_irqrestore(&ufs_debug_dump, flags);
}

static int ufs_sprd_dbg_info_show(struct seq_file *m, void *v)
{
	seq_puts(m, "========== UFS Debug Dump START ==========\n\n");

	ufs_sprd_cmd_history_dump_trace(UFS_CMD_RECORD_DEPTH, m, 0);

	seq_puts(m, "\n=========== UFS Debug Dump END ===========\n");

	return 0;
}

static int ufs_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_sprd_dbg_info_show, inode->i_private);
}

static const struct proc_ops ufs_debug_fops = {
	.proc_open = ufs_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_dbg_on_proc_show(struct seq_file *m, void *v)
{
	struct ufs_sprd_host *host = m->private;

	seq_printf(m, "debug status : %d\n", host->debug_en);
	return 0;
}

static int ufs_dbg_on_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_dbg_on_proc_show, PDE_DATA(inode));
}

static ssize_t ufs_dbg_on_proc_write(struct file *file,
				      const char __user *buffer,
				      size_t count, loff_t *pos)
{
	struct ufs_sprd_host *host = PDE_DATA(file_inode(file));

	if (kstrtobool_from_user(buffer, count, &host->debug_en))
		return -EINVAL;

	ufshcd_common_trace(host->hba, UFS_TRACE_DEBUG_TRIGGER, NULL);
	return count;
}

static const struct proc_ops ufs_debug_on_fops = {
	.proc_open = ufs_dbg_on_proc_open,
	.proc_write = ufs_dbg_on_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_err_panic_proc_show(struct seq_file *m, void *v)
{
	struct ufs_sprd_host *host = m->private;

	seq_puts(m, "When ufs encounters an error, system will trigger crash for debug.\n");
	seq_puts(m, "---this will only work if you set it on USERDEBUG PAC.\n");
	seq_printf(m, "UFS err panic status : %d\n", host->err_panic);
	return 0;
}

static int ufs_err_panic_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_err_panic_proc_show, PDE_DATA(inode));
}

static ssize_t ufs_err_panic_proc_write(struct file *file,
				      const char __user *buffer,
				      size_t count, loff_t *pos)
{
	struct ufs_sprd_host *host = PDE_DATA(file_inode(file));

	if (kstrtobool_from_user(buffer, count, &host->err_panic))
		return -EINVAL;

	ufshcd_common_trace(host->hba, UFS_TRACE_DEBUG_TRIGGER, NULL);
	return count;
}

static const struct proc_ops ufs_err_panic_fops = {
	.proc_open = ufs_err_panic_proc_open,
	.proc_write = ufs_err_panic_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sprd_ufs_panic_handler(struct notifier_block *self,
			       unsigned long val, void *reason)
{
	if (ufs_cmd_history_str)
		ufs_sprd_cmd_history_dump_trace(UFS_CMD_RECORD_DEPTH, NULL, 1);
	return NOTIFY_DONE;
}

static struct notifier_block sprd_ufs_event_nb = {
	.notifier_call	= sprd_ufs_panic_handler,
	.priority	= INT_MAX,
};

int ufs_sprd_debug_init(struct ufs_hba *hba)
{
	struct proc_dir_entry *ufs_dir;
	struct proc_dir_entry *prEntry;
	struct ufs_sprd_host *host;

	if (!hba || !hba->priv) {
		pr_info("%s: NULL host exiting\n", __func__);
		return -EINVAL;
	}
	host = hba->priv;
	hba_tmp = hba;

	host->err_panic = UFS_DEBUG_ERR_PANIC_DEF;
	host->debug_en = UFS_DEBUG_ON_DEF;

	spin_lock_init(&ufs_debug_dump);

	ufs_dir = proc_mkdir("ufs", NULL);
	if (!ufs_dir) {
		pr_err("%s: failed to create /proc/ufs\n",
			__func__);
		return -1;
	}

	/* cmd_history */
	prEntry = proc_create_data("cmd_history", 0440, ufs_dir, &ufs_debug_fops, host);
	if (!prEntry)
		pr_info("%s: failed to create /proc/ufs/debug_info\n",
			__func__);

	prEntry = proc_create_data("debug_on", UFS_DBG_ACS_LVL, ufs_dir,
			      &ufs_debug_on_fops, host);
	if (!prEntry)
		pr_info("%s: failed to create /proc/ufs/debug_on\n",
			__func__);

	prEntry = proc_create_data("err_panic", UFS_DBG_ACS_LVL, ufs_dir,
			      &ufs_err_panic_fops, host);
	if (!prEntry)
		pr_info("%s: failed to create /proc/ufs/err_panic\n",
			__func__);

	ufs_cmd_history_str = devm_kzalloc(hba->dev, DUMP_BUFFER_S, GFP_KERNEL);
	if (!ufs_cmd_history_str) {
		dev_err(hba->dev, "%s devm_kzalloc dump buffer fail!\n",
			__func__);
		return -ENOMEM;
	}

	if (minidump_save_extend_information("ufs_cmd_history",
					     __pa(ufs_cmd_history_str),
					     __pa(ufs_cmd_history_str + DUMP_BUFFER_S)))
		pr_info("%s: failed to link ufs_cmd_history to minidump\n",
			__func__);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &sprd_ufs_event_nb);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_sprd_debug_init);
