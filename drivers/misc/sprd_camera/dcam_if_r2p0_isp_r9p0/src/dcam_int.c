/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <video/sprd_mm.h>

#include "dcam_drv.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "dcam_int: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

typedef void (*dcam_isr) (enum dcam_id idx, enum dcam_irq_id irq_id,
			void *param);

static dcam_isr_func s_user_func[DCAM_MAX_COUNT][DCAM_IRQ_NUMBER];
static void *s_user_data[DCAM_MAX_COUNT][DCAM_IRQ_NUMBER];

static struct{
	uint32_t irq;
	char *irq_name;
	int enable_log;
} s_irq_vect[] = {
	{DCAM_SN_SOF,            "sensor_sof",       0},
	{DCAM_SN_EOF,            "sensor_eof",       0},
	{DCAM_CAP_SOF,           "cap_sof",          0},
	{DCAM_CAP_EOF,           "cap_eof",          0},
	{DCAM_DCAM_OVF,          "dcam_ovf",         1},
	{DCAM_PREVIEW_SOF,       "preview_sof",      0},
	{DCAM_ISP_ENABLE_PULSE,  "isp_enable_pulse", 0},
	{DCAM_FETCH_SOF_INT,     "fetch_sof int",    0},
	{DCAM_AFL_LAST_SOF,      "afl_last_sof",     0},
	{DCAM_BPC_MEM_ERR,       "bpc_mem_err",      1},
	{DCAM_CAP_LINE_ERR,      "cap_line_err",     1},
	{DCAM_CAP_FRM_ERR,       "cap_frm_err",      1},
	{DCAM_FULL_PATH_END,     "full_path_end",    0},
	{DCAM_BIN_PATH_END,      "bin_path_end",     0},
	{DCAM_AEM_PATH_END,      "aem_path_end",     0},
	{DCAM_PDAF_PATH_END,     "pdaf_path_end",    0},
	{DCAM_VCH2_PATH_END,     "vch2_path_end",    0},
	{DCAM_VCH3_PATH_END,     "vch3_path_end",    0},
	{DCAM_FULL_PATH_TX_DONE, "full_path_done",   0},
	{DCAM_BIN_PATH_TX_DONE,  "bin_path_done",    0},
	{DCAM_AEM_PATH_TX_DONE,  "aem_path_done",    0},
	{DCAM_PDAF_PATH_TX_DONE, "pdaf_path_done",   0},
	{DCAM_VCH2_PATH_TX_DONE, "vch2_path_done",   0},
	{DCAM_VCH3_PATH_TX_DONE, "vch3_path_done",   0},
	{DCAM_BPC_MAP_DONE,      "bpc_map_done",     0},
	{DCAM_BPC_POS_DONE,      "bpc_pos_done",     0},
	{DCAM_AFM_INTREQ0,       "afm_intreq0",      0},
	{DCAM_AFM_INTREQ1,       "afm_intreq1",      0},
	{DCAM_AFL_TX_DONE,       "afl_tx_done",      0},
	{DCAM_NR3_TX_DONE,       "nr3_tx_done",      0},
	{DCAM_RESERVED,          "reserved",         0},
	{DCAM_MMU_INT,           "mmu_int",          0},
};

static int sprd_dcamint_update_time(struct camera_frame *frame,
	struct dcam_module *module)
{
	int ret = DCAM_RTN_SUCCESS;
	int i = 0;

	if (frame == NULL || module == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	if (module->frame_id - frame->frame_id >= DCAM_FRM_QUEUE_LENGTH) {
		pr_debug("cb: %pS\n", __builtin_return_address(0));
		pr_debug("dcam%d frame_id %d overwrte frame %d\n",
			module->id, module->frame_id, frame->frame_id);
	}

	i = frame->frame_id % DCAM_FRM_QUEUE_LENGTH;
	frame->time = module->time[i];
	frame->dual_info = module->dual_info[i];
	return ret;
}

static int sprd_dcamint_get_time(struct camera_frame *frame,
	struct dcam_module *module)
{
	int ret = DCAM_RTN_SUCCESS;
	int i = 0;
	struct dcam_group *dcam_group = NULL;
	struct dcam_module *dcam0 = NULL;
	struct dcam_module *dcam1 = NULL;
	s64 time_diff = 0, half_cycle = 0;
	uint32_t idx = 0;

