// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/err.h>
#include <linux/string.h>
#include <asm/unaligned.h>
#include <linux/proc_fs.h>
#include <linux/panic_notifier.h>
#include <linux/sched/clock.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

#include "ufs.h"
#include "ufshcd.h"
#include "unipro.h"
#include "ufs-sprd.h"
#include "ufs-sysfs.h"
#include "ufs-sprd-debug.h"

#ifdef CONFIG_SPRD_DEBUG
#define UFS_DBG_ACS_LVL 0660
#else
#define UFS_DBG_ACS_LVL 0440
#endif

int cmd_record_index = -1;
static bool exceed_max_depth;
static spinlock_t ufs_debug_dump;

/* ufs error code count */
struct ufs_uic_err_code_cnt ufs_uic_err_code_cnt;
/* CMD info buffer */
struct ufs_event_info uei[UFS_CMD_RECORD_DEPTH];
/* Minidump buffer */
char *ufs_dump_buffer;
struct ufs_hba *hba_tmp;
struct ufs_err_cnt ufs_err_cnt;
struct ufs_pa_layer_attr dts_pwr_info;
struct ufs_pa_layer_attr compare_pwr_info;
static const char *ufs_event_str[UFS_MAX_EVENT] = {
	"SCSI Send     ",
	"SCSI Complete ",
	"SCSI TIMEOUT!!",
	"DM Send       ",
	"DM Complete   ",
	"TM Send       ",
	"TM Complete   ",
	"TM ERR!!!     ",
	"UIC Send      ",
	"UIC Complete  ",
	"CLK GATE!     ",
	"EVT UPDATE!   ",
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

void sprd_ufs_print_err_cnt(struct ufs_hba *hba)
{
	dev_err(hba->dev, "sprd_reset: total cnt=%llu\n", ufs_err_cnt.sprd_reset_cnt);
	dev_err(hba->dev, "line_reset: total cnt=%llu\n", ufs_err_cnt.line_reset_cnt);
	dev_err(hba->dev, "scsi_timeout: total cnt=%llu\n", ufs_err_cnt.scsi_timeout_cnt);
}

static void sprd_ufs_cmd_record(struct ufs_hba *hba, enum ufs_event_list event,
			int idx, unsigned int tag)
{
	u32 crypto;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd;
	struct request *rq;

	if (lrbp->cmd) {
		cmd = lrbp->cmd;
		rq = scsi_cmd_to_rq(cmd);
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

			if (event == UFS_TRACE_SEND) {
				uei[idx].pkg.ci.keyslot =
					crypto ? hba->lrb[tag].crypto_key_slot : 0;
			} else {
				/* UFS_TREAC_SCSI_TIME_OUT */
				uei[idx].pkg.ci.rq = rq;
				uei[idx].pkg.ci.deadline = rq->deadline;
			}
		}
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
		   lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
		uei[idx].pkg.dmi.tag = tag;
		uei[idx].pkg.dmi.lun = lrbp->lun;

		if (event == UFS_TRACE_DEV_COMPLETED) {
			uei[idx].pkg.dmi.time_cost =
				lrbp->compl_time_stamp - lrbp->issue_time_stamp;
			/* response info */
			uei[idx].pkg.dmi.ocs =
				le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
			memcpy(&uei[idx].pkg.dmi.rsp, lrbp->ucd_rsp_ptr,
			       sizeof(struct utp_upiu_rsp));
		} else {
			memcpy(&uei[idx].pkg.dmi.req, lrbp->ucd_req_ptr,
			       sizeof(struct utp_upiu_req));
		}
	}

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
	uei[idx].time = local_clock();
	uei[idx].jiffies = jiffies;

	switch (event) {
	case UFS_TRECE_SCSI_TIME_OUT:
	case UFS_TRACE_SEND:
	case UFS_TRACE_COMPLETED:
	case UFS_TRACE_DEV_SEND:
	case UFS_TRACE_DEV_COMPLETED:
		sprd_ufs_cmd_record(hba, event, idx, *(unsigned int *)data);
		break;
	case UFS_TRACE_TM_SEND:
	case UFS_TRACE_TM_COMPLETED:
	case UFS_TRACE_TM_ERR:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_tm_cmd_info));
		break;
	case UFS_TRACE_UIC_SEND:
	case UFS_TRACE_UIC_CMPL:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_uic_cmd_info));
		break;
	case UFS_TRACE_CLK_GATE:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_clk_dbg));
		break;
	case UFS_TRACE_EVT:
		memcpy(&uei[idx].pkg, data, sizeof(struct ufs_evt_dbg));
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