	if (frame == NULL || module == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	i = frame->frame_id % DCAM_FRM_QUEUE_LENGTH;
	module->time[i].boot_time = ktime_get_boottime();
	sprd_cam_com_timestamp(&module->time[i].timeval);
	frame->time.boot_time = module->time[i].boot_time;
	frame->time.timeval = module->time[i].timeval;
	module->time_index = i;
	idx = module->id;

	dcam_group = sprd_dcam_drv_group_get();
	if (!dcam_group || !dcam_group->dual_cam)
		return ret;

	dcam0 = dcam_group->dcam[DCAM_ID_0];
	dcam1 = dcam_group->dcam[DCAM_ID_1];
	time_diff = (dcam1->time[dcam1->time_index].boot_time.tv64)
		- (dcam0->time[dcam0->time_index].boot_time.tv64);
	time_diff = (time_diff > 0 ? time_diff : -time_diff);
	if (frame->frame_id >= 1)
		half_cycle = frame->time.boot_time.tv64 -
			module->time[(frame->frame_id - 1)
			% DCAM_FRM_QUEUE_LENGTH].boot_time.tv64;
	else
		time_diff = 0x7FFFFFFFFFFFFFFF;
	half_cycle >>= 1;
	frame->dual_info.time_diff = time_diff;
	if (time_diff > half_cycle)
		frame->dual_info.is_last_frm = 0;
	else
		frame->dual_info.is_last_frm = 1;
	module->dual_info[i] = frame->dual_info;

	return ret;
}

static int sprd_dcamint_isr_proc(enum dcam_id idx, enum dcam_irq_id id,
				struct camera_frame *frame)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func user_func;
	void *user_data;
	struct dcam_module *module = sprd_dcam_drv_module_get(idx);

	if (id >= DCAM_IRQ_NUMBER) {
		rtn = DCAM_RTN_ISR_ID_ERR;
	} else {
		sprd_dcamint_update_time(frame, module);

		user_func = s_user_func[idx][id];
		user_data = s_user_data[idx][id];
		if (user_func)
			user_func(frame, user_data);
	}

	return -rtn;
}

static void sprd_dcamint_3dnr_frame_mv_get(enum dcam_id idx,
	int *mv_x, int *mv_y)
{
	int out0 = 0;
	int out1 = 0;
	signed char temp_mv_x = 0;
	signed char temp_mv_y = 0;
	uint32_t param1 = 0;
	uint32_t fast_me_done = 0;
	uint32_t ping_pang_en = 0;

	param1 = DCAM_REG_RD(idx, DCAM_NR3_PARA1);
	ping_pang_en = param1 & BIT_1;

	if (ping_pang_en == 1) {
		out1 = DCAM_REG_RD(idx, DCAM_NR3_OUT1);
		out0 = DCAM_REG_RD(idx, DCAM_NR3_OUT0);

		fast_me_done = out1 & BIT_0;
		if (fast_me_done == 0) {
			temp_mv_x = (out0 >> 8) & 0xFF;
			temp_mv_y = out0 & 0xFF;
			*mv_x = temp_mv_x;
			*mv_y = temp_mv_y;
		} else {
			temp_mv_x = (out0 >> 24) & 0xFF;
			temp_mv_y = (out0 >> 16) & 0xFF;
			*mv_x = temp_mv_x;
			*mv_y = temp_mv_y;
		}
	} else if (ping_pang_en == 0) {
		out0 = DCAM_REG_RD(idx, DCAM_NR3_OUT0);
		temp_mv_x  = (out0 >> 8) & 0xFF;
		temp_mv_y = out0 & 0xFF;
		*mv_x = temp_mv_x;
		*mv_y = temp_mv_y;
	} else {
		pr_err("fail to get 3dnr frame_mv\n");
	}
}

static void sprd_dcamint_3dnr_me_frame_store(
	struct dcam_module *dcam_dev,
	int irq_id, struct camera_frame *frame)
{
	if (dcam_dev == NULL || frame == NULL) {
		pr_err("fail to fast_me_store_frame\n");
		return;
	}

	if (irq_id == DCAM_BIN_PATH_TX_DONE)
		memcpy(&(dcam_dev->fast_me.bin_frame),
			frame, sizeof(struct camera_frame));
	else
		memcpy(&(dcam_dev->fast_me.full_frame),
			frame, sizeof(struct camera_frame));
}

static void sprd_dcamint_3dnr_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	int x = 0;
	int y = 0;
	struct dcam_module *dcam_dev = NULL;

	dcam_dev = (struct dcam_module *)param;

	if (!dcam_dev->need_nr3)
		return;

	dcam_dev->fast_me.mv_ready_cnt++;
	if (dcam_dev->fast_me.mv_ready_cnt > 1)
		dcam_dev->fast_me.mv_ready_cnt = 1;

	sprd_dcamint_3dnr_frame_mv_get(idx, &x, &y);
	dcam_dev->fast_me.mv_x = x;
	dcam_dev->fast_me.mv_y = y;

	if (dcam_dev->fast_me.bin_frame_cnt > 0) {
		sprd_dcamint_isr_proc(idx, DCAM_BIN_PATH_TX_DONE,
			&(dcam_dev->fast_me.bin_frame));
		dcam_dev->fast_me.mv_ready_cnt = 0;
		dcam_dev->fast_me.bin_frame_cnt = 0;
	}

	if (dcam_dev->fast_me.full_frame_cnt > 0) {
		sprd_dcamint_isr_proc(idx, DCAM_FULL_PATH_TX_DONE,
			&(dcam_dev->fast_me.full_frame));
		dcam_dev->fast_me.mv_ready_cnt = 0;
		dcam_dev->fast_me.full_frame_cnt = 0;
	}
}

static void sprd_dcamint_full_path_sof(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	if (full_path->status == DCAM_ST_START) {
		if (full_path->valid == 0) {
			pr_err("fail to get DCAM%d valid full_path\n", idx);
			return;
		}

		rtn = sprd_dcam_full_path_next_frm_set(idx);
		if (rtn) {
			full_path->need_wait = 1;
			pr_err("fail to set DCAM%d: full_path next frm\n", idx);
			return;
		}
	}
}

static void sprd_dcamint_full_path_done(enum dcam_id idx,
		enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	if (DCAM_ADDR_INVALID(full_path))
		return;

	if (full_path->valid == 0) {
		pr_err("fail to get DCAM%d valid full_path\n", idx);
		return;
	}

	dcam_dev = (struct dcam_module *)param;
	if (full_path->need_wait) {
		full_path->need_wait = 0;
	} else {
		struct camera_frame frame;

		rtn = sprd_cam_queue_frm_dequeue(
			&full_path->frame_queue, &frame);
		if (rtn)
			return;
		if (frame.buf_info.dev == NULL)
			pr_err("fail to done DCAM%d dev NULL %p\n",
				idx, frame.buf_info.dev);
		if (!sprd_cam_buf_is_equal(&frame.buf_info,
			&full_path->reserved_frame.buf_info)) {

			frame.width = full_path->output_size.w;
			frame.height = full_path->output_size.h;
			frame.irq_type = CAMERA_IRQ_IMG;
			frame.irq_property = DCAM_FULL_PATH_TX_DONE;

			DCAM_TRACE("y uv, 0x%x 0x%x, mfd 0x%x,0x%x,iova 0x%x\n",
				frame.yaddr, frame.uaddr,
				frame.buf_info.mfd[0],
				frame.buf_info.mfd[1],
				(uint32_t)frame.buf_info.iova[0] + frame.yaddr);

			if (dcam_dev->need_nr3) {
				uint32_t mv_ready_cnt =
					dcam_dev->fast_me.mv_ready_cnt;
				if (mv_ready_cnt == 0) {
					dcam_dev->fast_me.full_frame_cnt++;
					sprd_dcamint_3dnr_me_frame_store(
						dcam_dev,
						DCAM_FULL_PATH_TX_DONE, &frame);
				} else if (mv_ready_cnt == 1) {
					frame.mv.mv_x = dcam_dev->fast_me.mv_x;
					frame.mv.mv_y = dcam_dev->fast_me.mv_x;
					sprd_dcamint_isr_proc(idx, irq_id,
						&frame);
					dcam_dev->fast_me.full_frame_cnt = 0;
					dcam_dev->fast_me.mv_ready_cnt = 0;
				} else {
					dcam_dev->fast_me.mv_ready_cnt = 0;
					dcam_dev->fast_me.full_frame_cnt = 0;
					pr_err("DCAM%d:fail to full mv_cnt err\n",
						idx);
					return;
				}
			} else {
				sprd_dcamint_isr_proc(idx, irq_id, &frame);
			}
		} else {
			DCAM_TRACE("DCAM%d: use reserved full_path\n", idx);
		}
	}
}