void ufs_sprd_cmd_history_dump(u32 dump_req, struct seq_file *m, char **dump_pos)
{
	int ptr;
	int i = 0;
	int actual_dump_num;
	unsigned long flags;
	char b[120];
	int k = 0;
	int n = 0;
	int sb = (int)sizeof(b);
	int transaction_type;

	if (cmd_record_index == -1)
		return;

	spin_lock_irqsave(&ufs_debug_dump, flags);

	if (exceed_max_depth == true)
		actual_dump_num = UFS_CMD_RECORD_DEPTH;
	else
		actual_dump_num = cmd_record_index + 1;

	if (dump_req)
		actual_dump_num = min_t(u32, dump_req, actual_dump_num);

	ptr = ((cmd_record_index + 1) / actual_dump_num) ?
		(cmd_record_index + 1 - actual_dump_num) :
		(cmd_record_index + 1 + UFS_CMD_RECORD_DEPTH - actual_dump_num);

	PRINT_SWITCH(m, dump_pos, "\n[UFS] CMD History: total_dump_num=%d\n",
		     actual_dump_num);

	for (; i < actual_dump_num; i++, ptr++, k = 0, n = 0) {
		if (ptr == UFS_CMD_RECORD_DEPTH)
			ptr = 0;

		PRINT_SWITCH(m, dump_pos, "[%lld.%09lld][%lld][T%5d@C%d][%s]:",
			     uei[ptr].time / NSEC_PER_SEC, uei[ptr].time % NSEC_PER_SEC,
			     uei[ptr].jiffies, uei[ptr].pid, uei[ptr].cpu,
			     ufs_event_str[uei[ptr].event]);

		switch (uei[ptr].event) {
		case UFS_TRECE_SCSI_TIME_OUT:
			/* CDB info */
			if (!((uei[ptr].pkg.ci.opcode == READ_10) ||
				(uei[ptr].pkg.ci.opcode == WRITE_10) ||
				(uei[ptr].pkg.ci.opcode == UNMAP)))
				for (; k < uei[ptr].pkg.ci.cmd_len && n < sb; ++k)
					n += scnprintf(b + n, sb - n, "%02x ",
						       (u32)uei[ptr].pkg.ci.cmnd[k]);

			PRINT_SWITCH(m, dump_pos,
			"opc:0x%2x,tag:%2d,LBA:%10lld,ICE:%s,(dl:%lld,rq:0x%lx),CDB:(%s)\n",
			uei[ptr].pkg.ci.opcode,
			uei[ptr].pkg.ci.tag,
			(u64)uei[ptr].pkg.ci.lba,
			uei[ptr].pkg.ci.crypto_en ? "ON " : "OFF",
			uei[ptr].pkg.ci.deadline,
			(unsigned long)uei[ptr].pkg.ci.rq,
			n ? b : "NO RECORD");
			break;
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
			    (uei[ptr].pkg.ci.scsi_stat & (SAM_STAT_CHECK_CONDITION |
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
			n ? b : "NO SENSEDATA");
			break;
		case UFS_TRACE_DEV_SEND:
			PRINT_SWITCH(m, dump_pos,
			"opc:0x%2x,tag:%2d,lun:0x%2x,idn:0x%x,idx:0x%x,sel:0x%x\n",
			uei[ptr].pkg.dmi.req.qr.opcode,
			uei[ptr].pkg.dmi.tag,
			uei[ptr].pkg.dmi.lun,
			uei[ptr].pkg.dmi.req.qr.idn,
			uei[ptr].pkg.dmi.req.qr.index,
			uei[ptr].pkg.dmi.req.qr.selector);
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
		case UFS_TRACE_TM_ERR:
			PRINT_SWITCH(m, dump_pos,
			"tm_func:0x%2x,param1:0x%8x,param2:0x%8x,OCS:0x%2x\n",
			uei[ptr].pkg.tmi.tm_func,
			uei[ptr].pkg.tmi.param1,
			uei[ptr].pkg.tmi.param2,
			uei[ptr].pkg.tmi.ocs);
			break;
		case UFS_TRACE_UIC_CMPL:
			if (uei[ptr].pkg.uci.dwc_hc_ee_h8_compl) {
				PRINT_SWITCH(m, dump_pos, "cmd:0x%2x\n", UIC_CMD_DME_HIBER_ENTER);
				break;
			}
			fallthrough;
		case UFS_TRACE_UIC_SEND:
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
		case UFS_TRACE_EVT:
			PRINT_SWITCH(m, dump_pos, "id:%2d, data:0x%08x\n",
			uei[ptr].pkg.evt.id,
			uei[ptr].pkg.evt.val);
			break;
		default:
			break;
		}
	}

	spin_unlock_irqrestore(&ufs_debug_dump, flags);
}

static void ufs_sprd_base_info_dump(struct seq_file *m, char **dump_pos)
{
	struct scsi_device *sdev_ufs;
	struct ufs_hba *hba = hba_tmp;

	PRINT_SWITCH(m, dump_pos, "ufs_hba=0x%lx\n", (unsigned long)hba_tmp);

	PRINT_SWITCH(m, dump_pos, "PM in progress=%d, sys. suspended=%d\n",
			hba->pm_op_in_progress, hba->is_sys_suspended);

	PRINT_SWITCH(m, dump_pos, "[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%d, %d], rate = %d\n",
			hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
			hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
			hba->pwr_info.pwr_rx, hba->pwr_info.pwr_tx,
			hba->pwr_info.hs_rate);

	sdev_ufs = hba->sdev_ufs_device;
	if (sdev_ufs)
		PRINT_SWITCH(m, dump_pos, "[UFS] dev info: %.8s %.16s rev %.4s\n",
			     sdev_ufs->vendor, sdev_ufs->model, sdev_ufs->rev);
}

static int ufs_sprd_dbg_info_show(struct seq_file *m, void *v)
{
	/* dump ufs base info */
	if (!!hba_tmp)
		ufs_sprd_base_info_dump(m, NULL);

	/* dump ufs cmd history */
	ufs_sprd_cmd_history_dump(UFS_CMD_RECORD_DEPTH, m, NULL);

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

void ufs_sprd_update_err_cnt(struct ufs_hba *hba, u32 reg, enum err_type type)
{
	switch (type) {
	case UFS_SPRD_RESET:
		ufs_err_cnt.sprd_reset_cnt++;
		break;
	case UFS_LINE_RESET:
		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			ufs_err_cnt.line_reset_cnt++;
		break;
	case UFS_SCSI_TIMEOUT:
		ufs_err_cnt.scsi_timeout_cnt++;
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(ufs_sprd_update_err_cnt);

void ufs_sprd_update_uic_err_cnt(struct ufs_hba *hba, u32 reg, enum ufs_event_type evt)
{
	int i = 0;
	unsigned long reg_err = reg;

	switch (evt) {
	case UFS_EVT_PA_ERR:
		for_each_set_bit(i, &reg_err, UIC_ERR_PA_MAX) {
			ufs_uic_err_code_cnt.pa_err_cnt[i]++;
		}
		break;
	case UFS_EVT_DL_ERR:
		for_each_set_bit(i, &reg_err, UIC_ERR_DL_MAX) {
			ufs_uic_err_code_cnt.dl_err_cnt[i]++;
		}
		break;
	case UFS_EVT_NL_ERR:
		for_each_set_bit(i, &reg_err, UIC_ERR_NL_MAX) {
			ufs_uic_err_code_cnt.nl_err_cnt[i]++;
		}
		break;
	case UFS_EVT_TL_ERR:
		for_each_set_bit(i, &reg_err, UIC_ERR_TL_MAX) {
			ufs_uic_err_code_cnt.tl_err_cnt[i]++;
		}
		break;
	case UFS_EVT_DME_ERR:
		for_each_set_bit(i, &reg_err, UIC_ERR_DME_MAX) {
			ufs_uic_err_code_cnt.dme_err_cnt[i]++;
		}
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(ufs_sprd_update_uic_err_cnt);

static void ufs_uic_err_cnt_dump(struct ufs_hba *hba, struct seq_file *m, char **dump_pos)
{
	if (dump_pos)
		PRINT_SWITCH(m, dump_pos, "\n[UFS] ERR CNT :\n");

	PRINT_SWITCH(m, dump_pos, "pa_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_PA_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "pa_err:PHY error on lane 0 cnt=%llu\n",
			ufs_uic_err_code_cnt.pa_err_cnt[UFS_EVT_PA_PHY_LINE0_ERR]);
	PRINT_SWITCH(m, dump_pos, "pa_err:PHY error on lane 1 cnt=%llu\n",
			ufs_uic_err_code_cnt.pa_err_cnt[UFS_EVT_PA_PHY_LINE1_ERR]);
	PRINT_SWITCH(m, dump_pos, "pa_err:generic PHY adapter error cnt=%llu\n",
			ufs_uic_err_code_cnt.pa_err_cnt[UFS_EVT_PA_GRNERIC_PA_ERR]);

	PRINT_SWITCH(m, dump_pos, "dl_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_DL_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "dl_err:NAC_RECEIVED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_NAC_RECEIVED]);
	PRINT_SWITCH(m, dump_pos, "dl_err:TCx_REPLAY_TIMER_EXPIRED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_TCx_REPLAY_TIMER_EXPIRED]);
	PRINT_SWITCH(m, dump_pos, "dl_err:AFCx_REQUEST_TIMER_EXPIRED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_AFCx_REQUEST_TIMER_EXPIRED]);
	PRINT_SWITCH(m, dump_pos, "dl_err:FCx_PROTECTION_TIMER_EXPIRED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_FCx_PROTECTION_TIMER_EXPIRED]);
	PRINT_SWITCH(m, dump_pos, "dl_err:CRC_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_CRC_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:RX_BUFFER_OVERFLOW cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_RX_BUFFER_OVERFLOW]);
	PRINT_SWITCH(m, dump_pos, "dl_err:MAX_FRAME_LENGTH_EXCEEDED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_MAX_FRAME_LENGTH_EXCEEDED]);
	PRINT_SWITCH(m, dump_pos, "dl_err:WRONG_SEQUENCE_NUMBER cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_WRONG_SEQUENCE_NUMBER]);
	PRINT_SWITCH(m, dump_pos, "dl_err:AFC_FRAME_SYNTAX_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_AFC_FRAME_SYNTAX_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:NAC_FRAME_SYNTAX_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_NAC_FRAME_SYNTAX_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:EOF_SYNTAX_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_EOF_SYNTAX_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:FRAME_SYNTAX_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_FRAME_SYNTAX_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:BAD_CTRL_SYMBOL_TYPE cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_BAD_CTRL_SYMBOL_TYPE]);
	PRINT_SWITCH(m, dump_pos, "dl_err:PA_INIT_ERROR cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_PA_INIT_ERROR]);
	PRINT_SWITCH(m, dump_pos, "dl_err:PA_ERROR_IND_RECEIVED cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_PA_ERROR_IND_RECEIVED]);
	if ((hba->ufs_version & 0xF00) >= 0x300) {
		PRINT_SWITCH(m, dump_pos, "dl_err:PA_INIT cnt=%llu\n",
			ufs_uic_err_code_cnt.dl_err_cnt[UFS_EVT_DL_PA_INIT]);
	}

	PRINT_SWITCH(m, dump_pos, "nl_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_NL_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "nl_err:UNSUPPORTED_HEADER_TYPE cnt=%llu\n",
			ufs_uic_err_code_cnt.nl_err_cnt[UFS_EVT_NL_UNSUPPORTED_HEADER_TYPE]);
	PRINT_SWITCH(m, dump_pos, "nl_err:BAD_DEVICEID_ENC cnt=%llu\n",
			ufs_uic_err_code_cnt.nl_err_cnt[UFS_EVT_NL_BAD_DEVICEID_ENC]);
	PRINT_SWITCH(m, dump_pos, "nl_err:LHDR_TRAP_PACKET_DROPPING cnt=%llu\n",
			ufs_uic_err_code_cnt.nl_err_cnt[UFS_EVT_NL_LHDR_TRAP_PACKET_DROPPING]);
	PRINT_SWITCH(m, dump_pos, "nl_err:MAX_N_PDU_LENGTH_EXCEEDED cnt=%llu\n",
			ufs_uic_err_code_cnt.nl_err_cnt[UFS_EVT_NL_MAX_N_PDU_LENGTH_EXCEEDED]);

	PRINT_SWITCH(m, dump_pos, "tl_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_TL_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "tl_err:UNSUPPORTED_HEADER_TYPE cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_UNSUPPORTED_HEADER_TYPE]);
	PRINT_SWITCH(m, dump_pos, "tl_err:UNKNOWN_CPORTID cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_UNKNOWN_CPORTID]);
	PRINT_SWITCH(m, dump_pos, "tl_err:NO_CONNECTION_RX cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_NO_CONNECTION_RX]);
	PRINT_SWITCH(m, dump_pos, "tl_err:CONTROLLED_SEGMENT_DROPPING cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_CONTROLLED_SEGMENT_DROPPING]);
	PRINT_SWITCH(m, dump_pos, "tl_err:BAD_TC cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_BAD_TC]);
	PRINT_SWITCH(m, dump_pos, "tl_err:E2E_CREDIT_OVERFLOW cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_E2E_CREDIT_OVERFLOW]);
	PRINT_SWITCH(m, dump_pos, "tl_err:SAFETY_VALVE_DROPPING cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_SAFETY_VALVE_DROPPING]);
	PRINT_SWITCH(m, dump_pos, "tl_err:MAX_T_PDU_LENGTH_EXCEEDED cnt=%llu\n",
			ufs_uic_err_code_cnt.tl_err_cnt[UFS_EVT_TL_MAX_T_PDU_LENGTH_EXCEEDED]);

	PRINT_SWITCH(m, dump_pos, "dme_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_DME_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "dme_err:GENERIC_ERR cnt=%llu\n",
			ufs_uic_err_code_cnt.dme_err_cnt[UFS_EVT_DME_GENERIC_ERR]);
	PRINT_SWITCH(m, dump_pos, "dme_err:QOS_FROM_TX_DETECTED cnt=%llu\n",
			ufs_uic_err_code_cnt.dme_err_cnt[UFS_EVT_DME_QOS_FROM_TX_DETECTED]);
	PRINT_SWITCH(m, dump_pos, "dme_err:QOS_FROM_RX_DETECTED cnt=%llu\n",
			ufs_uic_err_code_cnt.dme_err_cnt[UFS_EVT_DME_QOS_FROM_RX_DETECTED]);
	PRINT_SWITCH(m, dump_pos, "dme_err:QOS_FROM_PA_INIT_DETECTED cnt=%llu\n",
			ufs_uic_err_code_cnt.dme_err_cnt[UFS_EVT_DME_QOS_FROM_PA_INIT_DETECTED]);

	PRINT_SWITCH(m, dump_pos, "auto_h8_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_AUTO_HIBERN8_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "fatal_err:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_FATAL_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "link_startup_fail:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_LINK_STARTUP_FAIL].cnt);
	PRINT_SWITCH(m, dump_pos, "resume_fail:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_RESUME_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "suspend_fail:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_SUSPEND_ERR].cnt);
	PRINT_SWITCH(m, dump_pos, "dev_reset:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_DEV_RESET].cnt);
	PRINT_SWITCH(m, dump_pos, "host_reset:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_HOST_RESET].cnt);
	PRINT_SWITCH(m, dump_pos, "task_abort:total cnt=%llu\n",
			hba->ufs_stats.event[UFS_EVT_ABORT].cnt);
	PRINT_SWITCH(m, dump_pos, "sprd_reset:total cnt=%llu\n", ufs_err_cnt.sprd_reset_cnt);
	PRINT_SWITCH(m, dump_pos, "line_reset:total cnt=%llu\n", ufs_err_cnt.line_reset_cnt);
	PRINT_SWITCH(m, dump_pos, "scsi_timeout:total cnt=%llu\n", ufs_err_cnt.scsi_timeout_cnt);
}

static int uic_err_cnt_show(struct seq_file *m, void *v)
{
	struct ufs_hba *hba = dev_get_drvdata((struct device *)m->private);

	ufs_uic_err_cnt_dump(hba, m, NULL);

	return 0;
}

static int uic_err_cnt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uic_err_cnt_show, PDE_DATA(inode));
}

static const struct proc_ops uic_err_cnt_fops = {
	.proc_open = uic_err_cnt_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void ufs_sprd_debug_dump(u32 dump_req, struct seq_file *m, char **dump_pos)
{
	u64 time;
	ktime_t ktime;
	struct ufs_hba *hba = hba_tmp;

	if (!!hba_tmp) {
		/* dump ufs base info */
		ufs_sprd_base_info_dump(m, dump_pos);
		/* dump ufs err cnt info */
		ufs_uic_err_cnt_dump(hba, m, dump_pos);
	}

	/* dump ufs cmd history */
	ufs_sprd_cmd_history_dump(dump_req, m, dump_pos);

	/* dump current timestamp */
	time = local_clock();
	ktime = ktime_get();
	PRINT_SWITCH(m, dump_pos, "local_clock:%lld.%09lld, ktime:%lld.%09lld\n",
		     time / NSEC_PER_SEC, time % NSEC_PER_SEC,
		     ktime / NSEC_PER_SEC, ktime % NSEC_PER_SEC);

	/* dump ufs dump buffer usage */
	if (dump_pos && *dump_pos)
		PRINT_SWITCH(m, dump_pos, "Dump buffer used:0x%x/(0x%x)\n",
				(u32)(*dump_pos - ufs_dump_buffer), DUMP_BUFFER_S);
}

static int sprd_ufs_panic_handler(struct notifier_block *self,
			       unsigned long val, void *reason)
{
	char *dump_ptr = ufs_dump_buffer;

	if (ufs_dump_buffer)
		ufs_sprd_debug_dump(UFS_CMD_RECORD_DEPTH, NULL, &dump_ptr);

	return NOTIFY_DONE;
}

static struct notifier_block sprd_ufs_event_nb = {
	.notifier_call	= sprd_ufs_panic_handler,
	.priority	= INT_MAX,
};

static enum blk_eh_timer_return ufs_sprd_eh_timed_out(struct scsi_cmnd *scmd)
{
	int tag = scsi_cmd_to_rq(scmd)->tag;
	struct Scsi_Host *host = scmd->device->host;
	struct ufs_hba *hba = shost_priv(host);

	ufs_sprd_update_err_cnt(hba, 0, UFS_SCSI_TIMEOUT);

	if (sprd_ufs_debug_is_supported(hba) == TRUE)
		ufshcd_common_trace(hba, UFS_TRECE_SCSI_TIME_OUT, &tag);

	sprd_ufs_debug_err_dump(hba);

	return BLK_EH_DONE;
}

static int ufs_sprd_debug_procfs_register(struct ufs_hba *hba)
{
	struct proc_dir_entry *ufs_dir;
	struct proc_dir_entry *prEntry;
	struct ufs_sprd_host *host = hba->priv;

	/* UFS debug procfs register */
	ufs_dir = proc_mkdir("ufs", NULL);
	if (!ufs_dir) {
		dev_err(hba->dev, "%s: failed to create /proc/ufs\n", __func__);
		return -ENOMEM;
	}

	prEntry = proc_create_data("cmd_history", 0440, ufs_dir, &ufs_debug_fops, host);
	if (!prEntry)
		dev_info(hba->dev, "%s: failed to create /proc/ufs/debug_info\n", __func__);

	prEntry = proc_create_data("debug_on", UFS_DBG_ACS_LVL, ufs_dir,
			      &ufs_debug_on_fops, host);
	if (!prEntry)
		dev_info(hba->dev, "%s: failed to create /proc/ufs/debug_on\n", __func__);

	prEntry = proc_create_data("err_panic", UFS_DBG_ACS_LVL, ufs_dir,
			      &ufs_err_panic_fops, host);
	if (!prEntry)
		dev_info(hba->dev, "%s: failed to create /proc/ufs/err_panic\n", __func__);

	prEntry = proc_create_data("uic_ec", UFS_DBG_ACS_LVL, ufs_dir,
				&uic_err_cnt_fops, hba->dev);
	if (!prEntry)
		dev_info(hba->dev, "%s: failed to create /proc/ufs/uic_ec\n", __func__);

	return 0;
}

static int ufs_sprd_minidump_register(struct ufs_hba *hba)
{
	ufs_dump_buffer = devm_kzalloc(hba->dev, DUMP_BUFFER_S, GFP_KERNEL);
	if (!ufs_dump_buffer)
		return -ENOMEM;

	/* UFS minidump register */
	if (minidump_save_extend_information("ufs_debug",
					     __pa(ufs_dump_buffer),
					     __pa(ufs_dump_buffer + DUMP_BUFFER_S)))
		dev_info(hba->dev, "%s: failed to link ufs_debug to minidump\n",
			__func__);

	return 0;
}

int ufs_sprd_debug_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_sprd_host *host;

	if (!hba || !hba->priv) {
		pr_info("%s: NULL host exiting\n", __func__);
		return -EINVAL;
	}
	host = hba->priv;
	hba_tmp = hba;

	/* UFS eh_timed_out callback register */
	hba->host->hostt->eh_timed_out = ufs_sprd_eh_timed_out;

	host->err_panic = UFS_DEBUG_ERR_PANIC_DEF;
	host->debug_en = UFS_DEBUG_ON_DEF;

	spin_lock_init(&ufs_debug_dump);

	ret = ufs_sprd_debug_procfs_register(hba);
	if (ret)
		goto out;

	ret = ufs_sprd_minidump_register(hba);
	if (ret)
		goto out;

	/* UFS panic callback register */
	atomic_notifier_chain_register(&panic_notifier_list,
				       &sprd_ufs_event_nb);

out:
	return ret;
}
EXPORT_SYMBOL_GPL(ufs_sprd_debug_init);

#define UFS_DME_GET(_name, _attr_sel, _peer)					\
static ssize_t _name##_show(struct device *dev,					\
	   struct device_attribute *attr, char *buf)				\
{										\
	struct ufs_hba *hba = dev_get_drvdata(dev);				\
	int ret;								\
	u32 mib_val;								\
										\
	ret = ufshcd_dme_get_attr(hba, UIC_ARG_MIB(_attr_sel), &mib_val, _peer);\
	if (ret)								\
		return -EINVAL;							\
										\
	return sprintf(buf, "0x%08x\n", mib_val);				\
}										\
static DEVICE_ATTR_RO(_name)

UFS_DME_GET(host_gear_tx, PA_TXGEAR, 0);
UFS_DME_GET(host_gear_rx, PA_RXGEAR, 0);
UFS_DME_GET(host_lanes_tx, PA_ACTIVETXDATALANES, 0);
UFS_DME_GET(host_lanes_rx, PA_ACTIVERXDATALANES, 0);
UFS_DME_GET(peer_gear_tx, PA_TXGEAR, 1);
UFS_DME_GET(peer_gear_rx, PA_RXGEAR, 1);
UFS_DME_GET(peer_lanes_tx, PA_ACTIVETXDATALANES, 1);
UFS_DME_GET(peer_lanes_rx, PA_ACTIVERXDATALANES, 1);

static struct attribute *ufs_sysfs_power_mode[] = {
	&dev_attr_host_gear_tx.attr,
	&dev_attr_host_gear_rx.attr,
	&dev_attr_host_lanes_tx.attr,
	&dev_attr_host_lanes_rx.attr,
	&dev_attr_peer_gear_tx.attr,
	&dev_attr_peer_gear_rx.attr,
	&dev_attr_peer_lanes_tx.attr,
	&dev_attr_peer_lanes_rx.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_power_mode_group = {
	.name = "pwr_modes",
	.attrs = ufs_sysfs_power_mode,
};

static const struct attribute_group *ufs_sysfs_group[] = {
	&ufs_sysfs_power_mode_group,
	NULL,
};

void ufs_sprd_sysfs_add_nodes(struct ufs_hba *hba)
{
	int ret;

	ret = sysfs_create_groups(&hba->dev->kobj, ufs_sysfs_group);
	if (ret)
		dev_err(hba->dev, "%s: sprd sysfs groups creation failed(err = %d)\n",
				__func__, ret);
}
EXPORT_SYMBOL_GPL(ufs_sprd_sysfs_add_nodes);

int ufs_compare_dev_req_pwr_mode(struct ufs_hba *hba, struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_pa_layer_attr  max_pwr_info_raw = {0};
	struct ufs_pa_layer_attr *max_pwr_info = &max_pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = dev_req_params;


	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&max_pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&max_pwr_info->lane_tx);

	if (pwr_info->pwr_tx == FAST_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_rx);
	else if (pwr_info->pwr_tx == SLOW_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_rx);

	if (pwr_info->pwr_rx == FAST_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_tx);
	else if (pwr_info->pwr_rx == SLOW_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_tx);

	memcpy(&(dts_pwr_info), pwr_info, sizeof(struct ufs_pa_layer_attr));
	memcpy(&(compare_pwr_info), max_pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (max_pwr_info->gear_rx < pwr_info->gear_rx  ||
			max_pwr_info->gear_tx < pwr_info->gear_tx  ||
			max_pwr_info->lane_rx < pwr_info->lane_rx  ||
			max_pwr_info->lane_tx < pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: the dev_req_pwr can not compare\n", __func__);
		dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				pwr_info->gear_rx, pwr_info->gear_tx,
				pwr_info->lane_rx, pwr_info->lane_tx, pwr_info->pwr_rx,
				pwr_info->pwr_tx, pwr_info->hs_rate);
		dev_err(hba->dev, "compare_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				max_pwr_info->gear_rx, max_pwr_info->gear_tx,
				max_pwr_info->lane_rx, max_pwr_info->lane_tx,
				max_pwr_info->pwr_rx, max_pwr_info->pwr_tx,
				max_pwr_info->hs_rate);
		return -EINVAL;
	}

	return 0;
}

int ufs_compare_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = &pwr_info_raw;
	struct ufs_pa_layer_attr *max_pwr_info = &hba->max_pwr_info.info;

	if (!hba->max_pwr_info.is_valid && (max_pwr_info->pwr_tx != 1))
		return -EINVAL;

	pwr_info->pwr_tx = FAST_MODE;
	pwr_info->pwr_rx = FAST_MODE;
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
					__func__, pwr_info->gear_rx);
			return -EINVAL;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
					__func__, pwr_info->gear_tx);
			return -EINVAL;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	}
	memcpy(&(dts_pwr_info), max_pwr_info, sizeof(struct ufs_pa_layer_attr));
	memcpy(&(compare_pwr_info), pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (pwr_info->gear_rx != max_pwr_info->gear_rx  ||
			pwr_info->gear_tx != max_pwr_info->gear_tx  ||
			pwr_info->lane_rx != max_pwr_info->lane_rx   ||
			pwr_info->lane_tx != max_pwr_info->lane_tx   ||
			pwr_info->pwr_rx != max_pwr_info->pwr_rx     ||
			pwr_info->pwr_tx != max_pwr_info->pwr_tx     ||
			pwr_info->hs_rate != max_pwr_info->hs_rate) {
		dev_err(hba->dev, "%s: the max can not compare\n", __func__);
		dev_err(hba->dev, "compare_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				pwr_info->gear_rx, pwr_info->gear_tx,
				pwr_info->lane_rx, pwr_info->lane_tx, pwr_info->pwr_rx,
				pwr_info->pwr_tx, pwr_info->hs_rate);
		dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				max_pwr_info->gear_rx, max_pwr_info->gear_tx,
				max_pwr_info->lane_rx, max_pwr_info->lane_tx, max_pwr_info->pwr_rx,
				max_pwr_info->pwr_tx, max_pwr_info->hs_rate);
		return -EINVAL;
	}
	return 0;
}

int ufs_sprd_pwr_post_compare(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw = {0};
	struct ufs_pa_layer_attr *pwr_mode = &pwr_info_raw;
	int ret = 0, pwr = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_RXGEAR),
			&pwr_mode->gear_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TXGEAR),
			&pwr_mode->gear_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			&pwr_mode->lane_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			&pwr_mode->lane_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HSSERIES),
			&pwr_mode->hs_rate);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			&pwr);
	if (ret)
		goto out;
	pwr_mode->pwr_rx = (pwr >> 4) & 0xf;
	pwr_mode->pwr_tx = (pwr >> 0) & 0xf;
	if (pwr_mode->gear_rx == dts_pwr_info.gear_rx &&
			pwr_mode->gear_tx == dts_pwr_info.gear_tx &&
			pwr_mode->lane_rx == dts_pwr_info.lane_rx &&
			pwr_mode->lane_tx == dts_pwr_info.lane_tx &&
			pwr_mode->pwr_rx == dts_pwr_info.pwr_rx &&
			pwr_mode->pwr_tx == dts_pwr_info.pwr_tx &&
			pwr_mode->hs_rate == dts_pwr_info.hs_rate){
		return 0;
	}
out:
	dev_err(hba->dev, "sprd_pwr_post can not compare\n");
	dev_err(hba->dev, "compare %d,%d,%d,%d,%d,%d,%d\n",
			pwr_mode->gear_rx, pwr_mode->gear_tx,
			pwr_mode->lane_rx, pwr_mode->lane_tx, pwr_mode->pwr_rx,
			pwr_mode->pwr_tx, pwr_mode->hs_rate);
	dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
			dts_pwr_info.gear_rx, dts_pwr_info.gear_tx,
			dts_pwr_info.lane_rx, dts_pwr_info.lane_tx,
			dts_pwr_info.pwr_rx,
			dts_pwr_info.pwr_tx, dts_pwr_info.hs_rate);
	return -1;
}

int  ufs_sprd_pwr_change_compare(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *dev_max_params,
		struct ufs_pa_layer_attr *dev_req_params,
		int err)
{
	struct ufs_sprd_host *host = hba->priv;

	switch (status) {
	case PRE_CHANGE:
		host->times_pre_pwr++;
		if (err == 0) {
			if (ufs_compare_dev_req_pwr_mode(hba, dev_req_params)) {
				host->times_pre_compare_fail++;
				dev_err(hba->dev, "%s: err: compare_pwr\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
				panic("dev_req_pwr_mode pre_compare_fail");
#endif
			}
		} else {
			if (ufs_compare_max_pwr_mode(hba)) {
				host->times_pre_compare_fail++;
				dev_err(hba->dev, "%s: err:  compare_max_pwr\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
				panic("max_pwr_mode pre_compare_fail");
#endif
			}
		}
		break;
	case POST_CHANGE:
		host->times_post_pwr++;
		/* if already configured to the requested pwr_mode */
		if (ufs_sprd_pwr_post_compare(hba)) {
			host->times_post_compare_fail++;
			dev_err(hba->dev, "%s: power configured error\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
			panic("post_compare_fail");
#endif
		}
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}
EXPORT_SYMBOL_GPL(ufs_sprd_pwr_change_compare);