static void sprd_dcamint_bin_path_sof(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *bin_path = sprd_dcam_drv_bin_path_get(idx);

	if (bin_path->status == DCAM_ST_START) {
		if (bin_path->valid == 0) {
			pr_err("fail to get DCAM%d valid bin_path\n", idx);
			return;
		}

		if (bin_path->need_downsizer) {
			rtn =  sprd_dcam_bin_path_scaler_cfg(idx);
			if (rtn) {
				pr_err("fail to cfg dcam%d bin scaler\n", idx);
				return;
			}
		}
		rtn = sprd_dcam_bin_path_next_frm_set(idx);
		if (rtn) {
			bin_path->need_wait = 1;
			pr_err("fail to set DCAM%d: bin_path next frm\n", idx);
			return;
		}
	}
}

static void sprd_dcamint_bin_path_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct dcam_path_desc *bin_path = sprd_dcam_drv_bin_path_get(idx);

	if (DCAM_ADDR_INVALID(bin_path))
		return;

	if (bin_path->valid == 0) {
		pr_err("fail to get DCAM%d valid bin_path\n", idx);
		return;
	}

	dcam_dev = (struct dcam_module *)param;
	if (idx == DCAM_ID_1 && dcam_dev->need_4in1)
		dcam_dev->cap_4in1 = 1;

	if (bin_path->need_wait) {
		bin_path->need_wait = 0;
	} else {
		struct camera_frame frame;

		rtn = sprd_cam_queue_frm_dequeue(
			&bin_path->frame_queue, &frame);
		if (rtn)
			return;
		if (frame.buf_info.dev == NULL)
			pr_err("fail to done DCAM%d dev NULL %p\n",
				idx, frame.buf_info.dev);
		if (!sprd_cam_buf_is_equal(&frame.buf_info,
			&bin_path->reserved_frame.buf_info)) {
			if (!bin_path->need_downsizer) {
				frame.width = bin_path->output_size.w;
				frame.height = bin_path->output_size.h;
			}
			frame.irq_type = CAMERA_IRQ_IMG;
			frame.irq_property = DCAM_BIN_PATH_TX_DONE;
			DCAM_TRACE("y uv, 0x%x 0x%x, mfd 0x%x,0x%x,iova 0x%x\n",
				frame.yaddr, frame.uaddr,
				frame.buf_info.mfd[0],
				frame.buf_info.mfd[1],
				(uint32_t)frame.buf_info.iova[0] + frame.yaddr);

			if (dcam_dev->need_nr3) {
				uint32_t mv_ready_cnt =
					dcam_dev->fast_me.mv_ready_cnt;

				if (mv_ready_cnt == 0) {
					dcam_dev->fast_me.bin_frame_cnt++;
					sprd_dcamint_3dnr_me_frame_store(
						dcam_dev,
						DCAM_BIN_PATH_TX_DONE, &frame);
				} else if (mv_ready_cnt == 1) {
					frame.mv.mv_x = dcam_dev->fast_me.mv_x;
					frame.mv.mv_y = dcam_dev->fast_me.mv_x;
					sprd_dcamint_isr_proc(idx, irq_id,
						&frame);
					dcam_dev->fast_me.bin_frame_cnt = 0;
					dcam_dev->fast_me.mv_ready_cnt = 0;
				} else {
					dcam_dev->fast_me.mv_ready_cnt = 0;
					dcam_dev->fast_me.bin_frame_cnt = 0;
					pr_err("DCAM%d:fail to 3DNR bin mv_cnt err\n",
						idx);
					return;
				}
			} else {
				sprd_dcamint_isr_proc(idx, irq_id, &frame);
			}
		} else {
			DCAM_TRACE("DCAM%d: use reserved bin_path\n", idx);
		}
	}
}

static void sprd_dcamint_aem_start(enum dcam_id idx, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;

	rtn = sprd_cam_statistic_next_buf_set(module, ISP_AEM_BLOCK,
		dcam_dev->frame_id);
	if (rtn)
		pr_err("fail to set AEM next statis buf\n");
}

static void sprd_dcamint_aem_done(enum dcam_id idx, enum dcam_irq_id irq_id,
		void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->aem_statis_frm_queue;

	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP%d:fail to dequeue AEM buf error\n", idx);
		return;
	}

	if (node.mfd != module->aem_buf_reserved.mfd ||
		node.phy_addr != module->aem_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AEM_STATIS;
		frame_info.frame_id = node.frame_id;

		/*call_back func to write the buf addr to usr_buf_queue*/
		sprd_dcamint_isr_proc(idx, DCAM_AEM_PATH_TX_DONE, &frame_info);
	}
}

static void sprd_dcamint_afl_start(enum dcam_id idx, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;

	rtn = sprd_cam_statistic_next_buf_set(module, ISP_AFL_BLOCK,
		dcam_dev->frame_id);
	if (rtn)
		pr_err("fail to set AFL next statis buf\n");
}

static void sprd_dcamint_afl_done(enum dcam_id idx, enum dcam_irq_id irq_id,
		void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->afl_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP%d: fail to dequeue AFL buf error\n", idx);
		return;
	}

	if (node.mfd != module->afl_buf_reserved.mfd ||
		node.phy_addr != module->afl_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AFL_STATIS;
		frame_info.frame_id = node.frame_id;

		/*call_back func to write the buf addr to usr_buf_queue*/
		sprd_dcamint_isr_proc(idx, DCAM_AFL_TX_DONE, &frame_info);
	}
	sprd_dcamint_afl_start(idx, param);
}

static void sprd_dcamint_afm_start(enum dcam_id idx, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;

	rtn = sprd_cam_statistic_next_buf_set(module, ISP_AFM_BLOCK,
		dcam_dev->frame_id);
	if (rtn)
		pr_err("fail to set AFM next statis buf\n");
}

static void sprd_dcamint_afm_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->afm_statis_frm_queue;

	memset(&node, 0x00, sizeof(node));
	memset(&frame_info, 0x00, sizeof(frame_info));
	/*dequeue the statis buf from a array*/
	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP%d:fail to dequeue AFM statis buf\n", idx);
		return;
	}

	if (node.mfd != module->afm_buf_reserved.mfd ||
		node.phy_addr != module->afm_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_AFM_STATIS;
		frame_info.frame_id = node.frame_id;

		/*call_back func to write the buf addr to usr_buf_queue*/
		sprd_dcamint_isr_proc(idx, DCAM_AFM_INTREQ1, &frame_info);
	}
}

static void sprd_dcamint_vch2_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->pdaf_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP%d:fail to dequeue PDAF buf error\n", idx);
		return;
	}

	if (node.mfd != module->pdaf_buf_reserved.mfd
		 || node.phy_addr != module->pdaf_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_PDAF_STATIS;
		frame_info.frame_id = node.frame_id;

		/*call_back func to write the buf addr to usr_buf_queue*/
		sprd_dcamint_isr_proc(idx, DCAM_PDAF_PATH_TX_DONE,
			&frame_info);
	}
}

static void sprd_dcamint_pdaf_start(enum dcam_id idx, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;

	rtn = sprd_cam_statistic_next_buf_set(module, ISP_PDAF_BLOCK,
		dcam_dev->frame_id);
	if (rtn)
		pr_err("fail to set pdaf next statis buf\n");
}

static void sprd_dcamint_pdaf_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->pdaf_statis_frm_queue;

	/*dequeue the statis buf from a array*/
	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("ISP%d:fail to dequeue PDAF buf error\n", idx);
		return;
	}

	if (node.mfd != module->pdaf_buf_reserved.mfd
		 || node.phy_addr != module->pdaf_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_PDAF_STATIS;
		frame_info.frame_id = node.frame_id;

		/*call_back func to write the buf addr to usr_buf_queue*/
		sprd_dcamint_isr_proc(idx, DCAM_PDAF_PATH_TX_DONE,
		&frame_info);
	}
}

static void sprd_dcamint_ebd_start(enum dcam_id idx, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_statis_module *module = NULL;

	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;

	rtn = sprd_cam_statistic_next_buf_set(module, ISP_EBD_BLOCK,
		dcam_dev->frame_id);
	if (rtn)
		pr_err("fail to set ebd next statis buf\n");
}

static void sprd_dcamint_ebd_done(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_module *dcam_dev = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *module = NULL;

	memset(&frame_info, 0x00, sizeof(frame_info));
	dcam_dev = (struct dcam_module *)param;
	module = &dcam_dev->statis_module_info;
	statis_heap = &module->ebd_statis_frm_queue;

	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn) {
		pr_err("fail to dequeue embedded line buf error\n");
		return;
	}

	if (node.mfd != module->ebd_buf_reserved.mfd
		 || node.phy_addr != module->ebd_buf_reserved.phy_addr) {
		frame_info.buf_size = node.buf_size;
		memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
				sizeof(node.buf_info.mfd));
		frame_info.phy_addr = node.phy_addr;
		frame_info.vir_addr = node.vir_addr;
		frame_info.addr_offset = node.addr_offset;
		frame_info.irq_type = CAMERA_IRQ_STATIS;
		frame_info.irq_property = IRQ_EBD_STATIS;
		frame_info.frame_id = node.frame_id;

		sprd_dcamint_isr_proc(idx, DCAM_VCH3_PATH_TX_DONE,
			&frame_info);
	}
}

static void sprd_dcamint_module_start(enum dcam_id idx,
	void *param)
{
	struct dcam_module *module = (struct dcam_module *)param;

	if (DCAM_ADDR_INVALID(module))
		return;

	if (!module->is_high_fps) {
		sprd_dcamint_aem_start(idx, param);
		sprd_dcamint_afm_start(idx, param);
	} else {
		module->high_fps_cnt++;
		if (module->high_fps_skip_num ==
			module->high_fps_cnt) {
			module->high_fps_cnt = 0;
			sprd_dcamint_aem_start(idx, param);
			sprd_dcamint_afm_start(idx, param);
		}
	}
	sprd_dcamint_pdaf_start(idx, param);
	sprd_dcamint_ebd_start(idx, param);
}

static void sprd_dcamint_cap_sof(enum dcam_id idx,
	enum dcam_irq_id irq_id, void *param)
{
	dcam_isr_func user_func;
	void *data;
	struct dcam_module *module = (struct dcam_module *)param;
	struct camera_frame frame;
	struct dcam_cap_desc *cap_desc = sprd_dcam_drv_cap_get(idx);

	if (DCAM_ADDR_INVALID(module))
		return;

	user_func = s_user_func[idx][irq_id];
	data = s_user_data[idx][irq_id];

	memset(&frame, 0x0, sizeof(frame));
	frame.frame_id = module->frame_id++;
	sprd_dcamint_get_time(&frame, module);

	sprd_dcamint_full_path_sof(idx);

	if (cap_desc->input_format == DCAM_CAP_MODE_RAWRGB) {
		sprd_dcamint_bin_path_sof(idx);
		sprd_dcamint_module_start(idx, param);
		sprd_dcam_drv_auto_copy(idx, ALL_COPY);
		if (user_func)
			(*user_func) (&frame, data);
	} else
		sprd_dcam_drv_auto_copy(idx, ALL_COPY);
}

static void sprd_dcamint_default_irq(enum dcam_id idx, enum dcam_irq_id irq_id,
		void *param)
{
	dcam_isr_func user_func = s_user_func[idx][irq_id];
	void *data = s_user_data[idx][irq_id];
	struct dcam_module *module = (struct dcam_module *)param;

	if (DCAM_ADDR_INVALID(module)) {
		pr_err("fail to get valid input ptr dcam%d int %s\n",
			idx, s_irq_vect[irq_id].irq_name);
		return;
	}
	if (s_irq_vect[irq_id].enable_log)
		pr_info("DCAM%d: int %s\n", idx, s_irq_vect[irq_id].irq_name);

	if (user_func)
		(*user_func) (NULL, data);
}

static const dcam_isr dcam_isr_list[DCAM_MAX_COUNT][DCAM_IRQ_NUMBER] = {
	[0][DCAM_SN_SOF] = sprd_dcamint_default_irq,
	[0][DCAM_SN_EOF] = sprd_dcamint_default_irq,
	[0][DCAM_CAP_SOF] = sprd_dcamint_cap_sof,
	[0][DCAM_CAP_EOF] = sprd_dcamint_default_irq,
	[0][DCAM_DCAM_OVF] = sprd_dcamint_default_irq,
	[0][DCAM_BPC_MEM_ERR] = sprd_dcamint_default_irq,
	[0][DCAM_CAP_LINE_ERR] = sprd_dcamint_default_irq,
	[0][DCAM_CAP_FRM_ERR] = sprd_dcamint_default_irq,
	[0][DCAM_FULL_PATH_TX_DONE] = sprd_dcamint_full_path_done,
	[0][DCAM_BIN_PATH_TX_DONE] = sprd_dcamint_bin_path_done,
	[0][DCAM_AEM_PATH_TX_DONE] = sprd_dcamint_aem_done,
	[0][DCAM_PDAF_PATH_TX_DONE] = sprd_dcamint_pdaf_done,
	[0][DCAM_VCH2_PATH_TX_DONE] = sprd_dcamint_vch2_done,
	[0][DCAM_VCH3_PATH_TX_DONE] = sprd_dcamint_ebd_done,
	[0][DCAM_AFM_INTREQ1] = sprd_dcamint_afm_done,
	[0][DCAM_AFL_TX_DONE] = sprd_dcamint_afl_done,
	[0][DCAM_NR3_TX_DONE] = sprd_dcamint_3dnr_done,

	[1][DCAM_SN_SOF] = sprd_dcamint_default_irq,
	[1][DCAM_SN_EOF] = sprd_dcamint_default_irq,
	[1][DCAM_CAP_SOF] = sprd_dcamint_cap_sof,
	[1][DCAM_CAP_EOF] = sprd_dcamint_default_irq,
	[1][DCAM_DCAM_OVF] = sprd_dcamint_default_irq,
	[1][DCAM_BPC_MEM_ERR] = sprd_dcamint_default_irq,
	[1][DCAM_CAP_LINE_ERR] = sprd_dcamint_default_irq,
	[1][DCAM_CAP_FRM_ERR] = sprd_dcamint_default_irq,
	[1][DCAM_FULL_PATH_TX_DONE] = sprd_dcamint_full_path_done,
	[1][DCAM_BIN_PATH_TX_DONE] = sprd_dcamint_bin_path_done,
	[1][DCAM_AEM_PATH_TX_DONE] = sprd_dcamint_aem_done,
	[1][DCAM_AFM_INTREQ1] = sprd_dcamint_afm_done,
	[1][DCAM_AFL_TX_DONE] = sprd_dcamint_afl_done,
	[1][DCAM_NR3_TX_DONE] = sprd_dcamint_3dnr_done,

	[2][DCAM_SN_SOF] = sprd_dcamint_cap_sof,
	[2][DCAM_SN_EOF] = sprd_dcamint_default_irq,
	[2][DCAM_DCAM_OVF] = sprd_dcamint_default_irq,
	[2][DCAM_CAP_LINE_ERR] = sprd_dcamint_default_irq,
	[2][DCAM_CAP_FRM_ERR] = sprd_dcamint_default_irq,
	[2][DCAM_FULL_PATH_TX_DONE] = sprd_dcamint_full_path_done,
};

static int sprd_dcamint_err_pre_proc(enum dcam_id idx,
	uint32_t irq_status)
{
	if (sprd_dcam_drv_module_get(idx)) {
		DCAM_TRACE("DCAM%d: state in err_pre_proc 0x%x\n", idx,
			sprd_dcam_drv_module_get(idx)->state);
		if (sprd_dcam_drv_module_get(idx)->state & DCAM_STATE_QUICKQUIT)
			return -1;
	}
	pr_info("DCAM%d: irq_status, 0x%x\n", idx, irq_status);

	sprd_isp_drv_reg_trace((int)idx);
	sprd_dcam_drv_reg_trace(idx);

	sprd_dcam_drv_glb_reg_mwr(idx, DCAM_CFG, BIT_0, 0, DCAM_CFG_REG);
	sprd_dcam_drv_stop(idx, 1);

	return 0;
}

static void sprd_dcamint_iommu_reg_trace(enum dcam_id idx)
{
	unsigned long addr = 0;

	pr_info("DCAM%d:iommu Register list\n", idx);
	for (addr = DCAM_MMU_EN; addr <= DCAM_MMU_EN_SHAD; addr += 16) {
		pr_info("addr= 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			DCAM_MMU_RD(addr),
			DCAM_MMU_RD(addr + 4),
			DCAM_MMU_RD(addr + 8),
			DCAM_MMU_RD(addr + 12));
	}
}

static int sprd_dcamint_iommu_err_pre_proc(
	enum dcam_id idx, unsigned int irq_status)
{
	if (sprd_dcam_drv_module_get(idx)) {
		if (sprd_dcam_drv_module_get(idx)->state & DCAM_STATE_QUICKQUIT)
			return -1;
	}
	pr_err("fail to run iommu, DCAM%d mmu err 0x%x\n", idx, irq_status);

	sprd_dcamint_iommu_reg_trace(idx);

	/*panic("fatal dcam iommu error!");*/

	return 0;
}

static irqreturn_t sprd_dcamint_isr_root(int irq, void *priv)
{
	int i = 0;
	uint32_t irq_line = 0, status = 0, vect = 0;
	unsigned long flag = 0;
	enum dcam_id idx = DCAM_ID_0;
	int irq_numbers = ARRAY_SIZE(s_irq_vect);

	if (s_dcam_irq[DCAM_ID_0] == irq)
		idx = DCAM_ID_0;
	else if (s_dcam_irq[DCAM_ID_1] == irq)
		idx = DCAM_ID_1;
	else if (s_dcam_irq[DCAM_ID_2] == irq)
		idx = DCAM_ID_2;
	else
		return IRQ_NONE;

	status = DCAM_REG_RD(idx, DCAM_INT_MASK) & DCAM_IRQ_LINE_MASK;
	if (unlikely(status == 0))
		return IRQ_NONE;
	DCAM_REG_WR(idx, DCAM_INT_CLR, status);

	if (unlikely(DCAM_IRQ_ERR_MMU & status))
		sprd_dcamint_iommu_err_pre_proc(idx, status);

	irq_line = status;
	if (unlikely(DCAM_IRQ_ERR_MASK & status))
		if (sprd_dcamint_err_pre_proc(idx, status))
			return IRQ_HANDLED;

	spin_lock_irqsave(&dcam_lock[idx], flag);

	for (i = 0; i < irq_numbers; i++) {
		vect = s_irq_vect[i].irq;
		if (irq_line & (1 << (uint32_t)vect)) {
			if (dcam_isr_list[idx][vect])
				dcam_isr_list[idx][vect](idx, i, priv);
		}
		irq_line &= ~(uint32_t)(1 << (uint32_t)vect);
		if (!irq_line)
			break;
	}

	spin_unlock_irqrestore(&dcam_lock[idx], flag);

	return IRQ_HANDLED;
}

int sprd_dcam_int_irq_request(enum dcam_id idx,
	void *param)
{
	int ret = 0;

	ret = request_irq(s_dcam_irq[idx], sprd_dcamint_isr_root,
			IRQF_SHARED, "DCAM", param);
	if (ret) {
		pr_err("fail to install IRQ %d\n", ret);
		return ret;
	}

	return ret;
}

void sprd_dcam_int_irq_free(enum dcam_id idx,
	void *param)
{
	free_irq(s_dcam_irq[idx], param);
}

int sprd_dcam_int_reg_isr(enum dcam_id idx, enum dcam_irq_id id,
		dcam_isr_func user_func, void *user_data)
{
	unsigned long flag = 0;
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;

	if (id >= DCAM_IRQ_NUMBER) {
		rtn = DCAM_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&dcam_lock[idx], flag);
		s_user_func[idx][id] = user_func;
		s_user_data[idx][id] = user_data;
		spin_unlock_irqrestore(&dcam_lock[idx], flag);
	}

	return -rtn;
}
