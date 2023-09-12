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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>

#include <sprd_mm.h>
#include <isp_hw.h>
#include "sprd_img.h"
#include "cam_trusty.h"
#include "cam_test.h"

#include "cam_debugger.h"
#include "isp_interface.h"
#include "flash_interface.h"

#include "sprd_sensor_drv.h"
#include "dcam_reg.h"
#include "csi_api.h"
#include "dcam_core.h"
#include "isp_core.h"
#include "isp_drv.h"
#include "cam_scaler_ex.h"
#include "dcam_int.h"
#include "dcam_offline.h"
#include "cam_dump.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define IMG_DEVICE_NAME                 "sprd_image"
#define CAMERA_TIMEOUT                  5000
#define CAMERA_RECOVERY_TIMEOUT         100
#define CAMERA_LONGEXP_TIMEOUT          50000

#define THREAD_STOP_TIMEOUT             3000

#define CAM_COUNT                       CAM_ID_MAX
/* TODO: extend this for slow motion dev */
#define CAM_SHARED_BUF_NUM              50
#define CAM_FRAME_Q_LEN                 48
#define CAM_IRQ_Q_LEN                   16
#define CAM_STATIS_Q_LEN                16
#define CAM_ZOOM_COEFF_Q_LEN            10
#define CAM_PMBUF_Q_LEN                 (PARAM_BUF_NUM_MAX)
#define CAM_ALLOC_Q_LEN                 48

/* TODO: tuning ratio limit for power/image quality */
#define RATIO_SHIFT                     16

#define ISP_SLICE_OVERLAP_W_MAX         64
#define ALIGN_UP(a, x)                  (((a) + (x) - 1) & (~((x) - 1)))

/* TODO: need to pass the num to driver by hal */
#define CAP_NUM_COMMON                  1

/* limit the w & h to 1080P *2: 1920 *2 && 1080 *2 */
#define CAM_VIDEO_LIMIT_W               3840
#define CAM_VIDEO_LIMIT_H               2160
#define PRE_RDS_OUT                     3264

unsigned long g_reg_wr_flag;
spinlock_t g_reg_wr_lock;

enum camera_module_state {
	CAM_INIT = 0,
	CAM_IDLE,
	CAM_CFG_CH,
	CAM_STREAM_ON,
	CAM_STREAM_OFF,
	CAM_RUNNING,
	CAM_ERROR,
};

enum camera_csi_switch_mode {
	CAM_CSI_NORMAL_SWITCH,
	CAM_CSI_RECOVERY_SWITCH,
	CAM_CSI_MAX_SWITCH,
};

enum camera_recovery_state {
	CAM_RECOVERY_NONE = 0,
	CAM_RECOVERY_RUNNING,
	CAM_RECOVERY_DONE,
	CAM_RECOVERY_MAX,
};
/* Static Variables Declaration */
static struct camera_format output_img_fmt[] = {
	{ /*ISP_STORE_UYVY = 0 */
		.name = "4:2:2, packed, UYVY",
		.fourcc = IMG_PIX_FMT_UYVY,
		.depth = 16,
	},
	{ /* ISP_STORE_YUV422_3FRAME,*/
		.name = "YUV 4:2:2, planar, (Y-Cb-Cr)",
		.fourcc = IMG_PIX_FMT_YUV422P,
		.depth = 16,
	},
	{ /*ISP_STORE_YUV420_2FRAME*/
		.name = "YUV 4:2:0 planar (Y-CbCr)",
		.fourcc = IMG_PIX_FMT_NV12,
		.depth = 12,
	},
	{ /* ISP_STORE_YVU420_2FRAME,*/
		.name = "YVU 4:2:0 planar (Y-CrCb)",
		.fourcc = IMG_PIX_FMT_NV21,
		.depth = 12,
	},
	{ /*ISP_STORE_YUV420_3FRAME,*/
		.name = "YUV 4:2:0 planar (Y-Cb-Cr)",
		.fourcc = IMG_PIX_FMT_YUV420,
		.depth = 12,
	},
	{
		.name = "RawRGB",
		.fourcc = IMG_PIX_FMT_GREY,
		.depth = 8,
	},
};

struct camera_group;

/* user set information for camera module */
struct camera_uchannel {
	uint32_t sn_fmt;
	uint32_t dst_fmt;

	uint32_t deci_factor;/* for ISP output path */
	uint32_t is_high_fps;/* for DCAM slow motion feature */
	uint32_t high_fps_skip_num;/* for DCAM slow motion feature */
	uint32_t frame_num;/* for DCAM slow motion feature 60 frame*/
	uint32_t high_fps_skip_num1;/* for DCAM slow motion feature */
	uint32_t frame_num1;/* for DCAM slow motion feature 90 frame*/
	uint32_t is_compressed;/* for ISP output fbc format */

	int32_t sensor_raw_fmt;
	int32_t dcam_raw_fmt;
	int32_t dcam_output_bit;
	uint32_t dcam_out_pack;
	uint32_t pyr_data_bits;
	uint32_t pyr_is_pack;

	struct sprd_img_rect zoom_ratio_base;
	struct sprd_img_size src_size;
	struct sprd_img_rect src_crop;
	struct sprd_img_rect total_src_crop;
	struct sprd_img_size dst_size;
	uint32_t scene;

	/* for binding small picture */
	uint32_t slave_img_en;
	uint32_t slave_img_fmt;
	struct sprd_img_size slave_img_size;

	/* for close callback stream frame sync */
	uint32_t frame_sync_close;

	struct dcam_regular_desc regular_desc;
	struct sprd_vir_ch_info vir_channel[VIR_CH_NUM];
};

struct camera_uinfo {
	/* cap info */
	struct sprd_img_sensor_if sensor_if;
	struct sprd_img_size sn_size;
	struct sprd_img_size sn_max_size;
	struct sprd_img_rect sn_rect;
	uint32_t capture_mode;
	uint32_t capture_skip;
	uint32_t is_longexp;
	uint32_t is_4in1;
	uint32_t is_rgb_ltm;
	uint32_t is_yuv_ltm;
	uint32_t is_pyr_rec;
	uint32_t is_pyr_dec;
	uint32_t is_rgb_gtm;
	uint32_t is_dual;
	uint32_t is_dewarp;
	uint32_t dcam_slice_mode;/*1: hw,  2:sw*/
	uint32_t slice_num;
	uint32_t slice_count;
	uint32_t zsl_num;
	uint32_t zsk_skip_num;
	uint32_t need_share_buf;
	uint32_t fdr_cap_pre_num;
	uint32_t gtm_ltm_sw_calc;
	uint32_t zoom_conflict_with_ltm;
	/* for raw alg*/
	uint32_t is_raw_alg;
	uint32_t raw_alg_type;
	uint32_t param_frame_sync;
	/* for dcam raw*/
	uint32_t need_dcam_raw;
	uint32_t virtualsensor;/* 1: virtual sensor 0: normal */
};

struct sprd_img_flash_info {
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;
	uint32_t flash_last_status;
	struct sprd_img_set_flash set_param;
};

struct channel_context {
	enum cam_ch_id ch_id;
	uint32_t enable;
	uint32_t frm_base_id;
	uint32_t frm_cnt;
	uint32_t pack_bits;
	atomic_t err_status;

	uint32_t compress_input;
	uint32_t compress_4bit_bypass;
	uint32_t compress_3dnr;
	uint32_t compress_offline;
	uint32_t compress_output;

	int32_t dcam_ctx_id;
	int32_t dcam_path_id;
	uint32_t second_path_id;/* second path */
	uint32_t second_path_enable;

	/* for which need anoter dcam & path offline processing.*/
	int32_t aux_dcam_path_id;
	/* for 6pro fdr need two offline path support */
	int32_t aux_raw_path_id;

	int32_t isp_ctx_id;
	int32_t isp_path_id;
	int32_t slave_isp_ctx_id;
	int32_t slave_isp_path_id;
	int32_t isp_fdrl_ctx;
	int32_t isp_fdrl_path;
	int32_t isp_fdrh_ctx;
	int32_t isp_fdrh_path;

	uint32_t zsl_buffer_num;
	uint32_t zsl_skip_num;

	struct camera_uchannel ch_uinfo;
	struct img_size swap_size;
	struct img_trim trim_dcam;
	struct img_trim total_trim_dcam;
	struct img_trim trim_isp;
	struct img_size dst_dcam;
	uint32_t rds_ratio;
	uint32_t dcam_out_fmt; /*for online path*/
	/* to store isp offline param data if frame is discarded. */
	void *isp_updata;

	uint32_t alloc_start;
	struct completion alloc_com;

	struct isp_secen_ctrl_info isp_scene_ctrl;

	uint32_t uinfo_3dnr;/* set by hal, 1:hw 3dnr; */
	uint32_t type_3dnr;/* CAM_3DNR_HW:enable hw,and alloc buffer */
	uint32_t mode_ltm;
	uint32_t ltm_rgb;
	uint32_t ltm_yuv;
	uint32_t pyr_layer_num;
	uint32_t mode_gtm;
	uint32_t gtm_rgb;
	struct camera_frame *fdrl_zoom_buf;
	struct camera_frame *fdrh_zoom_buf;
	struct camera_frame *postproc_buf;
	struct camera_frame *nr3_bufs[ISP_NR3_BUF_NUM];
	struct camera_frame *ltm_bufs[LTM_MAX][ISP_LTM_BUF_NUM];
	struct camera_frame *res_frame;
	struct camera_frame *pyr_rec_buf;
	struct camera_frame *pyr_rec_buf_alg;
	struct camera_frame *pyr_dec_buf;
	struct camera_frame *pyr_dec_buf_alg;
	int32_t reserved_buf_fd;

	/* dcam/isp shared frame buffer for full path */
	struct camera_queue share_buf_queue;
	struct camera_queue zoom_coeff_queue;/* channel specific coef queue */
};

struct camera_module {
	uint32_t idx;
	atomic_t state;
	atomic_t timeout_flag;
	uint32_t master_flag; /* master cam capture flag */
	uint32_t compat_flag;
	struct mutex lock;
	struct camera_group *grp;
	uint32_t exit_flag;/*= 1, normal exit, =0, abnormal exit*/
	uint32_t private_key;
	int attach_sensor_id;
	uint32_t iommu_enable;

	uint32_t cur_sw_ctx_id;
	uint32_t cur_aux_sw_ctx_id;
	uint32_t offline_cxt_id;
	uint32_t raw_cap_fetch_fmt;
	enum camera_cap_status cap_status;
	enum dcam_capture_status dcam_cap_status;

	struct isp_pipe_dev *isp_dev_handle;
	struct dcam_pipe_dev *dcam_dev_handle;
	/* for which need another dcam offline processing raw data*/
	void *aux_dcam_dev;
	struct cam_flash_task_info *flash_core_handle;
	uint32_t dcam_idx;
	uint32_t aux_dcam_id;

	struct mutex fdr_lock;
	uint32_t fdr_init;
	uint32_t fdr_done;
	uint32_t fdr_cap_normal_cnt;
	uint32_t paused;

	uint32_t simu_fid;
	uint32_t simulator;
	uint32_t is_smooth_zoom;
	uint32_t zoom_solution;/* for dynamic zoom type swicth. */
	uint32_t rds_limit;/* raw downsizer limit */
	uint32_t binning_limit;/* binning limit: 1 - 1/2,  2 - 1/4 */
	uint32_t zoom_ratio;/* userspace zoom ratio for aem statis */
	struct camera_uinfo cam_uinfo;

	uint32_t last_channel_id;
	struct channel_context channel[CAM_CH_MAX];
	struct mutex buf_lock[CAM_CH_MAX];

	struct completion frm_com;
	struct camera_queue frm_queue;/* frame message queue for user*/
	struct camera_queue irq_queue;/* IRQ message queue for user*/
	struct camera_queue statis_queue;/* statis data queue or user*/
	struct camera_queue alloc_queue;/* alloc data queue or user*/

	struct cam_thread_info cap_thrd;
	struct cam_thread_info zoom_thrd;
	struct cam_thread_info buf_thrd;
	struct cam_thread_info dcam_offline_proc_thrd;

	/*  dump raw  for debug*/
	struct cam_thread_info dump_thrd;
	struct cam_dump_ctx dump_base;

	/*  dcam output mes for hal*/
	struct cam_thread_info mes_thrd;
	struct cam_mes_ctx mes_base;

	/* for raw capture post process */
	struct completion streamoff_com;

	struct timer_list cam_timer;

	struct camera_queue zsl_fifo_queue;/* for cmp timestamp */
	struct camera_frame *dual_frame;/* 0: no, to find, -1: no need find */
	atomic_t capture_frames_dcam;/* how many frames to report, -1:always */
	atomic_t cap_total_frames;
	atomic_t cap_skip_frames;
	atomic_t cap_zsl_frames;
	atomic_t cap_flag;
	int64_t capture_times;/* *ns, timestamp get from start_capture */
	uint32_t capture_scene;
	uint32_t lowlux_4in1;/* flag */
	struct camera_queue remosaic_queue;/* 4in1: save camera_frame when remosaic */
	uint32_t auto_3dnr;/* 1: enable hw,and alloc buffer before stream on */
	struct sprd_img_flash_info flash_info;
	uint32_t flash_skip_fid;
	uint32_t is_flash_status;
	uint32_t path_state;

	struct camera_buf pmbuf_array[PARAM_BUF_NUM_MAX];
	struct camera_queue param_queue;
	int pmq_init;
	uint32_t raw_callback;
	struct mutex zoom_lock;
};

struct camera_group {
	atomic_t camera_opened;
	bool ca_conn;

	spinlock_t module_lock;
	uint32_t module_used;
	struct camera_module *module[CAM_COUNT];

	spinlock_t rawproc_lock;
	uint32_t rawproc_in;

	struct mutex dual_deal_lock;
	uint32_t dcam_count;/*dts cfg dcam count*/
	uint32_t isp_count;/*dts cfg isp count*/

	atomic_t runner_nr; /*running camera num*/

	struct miscdevice *md;
	struct platform_device *pdev;
	struct camera_queue empty_frm_q;
	struct camera_queue empty_state_q;
	struct camera_queue empty_interruption_q;
	struct camera_queue empty_mv_state_q;
	struct sprd_cam_sec_cfg camsec_cfg;
	struct camera_debugger debugger;
	struct cam_hw_info *hw_info;
	struct sprd_img_size mul_sn_max_size;
	atomic_t mul_buf_alloced;
	atomic_t mul_pyr_buf_alloced;
	uint32_t is_mul_buf_share;
	/* mul camera shared frame buffer for full path */
	struct camera_queue mul_share_buf_q;
	struct camera_frame *mul_share_pyr_dec_buf;
	struct camera_frame *mul_share_pyr_rec_buf;
	struct mutex pyr_mulshare_lock;
	struct wakeup_source *ws;

	atomic_t recovery_state;
	struct rw_semaphore switch_recovery_lock;
	struct cam_thread_info recovery_thrd;
};

struct secondary_buffer_info {
	struct sprd_img_frm_addr frame_addr_array[IMG_PATH_BUFFER_COUNT];
	uint32_t fd_array[IMG_PATH_BUFFER_COUNT];
};

#define COMPAT_SPRD_ISP_IO_CFG_PARAM \
	_IOWR(SPRD_IMG_IO_MAGIC, 41, struct compat_isp_io_param)

struct compat_isp_io_param {
	uint32_t scene_id;
	uint32_t sub_block;
	uint32_t property;
	u32 property_param;
};

struct cam_ioctl_cmd {
	unsigned int cmd;
	int (*cmd_proc)(struct camera_module *module, unsigned long arg);
};

struct camera_queue *g_empty_frm_q;
struct camera_queue *g_empty_state_q;
struct camera_queue *g_empty_interruption_q;
struct camera_queue *g_empty_mv_state_q;
struct cam_global_ctrl g_camctrl = {
	ZOOM_BINNING2,
	DCAM_SCALE_DOWN_MAX * 10,
	0,
	ISP_MAX_LINE_WIDTH
};

static inline uint32_t camcore_ratio16_divide(uint64_t num, uint32_t ratio16)
{
	return (uint32_t)div64_u64(num << 16, ratio16);
}

static inline uint32_t camcore_scale_fix(uint32_t size_in, uint32_t ratio16)
{
	uint64_t size_scaled;

	size_scaled = (uint64_t)size_in;
	size_scaled <<= (2 * RATIO_SHIFT);
	size_scaled = ((div64_u64(size_scaled, ratio16)) >> RATIO_SHIFT);
	return (uint32_t)size_scaled;
}

static inline void camcore_largest_crop_get(
	struct sprd_img_rect *crop_dst, struct sprd_img_rect *crop1)
{
	uint32_t end_x, end_y;
	uint32_t end_x_new, end_y_new;

	if (crop1) {
		end_x = crop_dst->x + crop_dst->w;
		end_y = crop_dst->y + crop_dst->h;
		end_x_new = crop1->x + crop1->w;
		end_y_new = crop1->y + crop1->h;

		crop_dst->x = MIN(crop1->x, crop_dst->x);
		crop_dst->y = MIN(crop1->y, crop_dst->y);
		end_x_new = MAX(end_x, end_x_new);
		end_y_new = MAX(end_y, end_y_new);
		crop_dst->w = end_x_new - crop_dst->x;
		crop_dst->h = end_y_new - crop_dst->y;
	}
}

static int camcore_crop_size_align(
	struct camera_module *module, struct sprd_img_rect *crop)
{
	struct img_size max_size;

	max_size.w = module->cam_uinfo.sn_rect.w;
	max_size.h = module->cam_uinfo.sn_rect.h;
	/* Sharkl5pro crop align need to do research*/
	crop->w = ((crop->w + DCAM_PATH_CROP_ALIGN - 1)
		& ~(DCAM_PATH_CROP_ALIGN - 1));
	crop->h = ((crop->h + DCAM_PATH_CROP_ALIGN - 1)
		& ~(DCAM_PATH_CROP_ALIGN - 1));
	if (max_size.w > crop->w)
		crop->x = (max_size.w - crop->w) / 2;
	if (max_size.h > crop->h)
		crop->y = (max_size.h - crop->h) / 2;
	crop->x &= ~1;
	crop->y &= ~1;

	if ((crop->x + crop->w) > max_size.w)
		crop->w -= DCAM_PATH_CROP_ALIGN;
	if ((crop->y + crop->h) > max_size.h)
		crop->h -= DCAM_PATH_CROP_ALIGN;

	pr_info("aligned crop %d %d %d %d.  max %d %d\n",
		crop->x, crop->y, crop->w, crop->h, max_size.w, max_size.h);

	return 0;
}

static void camcore_diff_trim_get(struct sprd_img_rect *orig,
	uint32_t ratio16, struct img_trim *trim0, struct img_trim *trim1)
{
	trim1->start_x = camcore_scale_fix(orig->x - trim0->start_x, ratio16);
	trim1->start_y = camcore_scale_fix(orig->y - trim0->start_y, ratio16);
	trim1->size_x = camcore_scale_fix(orig->w, ratio16);
	trim1->size_x = ALIGN(trim1->size_x, 2);
	trim1->size_y = camcore_scale_fix(orig->h, ratio16);
}

static int camcore_cap_info_set(struct camera_module *module)
{
	int ret = 0;
	struct camera_uinfo *info = &module->cam_uinfo;
	struct sprd_img_sensor_if *sensor_if = &info->sensor_if;
	struct dcam_cap_cfg cap_info = { 0 };

	cap_info.mode = info->capture_mode;
	cap_info.frm_skip = info->capture_skip;
	cap_info.is_4in1 = info->is_4in1;
	cap_info.dcam_slice_mode = info->dcam_slice_mode;
	cap_info.sensor_if = sensor_if->if_type;
	cap_info.format = sensor_if->img_fmt;
	cap_info.pattern = sensor_if->img_ptn;
	cap_info.frm_deci = sensor_if->frm_deci;
	cap_info.is_cphy = sensor_if->if_spec.mipi.is_cphy;
	if (cap_info.sensor_if == DCAM_CAP_IF_CSI2) {
		cap_info.href = sensor_if->if_spec.mipi.use_href;
		cap_info.data_bits = sensor_if->if_spec.mipi.bits_per_pxl;
	}
	cap_info.cap_size.start_x = info->sn_rect.x;
	cap_info.cap_size.start_y = info->sn_rect.y;
	cap_info.cap_size.size_x = info->sn_rect.w;
	cap_info.cap_size.size_y = info->sn_rect.h;

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
		DCAM_IOCTL_CFG_CAP, &cap_info);
	/* for dcam1 mipicap */
	if (info->dcam_slice_mode && module->aux_dcam_dev)
		ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
				DCAM_IOCTL_CFG_CAP, &cap_info);
	return ret;
}

static int camcore_slice_num_info_get(struct sprd_img_size *src, struct sprd_img_size *dst)
{
	uint32_t slice_num, slice_w, slice_w_out;
	uint32_t slice_max_w, max_w;
	uint32_t linebuf_len;
	uint32_t input_w = src->w;
	uint32_t output_w = dst->w;

	/* based input */
	linebuf_len = g_camctrl.isp_linebuf_len;
	max_w = input_w;
	slice_num = 1;
	slice_max_w = linebuf_len - ISP_SLICE_OVERLAP_W_MAX;
	if (max_w <= linebuf_len) {
		slice_w = max_w;
	} else {
		do {
			slice_num++;
			slice_w = (max_w + slice_num - 1) / slice_num;
		} while (slice_w >= slice_max_w);
	}
	pr_debug("input_w %d, slice_num %d, slice_w %d\n",
		max_w, slice_num, slice_w);

	/* based output */
	max_w = output_w;
	slice_num = 1;
	slice_max_w = linebuf_len;
	if (max_w > 0) {
		if (max_w > linebuf_len) {
			do {
				slice_num++;
				slice_w_out = (max_w + slice_num - 1) / slice_num;
			} while (slice_w_out >= slice_max_w);
		}
		/* set to equivalent input size, because slice size based on input. */
		slice_w_out = (input_w + slice_num - 1) / slice_num;
	} else
		slice_w_out = slice_w;
	pr_debug("max output w %d, slice_num %d, out limited slice_w %d\n",
		max_w, slice_num, slice_w_out);

	slice_w = MIN(slice_w, slice_w_out);
	slice_w = ALIGN(slice_w, 2);
	slice_num = (input_w + slice_w - 1) / slice_w;
	if (dst->h > DCAM_SW_SLICE_HEIGHT_MAX)
		slice_num *= 2;
	return slice_num;
}

/* 4in1_raw_capture
 * get the second buffer from the same fd for (bin) path
 * input: i: get i group buffer
 */
static struct camera_frame *camcore_secondary_buf_get(
	struct secondary_buffer_info *p, struct channel_context *ch, uint32_t i)
{
	struct camera_frame *pframe;
	int ret;
	uint32_t offset = 0;
	uint32_t pack_bits = 0;

	pframe = cam_queue_empty_frame_get();
	pframe->buf.type = CAM_BUF_USER;
	pframe->buf.mfd[0] = p->fd_array[i];
	/* raw capture: 4cell + bin-sum, cal offset */
	if(ch->dcam_path_id == 0)
		pack_bits = 0;
	else
		pack_bits = ch->pack_bits;
	offset = cal_sprd_raw_pitch(ch->ch_uinfo.src_size.w, pack_bits);
	offset *= ch->ch_uinfo.src_size.h;
	offset = ALIGN_UP(offset, 4096);
	/* first buf offset: p->frame_addr_array[i].y */
	offset += p->frame_addr_array[i].y;
	pr_debug("start 0x%x 0x%x 0x%x offset 0x%x\n",
		p->frame_addr_array[i].y,
		p->frame_addr_array[i].u,
		p->frame_addr_array[i].v, offset);
	pframe->buf.offset[0] = offset;
	pframe->channel_id = ch->ch_id;
	pframe->img_fmt = ch->ch_uinfo.dst_fmt;

	ret = cam_buf_ionbuf_get(&pframe->buf);
	if (ret) {
		cam_queue_empty_frame_put(pframe);
		pr_err("fail to get second buffer fail, ret %d\n", ret);
		return NULL;
	}

	return pframe;
}

static int camcore_capture_3dnr_set(struct camera_module *module,
		struct channel_context *ch)
{
	uint32_t mode_3dnr;

	if ((!module) || (!ch))
		return -EFAULT;
	mode_3dnr = MODE_3DNR_OFF;
	if (ch->uinfo_3dnr) {
		if (ch->ch_id == CAM_CH_CAP)
			mode_3dnr = MODE_3DNR_CAP;
		else
			mode_3dnr = MODE_3DNR_PRE;
	}
	pr_debug("mode %d\n", mode_3dnr);
	module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, ISP_PATH_CFG_3DNR_MODE,
		ch->isp_ctx_id,
		ch->isp_path_id, &mode_3dnr);

	return 0;
}

/* return the number of how many buf in the out_buf_queue */
static uint32_t camcore_outbuf_queue_cnt_get(void *dev, int path_id)
{
	struct dcam_path_desc *path;

	path = &(((struct dcam_sw_context *)dev)->path[path_id]);

	return cam_queue_cnt_get(&path->out_buf_queue);
}

static void camcore_k_frame_put(void *param)
{
	int ret = 0;
	struct camera_frame *frame;

	if (!param) {
		pr_err("fail to get valid param\n");
		return;
	}

	frame = (struct camera_frame *)param;
	if (frame->buf.type == CAM_BUF_USER)
		cam_buf_ionbuf_put(&frame->buf);
	else {
		if (frame->buf.mapping_state)
			cam_buf_kunmap(&frame->buf);
		cam_buf_free(&frame->buf);
	}
	ret = cam_queue_empty_frame_put(frame);
}

static void camcore_empty_frame_put(void *param)
{
	int ret = 0;
	struct camera_frame *frame;
	struct camera_module *module;

	if (!param) {
		pr_err("fail to get valid param\n");
		return;
	}

	frame = (struct camera_frame *)param;
	module = frame->priv_data;

	if (frame->priv_data) {
		if (!frame->irq_type)
			kfree(frame->priv_data);
		else if (module && module->exit_flag == 1)
			cam_buf_ionbuf_put(&frame->buf);
	}
	ret = cam_queue_empty_frame_put(frame);
}

/* No need release buffer, only give back camera_frame
 * for remosaic_queue, it save camera_frame info when
 * buf send to hal for remosaic, use again when 4in1_post
 */
static void camcore_camera_frame_release(void *param)
{
	struct camera_frame *frame;

	if (!param)
		return;
	frame = (struct camera_frame *)param;
	cam_queue_empty_frame_put(frame);
}

static int camcore_resframe_set(struct camera_module *module)
{
	int ret = 0;
	struct channel_context *ch = NULL, *ch_prv = NULL;
	uint32_t i = 0, j = 0, cmd = ISP_PATH_CFG_OUTPUT_RESERVED_BUF;
	uint32_t max_size = 0, out_size = 0, in_size = 0;
	uint32_t src_w = 0, src_h = 0;
	struct camera_frame *pframe = NULL;
	struct camera_frame *pframe1 = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct sprd_img_mipi_if *mipi = NULL;
	struct dcam_compress_cal_para cal_fbc = {0};

	mipi = &module->cam_uinfo.sensor_if.if_spec.mipi;
	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (ch->enable) {
			src_w = ch->ch_uinfo.src_size.w;
			src_h = ch->ch_uinfo.src_size.h;
			if (ch->ch_uinfo.dst_fmt != IMG_PIX_FMT_GREY)
				out_size = ch->ch_uinfo.dst_size.w *ch->ch_uinfo.dst_size.h * 3 / 2;
			else
				out_size = cal_sprd_raw_pitch(ch->ch_uinfo.dst_size.w, ch->ch_uinfo.dcam_raw_fmt)
					* ch->ch_uinfo.dst_size.h;

			if (ch->compress_input) {
				cal_fbc.compress_4bit_bypass = ch->compress_4bit_bypass;
				cal_fbc.data_bits = ch->ch_uinfo.dcam_output_bit;
				cal_fbc.fmt = ch->dcam_out_fmt;
				cal_fbc.height = src_h;
				cal_fbc.width = src_w;
				in_size = dcam_if_cal_compressed_size (&cal_fbc);
			} else if (ch->ch_uinfo.sn_fmt != IMG_PIX_FMT_GREY)
				in_size = src_w * src_h * 3 / 2;
			else if (ch->dcam_out_fmt == DCAM_STORE_RAW_BASE)
				in_size = cal_sprd_raw_pitch(src_w, ch->ch_uinfo.dcam_raw_fmt) * src_h;
			else if ((ch->dcam_out_fmt == DCAM_STORE_YUV420) || (ch->dcam_out_fmt == DCAM_STORE_YVU420))
				in_size = cal_sprd_yuv_pitch(src_w, ch->ch_uinfo.dcam_output_bit, ch->ch_uinfo.dcam_out_pack)
					* src_h * 3 / 2;

			if (module->cam_uinfo.is_pyr_rec && ch->ch_id != CAM_CH_CAP)
				in_size += dcam_if_cal_pyramid_size(src_w, src_h, ch->ch_uinfo.pyr_data_bits, ch->ch_uinfo.pyr_is_pack, 1, DCAM_PYR_DEC_LAYER_NUM);
			in_size = ALIGN(in_size, CAM_BUF_ALIGN_SIZE);

			max_size = max3(max_size, out_size, in_size);
			pr_debug("cam%d, ch %d, max_size = %d, %d, %d\n", module->idx, i, max_size, in_size, out_size);
		}
	}

	ch_prv = &module->channel[CAM_CH_PRE];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		pframe = ch->res_frame;
		if (!ch->enable || !pframe)
			continue;
		if (ch->isp_path_id >= 0 && ch->ch_uinfo.dst_fmt != IMG_PIX_FMT_GREY) {
			if (((ch->ch_id == CAM_CH_CAP)
				|| (ch->ch_id == CAM_CH_PRE)
				|| (ch->ch_id == CAM_CH_VID && !ch_prv->enable))) {
				cmd = DCAM_PATH_CFG_OUTPUT_RESERVED_BUF;
				pframe1 = cam_queue_empty_frame_get();
				pframe1->is_reserved = 1;
				pframe1->buf.type = CAM_BUF_USER;
				pframe1->buf.mfd[0] = pframe->buf.mfd[0];
				pframe1->buf.offset[0] = pframe->buf.offset[0];
				pframe1->buf.offset[1] = pframe->buf.offset[1];
				pframe1->buf.offset[2] = pframe->buf.offset[2];
				pframe1->channel_id = ch->ch_id;
				if (module->cam_uinfo.is_pyr_rec && ch->ch_id != CAM_CH_CAP)
					pframe1->pyr_status = ONLINE_DEC_ON;

				ret = cam_buf_ionbuf_get(&pframe1->buf);
				if (ret) {
					pr_err("fail to get ionbuf on cam%d, ch %d\n", module->idx, i);
					cam_queue_empty_frame_put(pframe1);
					ret = -EFAULT;
					break;
				}

				for (j = 0; j < 3; j++)
					pframe1->buf.size[j] = max_size;
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, cmd, ch->dcam_path_id, pframe1);

				if (ret) {
					pr_err("fail to cfg path on cam%d, ch %d\n", module->idx, i);
					cam_buf_ionbuf_put(&pframe1->buf);
					cam_queue_empty_frame_put(pframe1);
				}
			}

			cmd = ISP_PATH_CFG_OUTPUT_RESERVED_BUF;
			for (j = 0; j < 3; j++)
				pframe->buf.size[j] = max_size;

			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, cmd,
				ch->isp_ctx_id, ch->isp_path_id, pframe);
		} else {
			cmd = DCAM_PATH_CFG_OUTPUT_RESERVED_BUF;
			for (j = 0; j < 3; j++)
				pframe->buf.size[j] = max_size;
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, cmd, ch->dcam_path_id, pframe);
			/* 4in1_raw_capture, maybe need two image once */
			if (ch->second_path_enable) {
				pframe1 = cam_queue_empty_frame_get();
				pframe1->is_reserved = 1;
				pframe1->buf.type = CAM_BUF_USER;
				pframe1->buf.mfd[0] = pframe->buf.mfd[0];
				pframe1->buf.offset[0] = pframe->buf.offset[0];
				pframe1->buf.offset[1] = pframe->buf.offset[1];
				pframe1->buf.offset[2] = pframe->buf.offset[2];
				pframe1->channel_id = ch->ch_id;

				ret = cam_buf_ionbuf_get(&pframe1->buf);
				if (ret) {
					pr_err("fail to get ionbuf on cam%d, ch %d\n", module->idx, i);
					cam_queue_empty_frame_put(pframe1);
					ret = -EFAULT;
					break;
				}

				for (j = 0; j < 3; j++)
					pframe1->buf.size[j] = max_size;
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, cmd, ch->second_path_id, pframe1);
			}
		}

		if (ret) {
			pr_err("fail to set output buffer for ch%d.\n", ch->ch_id);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			ret = -EFAULT;
			break;
		}
	}

	return ret;
}

static int camcore_param_buffer_cfg(
		struct camera_module *module,
		struct isp_statis_buf_input *input)
{
	int ret = 0;
	int j;
	int32_t mfd;
	uint32_t offset;
	enum isp_statis_buf_type stats_type;
	struct camera_buf *ion_buf = NULL;
	struct camera_frame *pframe = NULL;

	stats_type = input->type;
	if (stats_type == STATIS_DBG_INIT)
		goto buf_init;
	if (stats_type == STATIS_PARAM &&
		atomic_read(&module->state) == CAM_RUNNING)
		goto cfg_single;

	return 0;

buf_init:

	pr_info("cam%d, start\n", module->idx);
	memset(&module->pmbuf_array[0], 0, sizeof(module->pmbuf_array));
	cam_queue_init(&module->param_queue,
		CAM_PMBUF_Q_LEN, camcore_camera_frame_release);

	for (j = 0; j < PARAM_BUF_NUM_MAX; j++) {
		mfd = input->mfd_pmdbg[j];
		if (mfd <= 0)
			continue;

		pr_debug("cam%d, param buf %d mfd %d, offset %d\n",
			module->idx, j, mfd, input->offset_pmdbg[j]);

		ion_buf = &module->pmbuf_array[j];
		ion_buf->mfd[0] = mfd;
		ion_buf->offset[0] = input->offset_pmdbg[j];
		ion_buf->type = CAM_BUF_USER;
		ret = cam_buf_ionbuf_get(ion_buf);
		if (ret) {
			memset(ion_buf, 0, sizeof(struct camera_buf));
			continue;
		}
		ret = cam_buf_kmap(ion_buf);
		if (ret) {
			pr_err("fail to kmap statis buf %d\n", mfd);
			cam_buf_ionbuf_put(ion_buf);
			memset(ion_buf, 0, sizeof(struct camera_buf));
			continue;
		}

		pframe = cam_queue_empty_frame_get();
		pframe->irq_property = STATIS_PARAM;
		pframe->buf = *ion_buf;
		ret = cam_queue_enqueue(&module->param_queue, &pframe->list);
		if (ret) {
			pr_warn("warning: cam%d pmbufq overflow\n", module->idx);
			cam_queue_empty_frame_put(pframe);
		}

		pr_debug("cam%d,mfd %d, off %d, kaddr 0x%lx\n",
			module->idx, mfd, ion_buf->offset[0], ion_buf->addr_k[0]);
	}

	module->pmq_init = 1;
	pr_info("cam%d init done\n", module->idx);
	return 0;

cfg_single:
	for (j = 0; j < PARAM_BUF_NUM_MAX; j++) {
		mfd = module->pmbuf_array[j].mfd[0];
		offset = module->pmbuf_array[j].offset[0];
		if ((mfd > 0) && (mfd == input->mfd)
			&& (offset == input->offset)) {
			ion_buf = &module->pmbuf_array[j];
			break;
		}
	}

	if (ion_buf == NULL) {
		pr_err("fail to get pm buf %d\n", input->mfd);
		ret = -EINVAL;
		goto exit;
	}

	pframe = cam_queue_empty_frame_get();
	pframe->irq_property = input->type;
	pframe->buf = *ion_buf;
	ret = cam_queue_enqueue(&module->param_queue, &pframe->list);
	pr_debug("cam%d, pmbuf, mfd %d, off %d,kaddr 0x%lx\n",
		module->idx, mfd, offset, pframe->buf.addr_k[0]);
	if (ret)
		cam_queue_empty_frame_put(pframe);
exit:
	return ret;
}

static int camcore_param_buffer_uncfg(struct camera_module *module)
{
	int j;
	int32_t mfd;
	struct camera_buf *ion_buf = NULL;

	if (module->pmq_init == 0)
		return 0;

	module->pmq_init = 0;
	cam_queue_clear(&module->param_queue, struct camera_frame, list);

	for (j = 0; j < PARAM_BUF_NUM_MAX; j++) {
		ion_buf = &module->pmbuf_array[j];
		mfd = ion_buf->mfd[0];
		if (mfd <= 0)
			continue;

		pr_debug("cam%d, j %d,  mfd %d, offset %d\n",
			module->idx, j, mfd, ion_buf->offset[0]);
		cam_buf_kunmap(ion_buf);
		cam_buf_ionbuf_put(ion_buf);
		memset(ion_buf, 0, sizeof(struct camera_buf));
	}

	pr_info("cam%d done\n", module->idx);
	return 0;
}

static void camcore_compression_cal(struct camera_module *module)
{
	uint32_t dcam_hw_ctx_id =  DCAM_HW_CONTEXT_MAX;
	uint32_t nr3_compress_eb = 0;
	struct channel_context *ch_pre, *ch_cap, *ch_vid, *ch_raw;
	struct cam_hw_info *dcam_hw;
	struct compression_override *override;

	dcam_hw_ctx_id = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;
	ch_pre = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
	ch_raw = &module->channel[CAM_CH_RAW];
	/*
	 * Enable compression for DCAM path by default. Full path is prior to
	 * bin path.
	 */
	dcam_hw = module->grp->hw_info;
	ch_cap->compress_input = ch_cap->enable
		&& ch_cap->ch_uinfo.sn_fmt == IMG_PIX_FMT_GREY
		&& !ch_cap->ch_uinfo.is_high_fps
		&& !module->cam_uinfo.is_4in1
		&& dcam_hw->ip_dcam[dcam_hw_ctx_id]->dcam_full_fbc_mode
		&& !(g_dbg_fbc_control & DEBUG_FBC_CRL_FULL);
	ch_pre->compress_input = ch_pre->enable
		&& ch_pre->ch_uinfo.sn_fmt == IMG_PIX_FMT_GREY
		&& !ch_pre->ch_uinfo.is_high_fps
		&& !module->cam_uinfo.is_4in1
		&& dcam_hw->ip_dcam[dcam_hw_ctx_id]->dcam_bin_fbc_mode
		&& !(g_dbg_fbc_control & DEBUG_FBC_CRL_BIN);
	ch_vid->compress_input = ch_pre->compress_input;
	ch_raw->compress_input = ch_raw->enable
		&& ch_raw->ch_uinfo.sn_fmt == IMG_PIX_FMT_GREY
		&& !ch_raw->ch_uinfo.is_high_fps
		&& !module->cam_uinfo.is_4in1
		&& dcam_hw->ip_dcam[dcam_hw_ctx_id]->dcam_raw_fbc_mode
		&& !(g_dbg_fbc_control & DEBUG_FBC_CRL_RAW);
	ch_cap->compress_offline = dcam_hw->ip_dcam[dcam_hw_ctx_id]->dcam_offline_fbc_mode;
	ch_raw->compress_input = ch_cap->compress_input ? 0 : ch_raw->compress_input;

	/* Disable compression for 3DNR by default */
	nr3_compress_eb = module->isp_dev_handle->isp_hw->ip_isp->nr3_compress_support;
	if (ch_cap->uinfo_3dnr)
		ch_cap->compress_3dnr = nr3_compress_eb;
	if (ch_pre->uinfo_3dnr) {
		ch_pre->compress_3dnr = nr3_compress_eb;
		ch_vid->compress_3dnr = ch_pre->compress_3dnr;
	}
	ch_raw->compress_3dnr = 0;

	/*
	 * Enable compression for ISP store according to HAL setting. Normally
	 * this only happens in slow motion and only for video path.
	 * raw channel has no isp path
	 */
	ch_cap->compress_output =
		ch_cap->enable && ch_cap->ch_uinfo.is_compressed;
	ch_pre->compress_output =
		ch_pre->enable && ch_pre->ch_uinfo.is_compressed;
	ch_vid->compress_output =
		ch_vid->enable && ch_vid->ch_uinfo.is_compressed;
	ch_raw->compress_output = 0;

	/* Bypass compression low_4bit by default */
	ch_cap->compress_4bit_bypass = 1;
	ch_pre->compress_4bit_bypass = 1;
	ch_vid->compress_4bit_bypass = 1;
	ch_raw->compress_4bit_bypass = 1;

	/* manually control compression policy here */
	override = &module->grp->debugger.compression[module->idx];
	if (override->enable) {
		ch_cap->compress_input = override->override[CH_CAP][FBC_DCAM];
		ch_cap->compress_3dnr = override->override[CH_CAP][FBC_3DNR];
		ch_cap->compress_output = override->override[CH_CAP][FBC_ISP];

		ch_pre->compress_input = override->override[CH_PRE][FBC_DCAM];
		ch_pre->compress_3dnr = override->override[CH_PRE][FBC_3DNR];
		ch_pre->compress_output = override->override[CH_PRE][FBC_ISP];

		ch_vid->compress_input = override->override[CH_VID][FBC_DCAM];
		ch_vid->compress_3dnr = override->override[CH_VID][FBC_3DNR];
		ch_vid->compress_output = override->override[CH_VID][FBC_ISP];
	}

	if (dcam_hw_ctx_id > DCAM_HW_CONTEXT_MAX) {
		ch_cap->compress_input = 0;
		ch_pre->compress_input = 0;
		ch_raw->compress_input = 0;
	}

	/* dcam not support fbc when dcam need fetch */
	if (module->cam_uinfo.dcam_slice_mode ||
		module->cam_uinfo.is_4in1 || module->cam_uinfo.is_raw_alg) {
		ch_cap->compress_input = 0;
		ch_raw->compress_input = 0;
	}

	pr_info("cam%d: cap %u %u %u, pre %u %u %u, vid %u %u %u raw %u offline %u.\n",
		module->idx,
		ch_cap->compress_input, ch_cap->compress_3dnr,
		ch_cap->compress_output,
		ch_pre->compress_input, ch_pre->compress_3dnr,
		ch_pre->compress_output,
		ch_vid->compress_input, ch_vid->compress_3dnr,
		ch_vid->compress_output,
		ch_raw->compress_input,
		ch_cap->compress_offline);
}

static void camcore_compression_config(struct camera_module *module)
{
	int fbc_mode = DCAM_FBC_DISABLE;
	uint32_t dcam_hw_ctx_id = DCAM_HW_CONTEXT_MAX;
	struct channel_context *ch_pre, *ch_cap, *ch_vid, *ch_raw;
	struct isp_ctx_compress_desc ctx_compression_desc;
	struct isp_path_compression_desc path_compression_desc;
	struct cam_hw_info *hw = NULL;
	struct compression_override *override = NULL;
	struct dcam_sw_context *sw_handle = NULL;

	ch_pre = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
	ch_raw = &module->channel[CAM_CH_RAW];
	hw = module->grp->hw_info;
	sw_handle = module->dcam_dev_handle->sw_ctx;
	override = &module->grp->debugger.compression[module->idx];
	dcam_hw_ctx_id = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;

	if (ch_cap->compress_input) {
		fbc_mode = hw->ip_dcam[dcam_hw_ctx_id]->dcam_full_fbc_mode;
		/* manually control compression policy here */
		if (override->enable)
			fbc_mode = override->override[CH_CAP][FBC_DCAM];
		if (DCAM_FBC_FULL_14_BIT == fbc_mode)
			ch_cap->compress_4bit_bypass = 0;
	} else
		fbc_mode = DCAM_FBC_DISABLE;

	if (ch_cap->enable)
		sw_handle[module->cur_sw_ctx_id].path[ch_cap->dcam_path_id].fbc_mode = fbc_mode;
	if (!fbc_mode)
		ch_cap->compress_input = 0;

	module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			DCAM_IOCTL_CFG_FBC, &fbc_mode);
	pr_debug("cap fbc = %d\n", fbc_mode);

	if (ch_pre->compress_input) {
		fbc_mode = hw->ip_dcam[dcam_hw_ctx_id]->dcam_bin_fbc_mode;
		/* manually control compression policy here */
		if (override->enable)
			fbc_mode = override->override[CH_PRE][FBC_DCAM];
		if (DCAM_FBC_BIN_14_BIT == fbc_mode)
			ch_pre->compress_4bit_bypass = 0;
	} else
		fbc_mode = DCAM_FBC_DISABLE;

	if (ch_pre->enable)
		sw_handle[module->cur_sw_ctx_id].path[ch_pre->dcam_path_id].fbc_mode = fbc_mode;
	else if ((!ch_pre->enable) && ch_vid->enable)
		sw_handle[module->cur_sw_ctx_id].path[ch_vid->dcam_path_id].fbc_mode = fbc_mode;

	if (!fbc_mode)
		ch_pre->compress_input = 0;

	ch_vid->compress_input = ch_pre->compress_input;
	ch_vid->compress_4bit_bypass = ch_pre->compress_4bit_bypass;

	module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			DCAM_IOCTL_CFG_FBC, &fbc_mode);
	pr_debug("pre fbc = %d\n", fbc_mode);

	if (ch_raw->compress_input) {
		fbc_mode = hw->ip_dcam[dcam_hw_ctx_id]->dcam_raw_fbc_mode;
		/* manually control compression policy here */
		if (override->enable)
			fbc_mode = override->override[CH_PRE][FBC_DCAM];
		if (DCAM_FBC_RAW_14_BIT == fbc_mode)
			ch_pre->compress_4bit_bypass = 0;
	} else
		fbc_mode = DCAM_FBC_DISABLE;

	if (ch_raw->enable)
		sw_handle[module->cur_sw_ctx_id].path[ch_raw->dcam_path_id].fbc_mode = fbc_mode;
	if (!fbc_mode)
		ch_raw->compress_input = 0;

	pr_debug("raw fbc = %d\n", fbc_mode);

	/* dcam offline fbc */
	if (ch_cap->compress_offline)
		fbc_mode = hw->ip_dcam[DCAM_HW_CONTEXT_1]->dcam_offline_fbc_mode;
	else
		fbc_mode = DCAM_FBC_DISABLE;

	if (ch_cap->enable && ch_cap->compress_offline)
		sw_handle[module->offline_cxt_id].path[ch_cap->aux_dcam_path_id].fbc_mode = fbc_mode;
	if (!fbc_mode)
		ch_cap->compress_offline = 0;

	pr_debug("dcam offline fbc = %d\n", fbc_mode);

	module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			DCAM_IOCTL_CFG_FBC, &fbc_mode);

	/* capture context */
	if (ch_cap->enable) {
		ctx_compression_desc.fetch_fbd = ch_cap->compress_input;
		ctx_compression_desc.fetch_fbd_4bit_bypass = ch_cap->compress_4bit_bypass;
		ctx_compression_desc.nr3_fbc_fbd = ch_cap->compress_3dnr;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_COMPRESSION,
				ch_cap->isp_ctx_id,
				0, &ctx_compression_desc);

		path_compression_desc.store_fbc = ch_cap->compress_output;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_COMPRESSION,
				ch_cap->isp_ctx_id,
				ch_cap->isp_path_id,
				&path_compression_desc);
	}

	/* preview context */
	if (ch_pre->enable) {
		ctx_compression_desc.fetch_fbd = ch_pre->compress_input;
		ctx_compression_desc.fetch_fbd_4bit_bypass = ch_pre->compress_4bit_bypass;
		ctx_compression_desc.nr3_fbc_fbd = ch_pre->compress_3dnr;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_COMPRESSION,
				ch_pre->isp_ctx_id,
				0, &ctx_compression_desc);

		path_compression_desc.store_fbc = ch_pre->compress_output;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_COMPRESSION,
				ch_pre->isp_ctx_id,
				ch_pre->isp_path_id,
				&path_compression_desc);
	}

	/* video context */
	if (ch_vid->enable) {
		ctx_compression_desc.fetch_fbd = ch_vid->compress_input;
		ctx_compression_desc.fetch_fbd_4bit_bypass = ch_vid->compress_4bit_bypass;
		ctx_compression_desc.nr3_fbc_fbd = ch_vid->compress_3dnr;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_COMPRESSION,
				ch_vid->isp_ctx_id,
				0, &ctx_compression_desc);

		path_compression_desc.store_fbc = ch_vid->compress_output;
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_COMPRESSION,
				ch_vid->isp_ctx_id,
				ch_vid->isp_path_id,
				&path_compression_desc);
	}
}

static int camcore_buffer_path_cfg(struct camera_module *module,
	uint32_t index)
{
	int ret = 0;
	uint32_t j = 0, isp_ctx_id = 0, isp_path_id = 0;
	struct channel_context *ch = NULL;
	struct cam_hw_info *hw = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct camera_frame *frame = NULL;

	if (!module) {
		pr_err("fail to get input ptr\n");
		return -EFAULT;
	}

	ch = &module->channel[index];
	hw = module->grp->hw_info;
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	if (!ch->alloc_start)
		return 0;

	if (index == CAM_CH_PRE || index == CAM_CH_VID || (index == CAM_CH_CAP && !module->channel[CAM_CH_PRE].enable)) {
		ret = wait_for_completion_interruptible(&ch->alloc_com);
		if (ret != 0) {
			pr_err("fail to config channel/path param work %d\n", ret);
			goto exit;
		}
	}

	if (atomic_read(&ch->err_status) != 0) {
		pr_err("fail to get ch %d correct status\n", ch->ch_id);
		ret = -EFAULT;
		goto exit;
	}

	if (index == CAM_CH_CAP && module->grp->is_mul_buf_share)
		goto mul_share_buf_done;

	/* set shared frame for dcam output */
	while (1) {
		struct camera_frame *pframe = NULL;

		pframe = cam_queue_dequeue(&ch->share_buf_queue, struct camera_frame, list);
		if (pframe == NULL)
			break;
		if (module->cam_uinfo.is_4in1 && index == CAM_CH_CAP) {
			dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				ch->aux_dcam_path_id, pframe);
		} else
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF, ch->dcam_path_id, pframe);

		if (ret) {
			pr_err("fail to config dcam output buffer. cur_sw_ctx_id:%d\n", module->cur_sw_ctx_id);
			cam_queue_enqueue(&ch->share_buf_queue, &pframe->list);
			ret = -EINVAL;
			goto exit;
		}
	}

mul_share_buf_done:
	isp_ctx_id = ch->isp_ctx_id;
	isp_path_id = ch->isp_path_id;
	for (j = 0; j < ISP_NR3_BUF_NUM; j++) {
		if (ch->nr3_bufs[j] == NULL)
			continue;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_3DNR_BUF,
				isp_ctx_id, isp_path_id, ch->nr3_bufs[j]);
		if (ret) {
			pr_err("fail to config isp 3DNR buffer\n");
			goto exit;
		}
	}

	if (hw->ip_dcam[DCAM_ID_0]->superzoom_support) {
		if (ch->postproc_buf) {
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_POSTPROC_BUF,
				isp_ctx_id, isp_path_id, ch->postproc_buf);
			if (ret) {
				pr_err("fail to config isp superzoom buffer sw %d,  path id %d\n",
					isp_ctx_id, isp_path_id);
				goto exit;
			}
		}
	}

	if ((module->cam_uinfo.is_pyr_rec && ch->ch_id != CAM_CH_CAP)
		|| (module->cam_uinfo.is_pyr_dec && ch->ch_id == CAM_CH_CAP)) {
		if (ch->pyr_rec_buf) {
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PYR_REC_BUF,
				isp_ctx_id, isp_path_id, ch->pyr_rec_buf);
			if (ret) {
				pr_err("fail to config isp pyr_rec buffer sw %d,  path id %d\n",
					isp_ctx_id, isp_path_id);
				goto exit;
			}
			if (module->cam_uinfo.is_raw_alg && (ch->ch_id == CAM_CH_CAP)) {
				frame = cam_queue_empty_frame_get();
				if (frame) {
					memcpy(frame, ch->pyr_rec_buf, sizeof(struct camera_frame));
					ch->pyr_rec_buf_alg = frame;
					module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
						ISP_PATH_CFG_PYR_REC_BUF,
						ch->isp_fdrh_ctx, ch->isp_fdrh_path, ch->pyr_rec_buf_alg);
				}
			}
		}
	}

	if (module->cam_uinfo.is_pyr_dec && ch->ch_id == CAM_CH_CAP) {
		if (ch->pyr_dec_buf) {
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PYR_DEC_BUF,
				isp_ctx_id, isp_path_id, ch->pyr_dec_buf);
			if (ret) {
				pr_err("fail to config isp pyr_dec buffer sw %d, path id %d\n",
					isp_ctx_id, isp_path_id);
				goto exit;
			}
			if (module->cam_uinfo.is_raw_alg) {
				frame = cam_queue_empty_frame_get();
				if (frame) {
					memcpy(frame, ch->pyr_dec_buf, sizeof(struct camera_frame));
					ch->pyr_dec_buf_alg = frame;
					module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
						ISP_PATH_CFG_PYR_DEC_BUF,
						ch->isp_fdrh_ctx, ch->isp_fdrh_path, ch->pyr_dec_buf_alg);
				}
			}
		}
	}
exit:
	ch->alloc_start = 0;
	return ret;
}

static int camcore_buffer_ltm_cfg(struct camera_module *module,
	uint32_t index)
{
	int ret = 0;
	uint32_t j, isp_ctx_id, isp_path_id;
	struct channel_context *ch = NULL;
	struct channel_context *ch_pre = NULL;
	struct channel_context *ch_cap = NULL;

	if (!module) {
		pr_err("fail to get input ptr\n");
		return -EFAULT;
	}

	ch_pre = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	/*non-zsl capture, or video path while preview path enable, do nothing*/
	if ((!ch_pre->enable && ch_cap->enable) || (index == CAM_CH_VID && ch_pre->enable))
		return 0;

	ch = &module->channel[index];
	isp_ctx_id = ch->isp_ctx_id;
	isp_path_id = ch->isp_path_id;

	if (module->cam_uinfo.is_rgb_ltm) {
		for (j = 0; j < ISP_LTM_BUF_NUM; j++) {
			ch->ltm_bufs[LTM_RGB][j] = ch_pre->ltm_bufs[LTM_RGB][j];
			if (ch->ltm_bufs[LTM_RGB][j] == NULL) {
				pr_err("fail to get rgb_buf ch->ltm_bufs[%d][%d] NULL, index : %x\n", LTM_RGB, j, index);
				goto exit;
			}
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
					ISP_PATH_CFG_RGB_LTM_BUF,
					isp_ctx_id, isp_path_id,
					ch->ltm_bufs[LTM_RGB][j]);
			if (ret) {
				pr_err("fail to config isp rgb LTM buffer\n");
				goto exit;
			}
		}
	}

	if (module->cam_uinfo.is_yuv_ltm) {
		for (j = 0; j < ISP_LTM_BUF_NUM; j++) {
			ch->ltm_bufs[LTM_YUV][j] = ch_pre->ltm_bufs[LTM_YUV][j];
			if (ch->ltm_bufs[LTM_YUV][j] == NULL) {
				pr_err("fail to get yuv_buf ch->ltm_bufs[%d][%d] NULL, index : %x\n", LTM_YUV, j, index);
				goto exit;
			}
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
					ISP_PATH_CFG_YUV_LTM_BUF,
					isp_ctx_id, isp_path_id,
					ch->ltm_bufs[LTM_YUV][j]);
			if (ret) {
				pr_err("fail to config isp yuv LTM buffer\n");
				goto exit;
			}
		}
	}
exit:
	return ret;
}

static int camcore_buffers_alloc_num(struct channel_context *channel,
		struct camera_module *module)
{
	int num = 5;

	if (channel->ch_id == CAM_CH_CAP) {
		channel->zsl_skip_num = module->cam_uinfo.zsk_skip_num;
		channel->zsl_buffer_num = module->cam_uinfo.zsl_num;
		num += channel->zsl_buffer_num;
		if (module->cam_uinfo.is_pyr_dec)
			num += 1;
	}

	if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.is_dual)
		num = 7;

	if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.is_dual && module->master_flag) /*for dual hdr*/
		num += 3;
	if (channel->ch_id == CAM_CH_CAP && !module->cam_uinfo.is_dual && !channel->zsl_buffer_num)
		num += 3;

	/* 4in1 non-zsl capture for single frame */
	if ((module->cam_uinfo.is_4in1 || module->cam_uinfo.dcam_slice_mode)
		&& channel->ch_id == CAM_CH_CAP &&
		module->channel[CAM_CH_PRE].enable == 0 &&
		module->channel[CAM_CH_VID].enable == 0)
		num = 1;

	if (module->dump_thrd.thread_task)
		num += 3;

	/* extend buffer queue for slow motion */
	if (channel->ch_uinfo.is_high_fps)
		num = CAM_SHARED_BUF_NUM;

	if (channel->ch_id == CAM_CH_PRE &&
		module->grp->camsec_cfg.camsec_mode != SEC_UNABLE) {
		num = 4;
	}

	return num;
}

static inline int camcore_mulsharebuf_verif(struct channel_context *ch, struct camera_uinfo *info)
{
	return ch->ch_id == CAM_CH_CAP && info->need_share_buf && !info->dcam_slice_mode && !info->is_4in1;
}

static int camcore_buffers_alloc(void *param)
{
	int ret = 0;
	int path_id = 0;
	int i, count, total, iommu_enable;
	uint32_t width = 0, height = 0, size = 0, pack_bits = 0, dcam_out_bits = 0, pitch = 0;
	uint32_t postproc_w = 0, postproc_h = 0;
	uint32_t is_super_size = 0, sec_mode = 0, is_pack = 0;
	struct camera_module *module;
	struct camera_frame *pframe;
	struct channel_context *channel = NULL;
	struct channel_context *channel_vid = NULL;
	struct camera_debugger *debugger;
	struct cam_hw_info *hw = NULL;
	struct camera_frame *alloc_buf = NULL;
	struct dcam_compress_info fbc_info;
	struct dcam_compress_cal_para cal_fbc = {0};
	struct dcam_sw_context *sw_ctx = NULL;
	struct camera_group *grp = NULL;
	struct camera_queue *cap_buf_q = NULL;
	struct camera_frame *pframe_dec = NULL;
	struct camera_frame *pframe_rec = NULL;
	pr_info("enter.\n");

	module = (struct camera_module *)param;
	grp = module->grp;
	sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	alloc_buf = cam_queue_dequeue(&module->alloc_queue,
		struct camera_frame, list);

	if (alloc_buf) {
		channel = (struct channel_context *)alloc_buf->priv_data;
		cam_queue_empty_frame_put(alloc_buf);
	} else {
		pr_err("fail to dequeue alloc_buf\n");
		return -1;
	}
	if (channel->ch_id == CAM_CH_CAP && g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN) {
		complete(&channel->alloc_com);
		return 0;
	}
	hw = module->grp->hw_info;
	iommu_enable = module->iommu_enable;
	channel_vid = &module->channel[CAM_CH_VID];
	sec_mode = module->grp->camsec_cfg.camsec_mode;

	total = camcore_buffers_alloc_num(channel, module);
	if (camcore_mulsharebuf_verif(channel, &module->cam_uinfo)) {
		cap_buf_q = &grp->mul_share_buf_q;
		width = grp->mul_sn_max_size.w;
		height = grp->mul_sn_max_size.h;
		grp->is_mul_buf_share = 1;
	} else {
		cap_buf_q = &channel->share_buf_queue;
		width = channel->swap_size.w;
		height = channel->swap_size.h;
	}

	dcam_out_bits = channel->ch_uinfo.dcam_output_bit;
	if (channel->aux_dcam_path_id >= 0)
		pack_bits = channel->ch_uinfo.sensor_raw_fmt;
	else
		pack_bits = channel->ch_uinfo.dcam_raw_fmt;
	if ((channel->ch_id == CAM_CH_CAP) && module->cam_uinfo.is_4in1)
		pack_bits = channel->ch_uinfo.dcam_raw_fmt;
	is_pack = 0;
	if ((channel->dcam_out_fmt & DCAM_STORE_RAW_BASE) && (pack_bits == DCAM_RAW_PACK_10))
		is_pack = 1;
	if ((channel->dcam_out_fmt & DCAM_STORE_YUV_BASE) && (dcam_out_bits == DCAM_STORE_10_BIT))
		is_pack = 1;

	if (channel->compress_input) {
		cal_fbc.compress_4bit_bypass = channel->compress_4bit_bypass;
		cal_fbc.data_bits = dcam_out_bits;
		cal_fbc.fbc_info = &fbc_info;
		cal_fbc.fmt = channel->dcam_out_fmt;
		cal_fbc.height = height;
		cal_fbc.width = width;
		size = dcam_if_cal_compressed_size (&cal_fbc);
		pr_info("dcam fbc buffer size %u\n", size);
	} else if (channel->dcam_out_fmt & DCAM_STORE_RAW_BASE) {
		size = cal_sprd_raw_pitch(width, pack_bits) * height;
	} else if ((channel->dcam_out_fmt == DCAM_STORE_YUV420) || (channel->dcam_out_fmt == DCAM_STORE_YVU420)) {
		pitch = cal_sprd_yuv_pitch(width, dcam_out_bits, is_pack);
		size = pitch * height * 3 / 2;
		pr_info("ch%d, dcam yuv size %d\n", channel->ch_id, size);
	} else {
		size = width * height * 3;
	}

	if (module->cam_uinfo.is_pyr_rec && channel->ch_id != CAM_CH_CAP)
		size += dcam_if_cal_pyramid_size(width, height, channel->ch_uinfo.pyr_data_bits, channel->ch_uinfo.pyr_is_pack, 1, DCAM_PYR_DEC_LAYER_NUM);
	size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
	pr_info("cam%d, ch_id %d, camsec=%d, buffer size: %u (%u x %u), num %d\n",
		module->idx, channel->ch_id, sec_mode,
		size, width, height, total);

	if (((channel->ch_id == CAM_CH_CAP) && (module->cam_uinfo.param_frame_sync || module->cam_uinfo.raw_alg_type != RAW_ALG_AI_SFNR) &&
		module->cam_uinfo.is_raw_alg) || (channel->ch_id == CAM_CH_CAP && grp->is_mul_buf_share
		&& atomic_inc_return(&grp->mul_buf_alloced) > 1))
		goto mul_alloc_end;

	for (i = 0, count = 0; i < total; i++) {
		do {
			pframe = cam_queue_empty_frame_get();
			pframe->channel_id = channel->ch_id;
			pframe->is_compressed = channel->compress_input;
			pframe->compress_4bit_bypass = channel->compress_4bit_bypass;
			pframe->width = width;
			pframe->height = height;
			pframe->endian = ENDIAN_LITTLE;
			pframe->pattern = module->cam_uinfo.sensor_if.img_ptn;
			pframe->fbc_info = fbc_info;
			pframe->need_ltm_hist = module->cam_uinfo.is_rgb_ltm;
			pframe->need_ltm_map = module->cam_uinfo.is_rgb_ltm;
			pframe->need_gtm_hist = module->cam_uinfo.is_rgb_gtm;
			pframe->need_gtm_map = module->cam_uinfo.is_rgb_gtm;
			pframe->gtm_mod_en = module->cam_uinfo.is_rgb_gtm;
			if (channel->ch_id == CAM_CH_PRE && sec_mode != SEC_UNABLE)
				pframe->buf.buf_sec = 1;
			if (module->cam_uinfo.is_pyr_rec && channel->ch_id != CAM_CH_CAP)
				pframe->pyr_status = ONLINE_DEC_ON;
			if (module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP)
				pframe->pyr_status = OFFLINE_DEC_ON;

			ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
			if (ret) {
				pr_err("fail to alloc buf: %d ch %d\n", i, channel->ch_id);
				cam_queue_empty_frame_put(pframe);
				atomic_inc(&channel->err_status);
				goto exit;
			}

			ret = cam_queue_enqueue(cap_buf_q, &pframe->list);
			if (ret) {
				pr_err("fail to enqueue shared buf: %d ch %d\n", i, channel->ch_id);
				cam_buf_free(&pframe->buf);
				cam_queue_empty_frame_put(pframe);
				break;
			} else {
				count++;
				pr_debug("frame %p,idx %d,cnt %d,phy_addr %p\n",
					pframe, i, count, (void *)pframe->buf.addr_vir[0]);
				break;
			}
		} while (1);
	}

mul_alloc_end:
	debugger = &module->grp->debugger;
	path_id = channel->dcam_path_id;
	is_super_size = (module->cam_uinfo.dcam_slice_mode == CAM_OFFLINE_SLICE_HW
		&& width >= DCAM_HW_SLICE_WIDTH_MAX) ? 1 : 0;

	if (hw->ip_dcam[DCAM_ID_0]->superzoom_support && !is_super_size) {
		postproc_w = channel->ch_uinfo.dst_size.w / ISP_SCALER_UP_MAX;
		postproc_h = channel->ch_uinfo.dst_size.h / ISP_SCALER_UP_MAX;
		if (channel->ch_id != CAM_CH_CAP && channel_vid->enable) {
			postproc_w = MAX(channel->ch_uinfo.dst_size.w,
				channel_vid->ch_uinfo.dst_size.w) / ISP_SCALER_UP_MAX;
			postproc_h = MAX(channel->ch_uinfo.dst_size.h,
				channel_vid->ch_uinfo.dst_size.h) / ISP_SCALER_UP_MAX;
		}

		size = ((postproc_w + 1) & (~1)) * postproc_h * 3 / 2;
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		pframe = cam_queue_empty_frame_get();
		if (!pframe) {
			pr_err("fail to superzoom no empty frame.\n");
			ret = -EINVAL;
			goto exit;
		}

		pframe->channel_id = channel->ch_id;
		if (sec_mode != SEC_UNABLE)
			pframe->buf.buf_sec = 1;

		ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
		if (ret) {
			pr_err("fail to alloc superzoom buf\n");
			cam_queue_empty_frame_put(pframe);
			atomic_inc(&channel->err_status);
			goto exit;
		}

		channel->postproc_buf = pframe;
		pr_info("hw_ctx_id %d, superzoom w %d, h %d, buf %p\n",
			sw_ctx->hw_ctx_id, postproc_w, postproc_h, pframe);
	}

	pr_debug("channel->ch_id = %d, channel->type_3dnr = %d, channel->uinfo_3dnr = %d\n",
		channel->ch_id, channel->type_3dnr, channel->uinfo_3dnr);
	if ((channel->type_3dnr == CAM_3DNR_HW) &&
		(!((channel->uinfo_3dnr == 0) && (channel->ch_id == CAM_CH_PRE)))) {
		/* YUV420 for 3DNR ref*/
		if (channel->compress_3dnr)
			size = isp_3dnr_cal_compressed_size(width, height);
		else {
			if ((channel->dcam_out_fmt == DCAM_STORE_YUV420)
				|| (channel->dcam_out_fmt == DCAM_STORE_YVU420)) {
				size = ((width + 1) & (~1)) * height * 3;
				size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
			} else {
				size = ((width + 1) & (~1)) * height * 3 / 2;
				size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
			}
		}

		pr_info("ch %d 3dnr buffer size: %u.\n", channel->ch_id, size);
		for (i = 0; i < ISP_NR3_BUF_NUM; i++) {
			pframe = cam_queue_empty_frame_get();

			if (channel->ch_id == CAM_CH_PRE && sec_mode != SEC_UNABLE)
				pframe->buf.buf_sec = 1;

			ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
			if (ret) {
				pr_err("fail to alloc 3dnr buf: %d ch %d\n", i, channel->ch_id);
				cam_queue_empty_frame_put(pframe);
				atomic_inc(&channel->err_status);
				goto exit;
			}
			channel->nr3_bufs[i] = pframe;
		}
	}

	if (channel->mode_ltm != MODE_LTM_OFF) {
		/* todo: ltm buffer size needs to be refined.*/
		/* size = ((width + 1) & (~1)) * height * 3 / 2; */
		/*
		 * sizeof histo from 1 tile: 128 * 16 bit
		 * MAX tile num: 8 * 8
		 */
		size = 64 * 128 * 2;
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);

		pr_info("ch %d, ltm_rgb %d, ltm buffer size: %u.\n", channel->ch_id, channel->ltm_rgb, size);
		if (channel->ltm_rgb) {
			for (i = 0; i < ISP_LTM_BUF_NUM; i++) {
				if (channel->ch_id == CAM_CH_PRE) {
					pframe = cam_queue_empty_frame_get();

					if (channel->ch_id == CAM_CH_PRE
						&& sec_mode == SEC_TIME_PRIORITY)
						pframe->buf.buf_sec = 1;
					ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
					if (ret) {
						pr_err("fail to alloc ltm buf: %d ch %d\n", i, channel->ch_id);
						cam_queue_empty_frame_put(pframe);
						atomic_inc(&channel->err_status);
						goto exit;
					}
					channel->ltm_bufs[LTM_RGB][i] = pframe;
				}
			}
		}

		if (channel->ltm_yuv) {
			for (i = 0; i < ISP_LTM_BUF_NUM; i++) {
				if (channel->ch_id == CAM_CH_PRE) {
					pframe = cam_queue_empty_frame_get();

					if (channel->ch_id == CAM_CH_PRE
						&& sec_mode == SEC_TIME_PRIORITY)
						pframe->buf.buf_sec = 1;
					ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
					if (ret) {
						pr_err("fail to alloc ltm buf: %d ch %d\n",
							i, channel->ch_id);
						cam_queue_empty_frame_put(pframe);
						atomic_inc(&channel->err_status);
						goto exit;
					}
					channel->ltm_bufs[LTM_YUV][i] = pframe;
				}
			}
		}
	}

	if (channel->ch_id == CAM_CH_CAP && grp->is_mul_buf_share && (module->cam_uinfo.is_pyr_rec || module->cam_uinfo.is_pyr_dec))
		mutex_lock(&grp->pyr_mulshare_lock);

	if ((channel->ch_id == CAM_CH_CAP && grp->is_mul_buf_share
		&& (module->cam_uinfo.is_pyr_rec || module->cam_uinfo.is_pyr_dec)
		&& atomic_inc_return(&grp->mul_pyr_buf_alloced) > 1)) {
		pframe_rec = cam_queue_empty_frame_get();
		memcpy(pframe_rec, grp->mul_share_pyr_rec_buf, sizeof(struct camera_frame));
		channel->pyr_rec_buf = pframe_rec;
		pframe_dec = cam_queue_empty_frame_get();
		memcpy(pframe_dec, grp->mul_share_pyr_dec_buf, sizeof(struct camera_frame));
		channel->pyr_dec_buf = pframe_dec;
		goto mul_pyr_alloc_end;
	}

	if ((module->cam_uinfo.is_pyr_rec && channel->ch_id != CAM_CH_CAP)
		|| (module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP)) {
		width = isp_rec_layer0_width(width, channel->pyr_layer_num);
		height = isp_rec_layer0_heigh(height, channel->pyr_layer_num);
		/* rec temp buf max size is equal to layer1 size: w/2 * h/2 */
		size = dcam_if_cal_pyramid_size(width, height, channel->ch_uinfo.pyr_data_bits, channel->ch_uinfo.pyr_is_pack, 1, channel->pyr_layer_num - 1);
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		pframe = cam_queue_empty_frame_get();
		if (channel->ch_id == CAM_CH_PRE && sec_mode == SEC_TIME_PRIORITY)
			pframe->buf.buf_sec = 1;
		pframe->width = width;
		pframe->height = height;
		pframe->channel_id = channel->ch_id;
		ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
		if (ret) {
			pr_err("fail to alloc rec buf\n");
			cam_queue_empty_frame_put(pframe);
			atomic_inc(&channel->err_status);
			goto exit;
		}
		if (camcore_mulsharebuf_verif(channel, &module->cam_uinfo)) {
			grp->mul_share_pyr_rec_buf = pframe;
			pframe_rec = cam_queue_empty_frame_get();
			memcpy(pframe_rec, grp->mul_share_pyr_rec_buf, sizeof(struct camera_frame));
			channel->pyr_rec_buf = pframe_rec;
		} else
			channel->pyr_rec_buf = pframe;

		pr_debug("hw_ctx_id %d, pyr_rec w %d, h %d, buf %p\n",
				sw_ctx->hw_ctx_id, width, height, pframe);
	}

	if (module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP) {
		size = dcam_if_cal_pyramid_size(width, height, channel->ch_uinfo.pyr_data_bits, channel->ch_uinfo.pyr_is_pack, 0, ISP_PYR_DEC_LAYER_NUM);
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		pframe = cam_queue_empty_frame_get();
		if (channel->ch_id == CAM_CH_PRE && sec_mode == SEC_TIME_PRIORITY)
			pframe->buf.buf_sec = 1;
		pframe->width = width;
		pframe->height = height;
		pframe->channel_id = channel->ch_id;
		pframe->data_src_dec = 1;
		pframe->is_compressed = 0;
		ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
		if (ret) {
			pr_err("fail to alloc dec buf\n");
			cam_queue_empty_frame_put(pframe);
			atomic_inc(&channel->err_status);
			goto exit;
		}
		if (camcore_mulsharebuf_verif(channel, &module->cam_uinfo)) {
			grp->mul_share_pyr_dec_buf = pframe;
			pframe_dec = cam_queue_empty_frame_get();
			memcpy(pframe_dec, grp->mul_share_pyr_dec_buf, sizeof(struct camera_frame));
			channel->pyr_dec_buf = pframe_dec;
		} else
			channel->pyr_dec_buf = pframe;

		pr_debug("hw_ctx_id %d, ch %d pyr_dec size %d, buf %p, w %d h %d, pack:%d\n",
			sw_ctx->hw_ctx_id, channel->ch_id, size, pframe, width, height, channel->ch_uinfo.dcam_out_pack);
	}
mul_pyr_alloc_end:
	if (channel->ch_id == CAM_CH_CAP && grp->is_mul_buf_share && (module->cam_uinfo.is_pyr_rec || module->cam_uinfo.is_pyr_dec))
		mutex_unlock(&grp->pyr_mulshare_lock);

exit:
	if (module->channel[CAM_CH_PRE].enable &&
		channel->ch_id != CAM_CH_PRE &&
		channel->ch_id != CAM_CH_VID) {
		ret = camcore_buffer_path_cfg(module, channel->ch_id);
		if (ret)
			pr_err("fail to cfg path buffer\n");
	}
	complete(&channel->alloc_com);
	pr_info("ch %d done. status %d\n", channel->ch_id, atomic_read(&channel->err_status));
	ret = cam_buf_mdbg_check();
	return ret;
}

/* frame to fifo queue for dual camera
 * return: NULL: only input, no output
 *         frame: fifo, set to path->out_buf_queue
 */
static struct camera_frame *camcore_dual_fifo_queue(struct camera_module *module,
		struct camera_frame *pframe,
		struct channel_context *channel)
{
	int ret;

	/* zsl, save frames to fifo buffer */
	ret = cam_queue_enqueue(&module->zsl_fifo_queue, &pframe->list);
	if (ret)
		return pframe;

	if (camcore_outbuf_queue_cnt_get(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
		channel->dcam_path_id) < 3) {
		/* do fifo */
		pframe = cam_queue_dequeue(&module->zsl_fifo_queue,
			struct camera_frame, list);
		if (pframe)
			return pframe;
	}

	return NULL;
}

static int camcore_dual_same_frame_get(struct camera_module *module, struct camera_frame *mframe)
{
	struct camera_group *grp = NULL;
	struct camera_module *pmd[CAM_COUNT] = {0};
	struct camera_queue *q = NULL;
	struct camera_frame *pframe = NULL;
	struct camera_module *slave_module = NULL;
	int i = 0, j = 0;
	int ret = 0;
	int64_t t_sec = 0, t_usec = 0;

	grp = module->grp;
	if (!grp)
		return -EFAULT;
	/* get the two module */
	for (i = 0, j = 0; i < CAM_COUNT; i++) {
		pmd[j] = grp->module[i];
		if (!pmd[j])
			continue;
		if (pmd[j] != module) {
			slave_module = pmd[j];
			break;
		}
	}

	q = &(slave_module->zsl_fifo_queue);
	t_sec = mframe->sensor_time.tv_sec;
	t_usec = mframe->sensor_time.tv_usec;
	ret = cam_queue_same_frame_get(q, &pframe, t_sec, t_usec);
	if (ret) {
		pr_debug("No find match frame\n");
		atomic_set(&slave_module->cap_flag, 1);
		slave_module->dual_frame = NULL;
		return 0;
	}
	slave_module->dual_frame = pframe;
	atomic_set(&slave_module->cap_flag, 1);

	return 0;
}
/*
 * return: 0: pframe to isp
 *         1: no need more deal
 */
static struct camera_frame *camcore_dual_frame_deal(struct camera_module *module,
		struct camera_frame *pframe,
		struct channel_context *channel)
{
	int ret = 0;
	struct camera_frame *pframe_pre = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	channel = &module->channel[pframe->channel_id];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	/* get the same frame */
	ret = camcore_dual_same_frame_get(module , pframe);
	if (!ret) {
		while (1) {
			pframe_pre = cam_queue_dequeue(&module->zsl_fifo_queue,
				struct camera_frame, list);
			if (!pframe_pre)
				break;
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe_pre);
		}
	}

	return pframe;
}

static struct camera_frame *camcore_4in1_frame_deal(struct camera_module *module,
		struct camera_frame *pframe,
		struct channel_context *channel)
{
	int ret;
	uint32_t shutoff = 0;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_hw_path_stop patharg;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	/* 1: aux dcam bin tx done, set frame to isp
	 * 2: lowlux capture, dcam0 full path done, set frame to isp
	 */
	if (pframe->irq_type != CAMERA_IRQ_4IN1_DONE) {
		/* offline timestamp, check time
		 * recove this time:190415
		 *
		 * if (pframe->sensor_time.tv_sec == 0 &&
		 *	pframe->sensor_time.tv_usec == 0)
		 */
		{
			timespec cur_ts;

			memset(&cur_ts, 0, sizeof(timespec));
			pframe->boot_sensor_time = ktime_get_boottime();
			ktime_get_ts(&cur_ts);
			pframe->sensor_time.tv_sec = cur_ts.tv_sec;
			pframe->sensor_time.tv_usec = cur_ts.tv_nsec / NSEC_PER_USEC;
		}

		return pframe;
	}

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	/* dcam0 full tx done, frame report to HAL or drop */
	if (atomic_read(&module->capture_frames_dcam) > 0) {
		/* 4in1 send buf to hal for remosaic */
		atomic_dec(&module->capture_frames_dcam);
		pframe->evt = IMG_TX_DONE;
		pframe->channel_id = CAM_CH_RAW;
		ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
		complete(&module->frm_com);
		pr_info("raw frame[%d] fd %d, size[%d %d], 0x%x\n",
			pframe->fid, pframe->buf.mfd[0], pframe->width,
			pframe->height, (uint32_t)pframe->buf.addr_vir[0]);

		/*stop full path & cap eb*/
		shutoff = 1;
		dev = module->dcam_dev_handle;
		patharg.path_id = channel->dcam_path_id;
		patharg.idx = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;
		dev->hw->dcam_ioctl(dev->hw, DCAM_HW_CFG_PATH_STOP, &patharg);
		dev->hw->dcam_ioctl(dev->hw, DCAM_HW_CFG_STOP_CAP_EB, &patharg.idx);
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF, channel->dcam_path_id, &shutoff);

		return NULL;
	}
	/* set buffer back to dcam0 full path, to out_buf_queue */
	channel = &module->channel[pframe->channel_id];
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
	if (unlikely(ret))
		pr_err("fail to set output buffer, ret %d\n", ret);

	return NULL;
}

/* 4in1_raw_capture
 * full path: sensor raw(4cell), bin path: 4in1 bin sum
 * two image save in one fd(one buffer), full + bin
 */
static struct camera_frame *camcore_4in1_raw_capture_deal(struct camera_module *module,
		struct camera_frame *pframe)
{
	static uint32_t flag_path;/* b0:bin tx done, b1:full tx done */

	/* full path tx done */
	if (pframe->irq_type == CAMERA_IRQ_4IN1_DONE) {
		flag_path |= BIT(1);
	} else { /* bin path tx done */
		flag_path |= BIT(0);
	}
	/* check bin, full both tx done */
	if ((flag_path & 0x2) == 0x2) {
		pframe->evt = IMG_TX_DONE;
		pframe->irq_type = CAMERA_IRQ_4IN1_DONE;
		flag_path = 0;
		return pframe;
	}
	/* not report */
	cam_buf_ionbuf_put(&pframe->buf);
	cam_queue_empty_frame_put(pframe);

	return NULL;
}

static int camcore_4in1_aux_init(struct camera_module *module,
		struct channel_context *channel)
{
	int ret = 0;
	uint32_t dcam_path_id;
	struct dcam_path_cfg_param ch_desc;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_pipe_dev *dcam = NULL;
	struct camera_group *grp = module->grp;
	uint32_t dcam_idx = DCAM_HW_CONTEXT_0;

	/* todo: will update after dcam offline ctx done. */
	dcam_path_id = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path;
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	dcam_sw_ctx->dcam_slice_mode = module->cam_uinfo.dcam_slice_mode;

	dcam = dcam_core_pipe_dev_get(grp->hw_info);
	if (IS_ERR_OR_NULL(dcam)) {
		pr_err("fail to get dcam\n");
		return -EFAULT;
	}
	module->aux_dcam_dev = dcam;

	for (; dcam_idx < DCAM_HW_CONTEXT_MAX; dcam_idx++) {
		if (dcam_idx != module->dcam_idx) {
			module->aux_dcam_id = dcam_idx;
			break;
		}
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->open(dcam);
	if (ret < 0) {
		pr_err("fail to open aux dcam dev\n");
		ret = -EFAULT;
		goto open_fail;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_sw_ctx, dcam_path_id);
	if (ret < 0) {
		pr_err("fail to get dcam path %d\n", dcam_path_id);
		ret = -EFAULT;
		goto get_path_fail;
	}

	channel->aux_dcam_path_id = dcam_path_id;
	pr_info("get aux dcam path %d\n", dcam_path_id);
	dcam_sw_ctx->fetch.fmt = DCAM_STORE_RAW_BASE;
	dcam_sw_ctx->pack_bits = DCAM_RAW_14;

	/* cfg dcam1 bin path */
	memset(&ch_desc, 0, sizeof(ch_desc));
	if (channel->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10 && channel->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX)
		ch_desc.raw_fmt = channel->ch_uinfo.dcam_raw_fmt;
	else {
		ch_desc.raw_fmt = dcam->hw->ip_dcam[0]->raw_fmt_support[0];
		if (dcam->hw->ip_dcam[0]->save_band_for_bigsize)
			ch_desc.raw_fmt = DCAM_RAW_PACK_10;
		channel->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
	}

	ch_desc.endian.y_endian = ENDIAN_LITTLE;
	ch_desc.input_size.w = channel->ch_uinfo.src_size.w;
	ch_desc.input_size.h = channel->ch_uinfo.src_size.h;
	/* dcam1 not trim, do it by isp */
	ch_desc.input_trim.size_x = channel->ch_uinfo.src_size.w;
	ch_desc.input_trim.size_y = channel->ch_uinfo.src_size.h;
	ch_desc.output_size.w = ch_desc.input_trim.size_x;
	ch_desc.output_size.h = ch_desc.input_trim.size_y;
	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;

	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_BASE, channel->aux_dcam_path_id, &ch_desc);
	if (ret) {
		pr_err("fail to cfg path base aux_dcam_path_id %d\n", channel->aux_dcam_path_id);
		goto get_path_fail;
	}
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SIZE, channel->aux_dcam_path_id, &ch_desc);
	if (ret) {
		pr_err("fail to cfg path size aux_dcam_path_id %d\n", channel->aux_dcam_path_id);
		goto get_path_fail;
	}
	/* 4in1 not choose 1 from 3 frames, TODO
	 * channel->frm_cnt = (uint32_t)(-3);
	 */
	pr_info("done\n");

	return ret;
get_path_fail:
	module->dcam_dev_handle->dcam_pipe_ops->close(module->aux_dcam_dev);
open_fail:
	dcam_core_pipe_dev_put(module->aux_dcam_dev);
	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;
	return ret;
}

static int camcore_4in1_aux_deinit(struct camera_module *module)
{
	int ret = 0;
	struct dcam_pipe_dev *dcam = NULL;
	uint32_t dcam_path_id = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path;
	struct dcam_sw_context *dcam_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	dcam = module->aux_dcam_dev;
	if (dcam == NULL)
		return ret;

	ret = module->dcam_dev_handle->dcam_pipe_ops->stop(dcam_ctx, DCAM_STOP);

	ret = module->dcam_dev_handle->dcam_pipe_ops->put_path(dcam_ctx, dcam_path_id);
	if (ret < 0) {
		pr_err("fail to put dcam path %d, ret = %d\n", dcam_path_id, ret);
		goto exit;
	}
	module->dcam_dev_handle->dcam_pipe_ops->close(module->aux_dcam_dev);
	dcam_core_pipe_dev_put(module->aux_dcam_dev);
	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;

	pr_info("Done\n");
exit:
	return ret;
}

/* 4in1_raw_capture
 * init second path for bin sum
 */
static int camcore_4in1_secondary_path_init(
	struct camera_module *module, struct channel_context *ch)
{
	int ret = 0;
	uint32_t second_path_id = DCAM_PATH_BIN;
	struct dcam_path_cfg_param ch_desc;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	/* now only raw capture can run to here */
	if (ch->ch_id != CAM_CH_RAW)
		return -EFAULT;

	ch->second_path_enable = 0;
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_ctx->dcam_slice_mode = module->cam_uinfo.dcam_slice_mode;
	ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_sw_ctx, second_path_id);
	if (ret < 0) {
		pr_err("fail to get dcam path %d\n", second_path_id);
		return -EFAULT;
	}
	ch->second_path_id = second_path_id;
	dcam_sw_ctx->fetch.fmt = DCAM_STORE_RAW_BASE;
	dcam_sw_ctx->pack_bits = DCAM_RAW_PACK_10;

	/* todo: cfg param to user setting. */
	memset(&ch_desc, 0, sizeof(ch_desc));
	if (ch->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10 && ch->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX)
		ch_desc.raw_fmt = ch->ch_uinfo.dcam_raw_fmt;
	else {
		ch_desc.raw_fmt = module->dcam_dev_handle->hw->ip_dcam[0]->raw_fmt_support[0];
		if (module->dcam_dev_handle->hw->ip_dcam[0]->save_band_for_bigsize)
			ch_desc.raw_fmt = DCAM_RAW_PACK_10;
		ch->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
	}

	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
	/*
	 * Configure slow motion for BIN path. HAL must set @is_high_fps
	 * and @high_fps_skip_num for both preview channel and video
	 * channel so BIN path can enable slow motion feature correctly.
	 */
	ch_desc.slowmotion_count = ch->ch_uinfo.high_fps_skip_num;
	ch_desc.endian.y_endian = ENDIAN_LITTLE;

	ch_desc.input_size.w = module->cam_uinfo.sn_size.w / 2;
	ch_desc.input_size.h = module->cam_uinfo.sn_size.h / 2;
	ch_desc.input_trim.start_x = 0;
	ch_desc.input_trim.start_y = 0;
	ch_desc.input_trim.size_x = ch_desc.input_size.w;
	ch_desc.input_trim.size_y = ch_desc.input_size.h;
	ch_desc.output_size.w = ch_desc.input_size.w;
	ch_desc.output_size.h = ch_desc.input_size.h;

	if (ch->ch_id == CAM_CH_RAW)
		ch_desc.is_raw = 1;
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_BASE, ch->second_path_id, &ch_desc);
	if (ret) {
		pr_err("fail to cfg path base second_path_id %d\n", ch->second_path_id);
		return -EFAULT;
	}
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SIZE, ch->second_path_id, &ch_desc);
	if (ret) {
		pr_err("fail to cfg path size second_path_id %d\n", ch->second_path_id);
		return -EFAULT;
	}
	/* bypass bin path all sub block except 4in1 */

	ch->second_path_enable = 1;
	pr_info("done\n");

	return 0;
}

/* 4in1_raw_capture
 * deinit second path
 */
static void camcore_4in1_secondary_path_deinit(
	struct camera_module *module, struct channel_context *ch)
{
	struct dcam_sw_context *dev = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	/* now only raw capture can run to here */
	if (ch->ch_id != CAM_CH_RAW || (!ch->second_path_enable))
		return;
	module->dcam_dev_handle->dcam_pipe_ops->put_path(
		dev, ch->second_path_id);
	ch->second_path_enable = 0;
	pr_info("done\n");
}

static uint32_t camcore_frame_start_proc(struct camera_module *module, struct camera_frame *pframe)
{
	struct dcam_sw_context *pctx = NULL;
	int ret = 0;

	if (module->cam_uinfo.virtualsensor)
		pctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	else
		pctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	pframe->priv_data = pctx;
	ret = cam_queue_enqueue(&pctx->in_queue, &pframe->list);
	if (ret == 0)
		complete(&module->dcam_offline_proc_thrd.thread_com);
	else
		pr_err("fail to enqueue frame to dev->in_queue, ret = %d\n", ret);
	return ret;
}

static struct camera_frame *camcore_supersize_frame_deal(struct camera_module *module,
		struct camera_frame *pframe,
		struct channel_context *channel)
{
	int ret;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct dcam_pipe_dev *dev = (struct dcam_pipe_dev *)module->aux_dcam_dev;

	/* 1: aux dcam bin tx done, set frame to isp
	 * 2: lowlux capture, dcam0 full path done, set frame to isp
	 */
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	if (pframe->irq_type == CAMERA_IRQ_SUPERSIZE_DONE) {
		/* dcam0 full tx done, frame send to dcam1 or drop */
		pr_debug("raw frame[%d] fd %d, size[%d %d], addr_vir[0]:0x%x, channel_id %d, catpure_cnt = %d, time %lld\n",
				pframe->fid, pframe->buf.mfd[0], pframe->width,
				pframe->height, (uint32_t)pframe->buf.addr_vir[0], pframe->channel_id,
				atomic_read(&module->capture_frames_dcam), pframe->boot_sensor_time);

		dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
		pr_debug("cur_aux_sw_ctx_id:%d, sw_ctx_id:%d\n", module->offline_cxt_id, dcam_sw_aux_ctx->sw_ctx_id);

		if (module->dcam_cap_status == DCAM_CAPTURE_START_FROM_NEXT_SOF
			&& (module->capture_times < pframe->boot_sensor_time)
			&& atomic_read(&module->capture_frames_dcam) > 0) {
			if (module->cam_uinfo.is_pyr_dec)
				pframe->width = module->channel[CAM_CH_CAP].ch_uinfo.src_crop.w;

			if (dev->hw->ip_dcam[0]->save_band_for_bigsize) {
				uint32_t shutoff = 1;
				struct dcam_hw_path_stop patharg;
				patharg.path_id = channel->dcam_path_id;
				patharg.idx = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;
				dev->hw->dcam_ioctl(dev->hw, DCAM_HW_CFG_PATH_STOP, &patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF, channel->dcam_path_id, &shutoff);
			}

			ret = camcore_frame_start_proc(module, pframe);
			if (ret == 0)
				return NULL;
		} else if (module->dcam_cap_status != DCAM_CAPTURE_START_FROM_NEXT_SOF) {
			if (module->cam_uinfo.is_pyr_dec)
				pframe->width = module->channel[CAM_CH_CAP].ch_uinfo.src_crop.w;

			if (dev->hw->ip_dcam[0]->save_band_for_bigsize) {
				uint32_t shutoff = 1;
				struct dcam_hw_path_stop patharg;
				patharg.path_id = channel->dcam_path_id;
				patharg.idx = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;
				dev->hw->dcam_ioctl(dev->hw, DCAM_HW_CFG_PATH_STOP, &patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF, channel->dcam_path_id, &shutoff);
			}

			ret = camcore_frame_start_proc(module, pframe);
			if (ret == 0)
				return NULL;
		}

		/* set buffer back to dcam0 full path, to out_buf_queue */
		pr_debug("drop frame[%d]\n", pframe->fid);
		channel = &module->channel[pframe->channel_id];

		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path( dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
		if (unlikely(ret))
			pr_err("fail to set output buffer, ret %d\n", ret);

		return NULL;
	} else {
		/*dcam1 deal with supersize finish*/
		timespec cur_ts;

		memset(&cur_ts, 0, sizeof(timespec));
		pframe->boot_sensor_time = ktime_get_boottime();
		ktime_get_ts(&cur_ts);
		pframe->sensor_time.tv_sec = cur_ts.tv_sec;
		pframe->sensor_time.tv_usec = cur_ts.tv_nsec / NSEC_PER_USEC;

		pr_info("[finish] frame[%d] fd %d, size[%d %d], 0x%x, channel_id %d\n",
			pframe->fid, pframe->buf.mfd[0], pframe->width,
			pframe->height, (uint32_t)pframe->buf.addr_vir[0], pframe->channel_id);

		return pframe;
	}
}

static int camcore_share_buf_cfg(enum share_buf_cb_type type,
		void *param, void *priv_data)
{
	int ret = 0;
	struct camera_frame *pframe = NULL;
	struct camera_frame **frame = NULL;
	struct camera_group *grp = NULL;
	struct camera_module *module = NULL;

	if (!priv_data) {
		pr_err("fail to get valid param %p\n", priv_data);
		return -EFAULT;
	}

	module = (struct camera_module *)priv_data;
	grp = module->grp;

	switch (type) {
		case SHARE_BUF_GET_CB:
			frame = (struct camera_frame **)param;
			if (!module->cam_uinfo.dcam_slice_mode && module->cam_uinfo.need_share_buf) {
					*frame = cam_queue_dequeue(&grp->mul_share_buf_q,
						struct camera_frame, list);
					pr_debug("cam %d dcam %d get share buf cnt %d frame %p\n", module->idx,
						module->dcam_idx, grp->mul_share_buf_q.cnt, *frame);
			} else
				*frame = NULL;
			break;
		case SHARE_BUF_SET_CB:
			pframe = (struct camera_frame *)param;
			pframe->not_use_isp_reserved_buf = 0;
			if (pframe->buf.mapping_state & CAM_BUF_MAPPING_DEV)
				cam_buf_iommu_unmap(&pframe->buf);
			if (module->cam_uinfo.need_share_buf) {
				ret = cam_queue_enqueue(&grp->mul_share_buf_q, &pframe->list);
				pr_debug("cam %d dcam %d set share buf cnt %d frame id %d\n", module->idx,
					module->dcam_idx, grp->mul_share_buf_q.cnt, pframe->fid);
			}
			break;
		default:
			pr_err("fail to get invalid %d\n", type);
			break;
	}

	return ret;
}

static int camcore_dump_config(void *priv_data, void *param)
{
	struct cam_dump_ctx *dump_base = NULL;
	struct camera_module *module = NULL;
	struct camera_frame *pframe = NULL;
	struct channel_context *channel = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	uint32_t start_layer = 0;
	uint32_t pdaf_type = 0;
	uint32_t dcam_path_id = 0;

	if (!priv_data || !param) {
		pr_err("fail to get valid param %p\n", priv_data);
		return -EFAULT;
	}
	module = (struct camera_module *)priv_data;
	pframe = (struct camera_frame *)param;
	dump_base = &module->dump_base;
	dump_base->ch_id = pframe->channel_id;
	channel = &module->channel[dump_base->ch_id];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	pdaf_type = dcam_sw_ctx->ctx[dcam_sw_ctx->cur_ctx_id].blk_pm.pdaf.pdaf_type;
	dcam_path_id = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path;

	if (g_dbg_dump.dump_en == DUMP_ISP_PYR_REC)
		start_layer = 1;
	if (dump_base->dump_cfg != NULL){
		if (g_dbg_dump.dump_en == DUMP_DCAM_OFFLINE)
			dump_base->dump_cfg(dump_base, DUMP_CFG_OUT_FMT, &dcam_sw_aux_ctx->path[dcam_path_id].out_fmt);
		else
			dump_base->dump_cfg(dump_base, DUMP_CFG_OUT_FMT, &channel->dcam_out_fmt);
		dump_base->dump_cfg(dump_base, DUMP_CFG_IS_PACK, &channel->ch_uinfo.dcam_out_pack);
		dump_base->dump_cfg(dump_base, DUMP_CFG_OUT_BITS, &channel->ch_uinfo.dcam_output_bit);
		if (g_dbg_dump.dump_en == DUMP_DCAM_PDAF)
			dump_base->dump_cfg(dump_base, DUMP_CFG_PDAF_TYPE, &pdaf_type);
		if (((g_dbg_dump.dump_en > 0 && g_dbg_dump.dump_en < DUMP_PATH_BIN) || g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN)
			&& pframe->need_pyr_rec == 0)
			dump_base->dump_cfg(dump_base, DUMP_CFG_PACK_BITS, &channel->ch_uinfo.dcam_raw_fmt);
		else {
			dump_base->dump_cfg(dump_base, DUMP_CFG_PYR_LAYER_NUM, &channel->pyr_layer_num);
			dump_base->dump_cfg(dump_base, DUMP_CFG_PYR_START_LAYER, &start_layer);
		}
	}
	return 0;
}

static int camcore_isp_callback(enum isp_cb_type type, void *param, void *priv_data)
{
	int ret = 0;
	uint32_t ch_id;
	int32_t isp_ctx_id;
	struct camera_frame *pframe = NULL;
	struct camera_module *module = NULL;
	struct channel_context *channel = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct isp_sw_context *isp_sw_ctx = NULL;

	if (!param || !priv_data) {
		pr_err("fail to get valid param %p %p\n", param, priv_data);
		return -EFAULT;
	}

	module = (struct camera_module *)priv_data;

	if (unlikely(type == ISP_CB_GET_PMBUF)) {
		struct camera_frame **pm_frame;
		if (module->pmq_init == 0)
			return 0;
		pm_frame = (struct camera_frame **)param;
		*pm_frame = cam_queue_dequeue(&module->param_queue, struct camera_frame, list);
		return 0;
	}

	if (unlikely(type == ISP_CB_DEV_ERR)) {
		pr_err("fail to get isp state, camera %d\n", module->idx);
		return 0;
	}

	pframe = (struct camera_frame *)param;
	pframe->priv_data = NULL;
	channel = &module->channel[pframe->channel_id];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	isp_sw_ctx = module->isp_dev_handle->sw_ctx[channel->isp_ctx_id];

	if ((pframe->fid & 0x3F) == 0)
		pr_debug("cam %d, module %p, frame %p, ch %d\n",
			module->idx, module, pframe, pframe->channel_id);

	switch (type) {
	case ISP_CB_RET_SRC_BUF:
		cam_queue_frame_param_unbind(&isp_sw_ctx->param_share_queue, pframe);

		if (pframe->irq_property != CAM_FRAME_COMMON) {
			/* FDR frames from user */
			pr_info("fdr %d src buf %x return.\n", pframe->irq_property, pframe->buf.mfd[0]);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			break;
		}

		if ((atomic_read(&module->state) != CAM_RUNNING) ||
			module->paused || (channel->dcam_path_id < 0)) {
			/* stream off or test_isp_only */
			pr_info("isp ret src frame %p\n", pframe);
			pframe->not_use_isp_reserved_buf = 0;
			cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
		} else if (module->cap_status == CAM_CAPTURE_RAWPROC) {
			if (module->cam_uinfo.dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
				struct channel_context *ch = NULL;

				pr_debug("slice %d %p\n", module->cam_uinfo.slice_count, pframe);
				module->cam_uinfo.slice_count++;
				ch = &module->channel[CAM_CH_CAP];
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF,
						ch->dcam_path_id, pframe);
				if (module->cam_uinfo.slice_count >= module->cam_uinfo.slice_num)
					module->cam_uinfo.slice_count = 0;
				else
					ret = camcore_frame_start_proc(module, pframe);
				return ret;
			}
			/* for case raw capture post-proccessing
			 * just release it, no need to return
			 */
			if (g_dbg_dump.dump_en == DUMP_DCAM_OFFLINE && module->dump_base.dump_enqueue != NULL) {
				ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
				if (ret == 0)
					return 0;
			}
			if (pframe->buf.type == CAM_BUF_USER)
				cam_buf_ionbuf_put(&pframe->buf);
			else
				cam_buf_free(&pframe->buf);
			pr_info("raw proc return mid frame %px\n", pframe);
			cam_queue_empty_frame_put(pframe);
		} else {
			/* return offline buffer to dcam available queue. */
			pr_debug("isp reset dcam path out %d\n", channel->dcam_path_id);
			pframe->need_ltm_hist = module->cam_uinfo.is_rgb_ltm;
			pframe->need_ltm_map = module->cam_uinfo.is_rgb_ltm;
			pframe->need_gtm_hist = module->cam_uinfo.is_rgb_gtm;
			pframe->need_gtm_map = module->cam_uinfo.is_rgb_gtm;
			pframe->gtm_mod_en = module->cam_uinfo.is_rgb_gtm;

			if (g_dbg_dump.dump_en == DUMP_PATH_BIN && module->dump_thrd.thread_task) {
				if (g_dbg_dumpswitch && !module->dump_base.dump_enqueue) {
					camdump_start(&module->dump_thrd, &module->dump_base, module->dcam_idx);
					ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
					if (ret == 0)
						return 0;
				}
				if (!g_dbg_dumpswitch && module->dump_base.dump_enqueue)
					camdump_stop(&module->dump_base);
			}
			if (((g_dbg_dump.dump_en > DUMP_DISABLE && g_dbg_dump.dump_en <= DUMP_ISP_PYR_REC) || g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN
				|| (g_dbg_dump.dump_en == DUMP_DCAM_OFFLINE && channel->ch_id == CAM_CH_CAP))
				&& module->dump_base.dump_enqueue != NULL) {
				if (g_dbg_dump.dump_en == DUMP_ISP_PYR_REC && channel->pyr_rec_buf != NULL) {
					channel->pyr_rec_buf->fid = pframe->fid;
					channel->pyr_rec_buf->width = pframe->width;
					channel->pyr_rec_buf->height = pframe->height;
					module->dump_base.dump_enqueue(&module->dump_base, channel->pyr_rec_buf);
				} else {
					ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
					if (ret == 0)
						return 0;
				}
			}
			if (((channel->aux_dcam_path_id == DCAM_PATH_BIN) || (channel->aux_dcam_path_id == DCAM_PATH_FULL))
				&& (module->cam_uinfo.is_4in1 || module->cam_uinfo.dcam_slice_mode > 0)) {
				if (pframe->buf.type == CAM_BUF_USER) {
					/* 4in1, lowlux capture, use dcam0
					 * full path output buffer, from
					 * SPRD_IMG_IO_SET_4IN1_ADDR, user space
					 */
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id,
						pframe);
				} else {
					/* alloced by kernel, bin path output buffer*/
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF,
							channel->aux_dcam_path_id,
							pframe);
				}
			} else {
				if (module->cam_uinfo.need_dcam_raw && (pframe->channel_id == CAM_CH_CAP
						)) {
					complete(&module->mes_thrd.thread_com);
					ret = cam_queue_enqueue(&module->mes_base.mes_queue, &pframe->list);
					if (ret == 0)
						complete(&module->mes_base.mes_com);
				} else {
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id,
						pframe);
					if (ret) {
						cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
					}
				}
			}
		}
		break;
	case ISP_CB_RET_DST_BUF:
		if (atomic_read(&module->state) == CAM_RUNNING) {
			if (module->cap_status == CAM_CAPTURE_RAWPROC) {
				pr_info("raw proc return dst frame %px\n", pframe);
				cam_buf_ionbuf_put(&pframe->buf);
				module->cap_status = CAM_CAPTURE_RAWPROC_DONE;
				pframe->irq_type = CAMERA_IRQ_DONE;
				pframe->irq_property = IRQ_RAW_PROC_DONE;
			} else {
				pframe->irq_type = CAMERA_IRQ_IMG;
				pframe->priv_data = module;

				/* FDR frame done use specific irq_type */
				if (pframe->irq_property != CAM_FRAME_COMMON) {
					if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1) {
						pframe->irq_type = (pframe->irq_property == CAM_FRAME_FDRL) ?
							CAMERA_IRQ_FDRL : CAMERA_IRQ_FDRH;
						module->fdr_done |= (1 << pframe->irq_type);
						if ((module->fdr_done & (1 << CAMERA_IRQ_FDRL)) &&
							(module->fdr_done & (1 << CAMERA_IRQ_FDRH)))
							complete(&module->cap_thrd.thread_com);
					} else if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) {
						if (pframe->irq_property == CAM_FRAME_FDRH) {
							pframe->irq_type = CAMERA_IRQ_FDR_DRC;
							if (module->cam_uinfo.raw_alg_type == RAW_ALG_MFNR)
								pframe->irq_type = CAMERA_IRQ_IMG;
						} else
							pr_err("fail to get irq_property in FDR2.0:%d.\n", pframe->irq_property);
						if (channel->fdrh_zoom_buf) {
							cam_buf_ionbuf_put(&channel->fdrh_zoom_buf->buf);
							cam_queue_empty_frame_put(channel->fdrh_zoom_buf);
							channel->fdrh_zoom_buf = NULL;
						}
						pr_debug("fdr %d yuv buf %x done %d\n", pframe->irq_property,
							pframe->buf.mfd[0], module->fdr_done);
					} else
						pr_debug("raw alg type:%d.\n", module->cam_uinfo.raw_alg_type);
				}
			}
			pframe->evt = IMG_TX_DONE;
			ch_id = pframe->channel_id;

			if (pframe->channel_id == CAM_CH_CAP && isp_sw_ctx && (atomic_read(&isp_sw_ctx->cap_cnt) != pframe->cap_cnt) &&
				module->dcam_cap_status != DCAM_CAPTURE_START_FROM_NEXT_SOF) {
				ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, ISP_PATH_RETURN_OUTPUT_BUF,
					channel->isp_ctx_id, channel->isp_path_id, pframe);
				break;
			}
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
			if (ret) {
				cam_buf_ionbuf_put(&pframe->buf);
				cam_queue_empty_frame_put(pframe);
			} else {
				complete(&module->frm_com);
				pr_debug("ch %d get out frame: %p, evt %d mfd %x\n",
					ch_id, pframe, pframe->evt, pframe->buf.mfd[0]);
			}
		} else {
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
		}
		break;
	case ISP_CB_RET_PYR_DEC_BUF:
		if (g_dbg_dump.dump_en == DUMP_ISP_PYR_DEC && channel->pyr_dec_buf != NULL
			&& module->dump_base.dump_enqueue != NULL) {
			ret = module->dump_base.dump_enqueue(&module->dump_base, channel->pyr_dec_buf);
			if (ret == 0)
				return 0;
		}
		if (dcam_sw_ctx->is_raw_alg)
			isp_ctx_id = channel->isp_fdrh_ctx;
		else
			isp_ctx_id = channel->isp_ctx_id;
		pr_debug("isp%d pyr dec done need rec %d, need raw:%d\n", isp_ctx_id, pframe->need_pyr_rec, dcam_sw_ctx->is_raw_alg);
		ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe, isp_ctx_id);
		if (ret) {
			module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PYR_DEC_BUF, isp_ctx_id, channel->isp_path_id, pframe);
		}
		break;
	case ISP_CB_STATIS_DONE:
		pframe->evt = IMG_TX_DONE;
		pframe->irq_type = CAMERA_IRQ_STATIS;
		pframe->priv_data = module;
		if (atomic_read(&module->state) == CAM_RUNNING) {
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
			if (ret) {
				cam_queue_empty_frame_put(pframe);
			} else {
				complete(&module->frm_com);
				pr_debug("get statis frame: %p, type %d, %d\n",
					pframe, pframe->irq_type, pframe->irq_property);
			}
		} else {
			cam_queue_empty_frame_put(pframe);
		}
		break;
	default:
		pr_err("fail to get cb cmd: %d\n", type);
		break;
	}

	return ret;
}

static int camcore_rawalg_judge(uint32_t cap_scene)
{
	if (cap_scene == CAPTURE_FDR || cap_scene == CAPTURE_AI_SFNR ||
		cap_scene == CAPTURE_RAWALG)
		return true;
	else
		return false;
}

static int camcore_rawalg_proc(struct camera_module *module, struct channel_context *channel,
		struct camera_frame *pframe, struct cam_hw_info *hw)
{
	struct camera_frame *pframe_pre = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct dcam_hw_path_stop patharg;
	struct dcam_hw_path_restart re_patharg;
	uint32_t ret = 0, shutoff = 0;

	if (!module || !channel || !pframe || !hw) {
		pr_err("fail to get valid param:%px,%px,%px,%px.\n", module, channel, pframe, hw);
		return -EFAULT;
	}

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	if ((channel->share_buf_queue.cnt > 0) && (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2)) {
		/* cap zsl frame is for pre frame cache before start capture*/
		if (atomic_read(&module->cap_zsl_frames) > 0) {
			if (channel->dcam_path_id == DCAM_PATH_RAW) {
				while (1) {
					pframe_pre = cam_queue_dequeue(&channel->share_buf_queue, struct camera_frame, list);
					if (pframe_pre == NULL)
						break;
					if (pframe_pre->boot_sensor_time < module->capture_times ||
						pframe_pre->img_fmt != IMG_PIX_FMT_GREY) {
						pr_debug("cam%d cap cap_time[%lld] sof_time[%lld] fmt %d mfd %x\n",
							module->idx, module->capture_times,
							pframe_pre->boot_sensor_time, pframe_pre->img_fmt, pframe_pre->buf.mfd[0]);
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe_pre);
						continue;
					} else {
						pframe_pre->irq_property = CAM_FRAME_PRE_FDR;
						ret = cam_queue_enqueue(&channel->share_buf_queue, &pframe_pre->list);
						if (ret) {
							pr_err("fail to enqueue frm_queue.\n");
							ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
								DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe_pre);
							return 0;
						}
						atomic_dec(&module->cap_zsl_frames);
					}
				}
			} else if (channel->dcam_path_id == DCAM_PATH_FULL) {
				while (1) {
					pframe_pre = cam_queue_dequeue(&channel->share_buf_queue, struct camera_frame, list);
					if (pframe_pre == NULL)
						break;
					ret = camcore_frame_start_proc(module, pframe_pre);
					if (unlikely(ret))
						pr_err("fail to start dcams for raw proc\n");
				}
			}

			/* when share buf queue frame not match judgement, need grep the frame later*/
			if (atomic_read(&module->cap_zsl_frames) > 0) {
				if (pframe->boot_sensor_time < module->capture_times
					|| pframe->img_fmt != IMG_PIX_FMT_GREY) {
					pr_debug("cam%d cap cap_time[%lld] sof_time[%lld] fmt %d\n",
						module->idx, module->capture_times,
						pframe->boot_sensor_time, pframe->img_fmt);
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id, pframe);
					return ret;
				} else {
					if (channel->dcam_path_id == DCAM_PATH_RAW) {
						ret = cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
						if (ret) {
							pr_err("fail to enqueue frm_queue.\n");
							ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
								DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
							return 0;
						}
						atomic_dec(&module->cap_zsl_frames);
					} else {
						ret = camcore_frame_start_proc(module, pframe);
						if (unlikely(ret))
							pr_err("fail to start dcams for raw proc\n");
					}
				}
			}
			pr_debug("dcam zsl cnt %d\n", atomic_read(&module->cap_zsl_frames));
			return ret;
		} else if ((atomic_read(&module->cap_zsl_frames) == 0) &&
		(atomic_read(&module->capture_frames_dcam) == 0)) {
		/*capture_frames_dcam = cap_all_frames - cap_zsl_frames,
		    when finish grep pre cache frame & cap frame,
		    sent its to offline dcam output bpc raw*/
			shutoff = 1;
			patharg.path_id = channel->aux_dcam_path_id;
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
				DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);

			shutoff = 0;
			re_patharg.path_id = channel->aux_raw_path_id;
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
				DCAM_PATH_CFG_SHUTOFF, re_patharg.path_id, &shutoff);

			shutoff = 1;
			patharg.path_id = DCAM_PATH_RAW;
			patharg.idx = dcam_sw_ctx->hw_ctx_id;
			hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_STOP, &patharg);
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);

			while (1) {
				pframe_pre = cam_queue_dequeue(&channel->share_buf_queue, struct camera_frame, list);
				if (pframe_pre == NULL)
					break;
				pframe_pre->irq_property = CAM_FRAME_PRE_FDR;
				ret = camcore_frame_start_proc(module, pframe_pre);
				if (unlikely(ret))
					pr_err("fail to start dcams for raw proc\n");
			}
		} else if (channel->dcam_path_id == DCAM_PATH_FULL) {
			while (1) {
				pframe_pre = cam_queue_dequeue(&channel->share_buf_queue, struct camera_frame, list);
				if (pframe_pre == NULL)
					break;
				pframe_pre->irq_property = CAM_FRAME_PRE_FDR;
				ret = camcore_frame_start_proc(module, pframe_pre);
				if (unlikely(ret))
					pr_err("fail to start dcams for raw proc\n");
			}
		}
	}

	/*if bpc raw buffer, sent it to hal.*/
	if ((module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) &&
		pframe->bpc_raw_flag) {
		pframe->priv_data = module;
		pframe->evt = IMG_TX_DONE;
		pframe->irq_type = CAMERA_IRQ_PRE_FDR;
		ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
		if (ret) {
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			pr_err("fail to enqueue frm_queue.\n");
		} else {
			complete(&module->frm_com);
			pr_debug("bpc raw to user mfd %x\n", pframe->buf.mfd[0]);
		}
	}

	return ret;
}

static inline bool camcore_capture_sizechoice(struct camera_module *module, struct channel_context *channel)
{
	return module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP &&
			(!module->cam_uinfo.is_raw_alg || (module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR && !module->cam_uinfo.param_frame_sync));
}

static bool camcore_dcam_capture_skip_condition(struct camera_frame *pframe, struct channel_context *channel, struct camera_module *module)
{
	if (channel->ch_id != CAM_CH_CAP)
		return false;
	if(!module->cam_uinfo.is_4in1 && !module->cam_uinfo.dcam_slice_mode && (module->cap_status != CAM_CAPTURE_RAWPROC) && (pframe->fid < 1))
		return true;
	if (camcore_capture_sizechoice(module, channel))
		return pframe->width != channel->trim_dcam.size_x || channel->zoom_coeff_queue.cnt;
	else
		return module->isp_dev_handle->sw_ctx[channel->isp_ctx_id]->uinfo.path_info[channel->isp_path_id].in_trim.size_x != channel->trim_isp.size_x || channel->zoom_coeff_queue.cnt;
}

static int camcore_dcam_callback(enum dcam_cb_type type, void *param, void *priv_data)
{
	int ret = 0, cap_frame = 0, total_cap_num = 0, recycle = 0;
	uint32_t shutoff = 0;
	struct camera_frame *pframe;
	struct camera_frame *pframe_pre;
	struct camera_module *module;
	struct channel_context *channel;
	struct isp_offline_param *cur;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_reg_trace trace;
	struct dcam_data_ctrl_info dcam_ctrl;
	struct cam_data_ctrl_in ctrl_in;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct dcam_hw_path_stop patharg;
	struct dcam_hw_path_restart re_patharg;
	struct camera_group *grp = NULL;

	if (!param || !priv_data) {
		pr_err("fail to get valid param %px %px\n", param, priv_data);
		return -EFAULT;
	}

	module = (struct camera_module *)priv_data;
	hw = module->grp->hw_info;
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	grp = module->grp;

	if (unlikely(type == DCAM_CB_GET_PMBUF)) {
		struct camera_frame **pm_frame;
		if (module->pmq_init == 0)
			return 0;
		pm_frame = (struct camera_frame **)param;
		*pm_frame = cam_queue_dequeue(&module->param_queue, struct camera_frame, list);
		return 0;
	}

	if (unlikely(type == DCAM_CB_DEV_ERR)) {
		uint32_t status = *(uint32_t*)param;
		pr_err("fail to check cb type. camera %d\n", module->idx);

		csi_api_reg_trace();

		trace.type = ABNORMAL_REG_TRACE;
		trace.idx = dcam_sw_ctx->hw_ctx_id;
		hw->isp_ioctl(hw, ISP_HW_CFG_REG_TRACE, &trace);

		if ((hw->ip_dcam[DCAM_ID_0]->recovery_support & status) && !g_dbg_recovery) {
			pr_info("cam %d start recovery\n", module->idx);
			hw->dcam_ioctl(hw, DCAM_HW_CFG_IRQ_DISABLE, &dcam_sw_ctx->hw_ctx_id);
			complete(&module->grp->recovery_thrd.thread_com);
			return 0;
		}

		module->dcam_dev_handle->dcam_pipe_ops->stop(dcam_sw_ctx, DCAM_DEV_ERR);

		pframe = cam_queue_empty_frame_get();
		if (pframe) {
			pframe->evt = IMG_TX_ERR;
			pframe->irq_type = CAMERA_IRQ_IMG;
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
		}
		complete(&module->frm_com);
		return 0;
	}

	if (atomic_read(&module->grp->recovery_state) == CAM_RECOVERY_DONE) {
		pr_info("recovery success\n");
		atomic_set(&module->grp->recovery_state, CAM_RECOVERY_NONE);
	}
	pframe = (struct camera_frame *)param;
	pframe->priv_data = NULL;
	channel = &module->channel[pframe->channel_id];

	pr_debug("module %px, cam%d ch %d.  cb cmd %d, frame %px w:%d, h:%d\n",
		module, module->idx, pframe->channel_id, type,
		pframe, pframe->width, pframe->height);

	switch (type) {
	case DCAM_CB_DATA_DONE:
		if (pframe->buf.addr_k[0]) {
			uint32_t *ptr = (uint32_t *)pframe->buf.addr_k[0];
			pr_debug("dcam path %d. outdata: %08x %08x %08x %08x\n",
				channel->dcam_path_id, ptr[0], ptr[1], ptr[2], ptr[3]);
		}

		if (atomic_read(&module->state) != CAM_RUNNING || module->paused) {
			pr_info("stream off or paused. put frame %px, state:%d\n", pframe, module->state);
			if (pframe->buf.type == CAM_BUF_KERNEL) {
				cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
			} else {
				/* 4in1 or raw buffer is allocate from user */
				cam_buf_ionbuf_put(&pframe->buf);
				cam_queue_empty_frame_put(pframe);
			}
			return 0;
		}

		if (channel->ch_id == CAM_CH_CAP && pframe->irq_property != CAM_FRAME_COMMON) {
			int32_t isp_ctx_id;
			if (pframe->irq_property == CAM_FRAME_FDRL || pframe->irq_property == CAM_FRAME_RAW_PROC)
				ctrl_in.scene_type = CAM_SCENE_CTRL_FDR_L;
			else
				ctrl_in.scene_type = CAM_SCENE_CTRL_FDR_H;

			module->dcam_dev_handle->dcam_pipe_ops->get_datactrl(dcam_sw_aux_ctx, &ctrl_in, &dcam_ctrl);
			if (dcam_ctrl.callback_ctrl == DCAM_CALLBACK_CTRL_USER) {
				if (pframe->irq_property == CAM_FRAME_FDRL)
					pframe->irq_type = CAMERA_IRQ_FDRL;
				else if (pframe->irq_property == CAM_FRAME_RAW_PROC)
					pframe->irq_type = CAMERA_IRQ_RAW_BPC_IMG;
				else
					pframe->irq_type = CAMERA_IRQ_FDRH;
				module->fdr_done |= (1 << pframe->irq_type);
				pr_debug("fdr %d yuv buf %d done %x\n", pframe->irq_property,
						pframe->buf.mfd[0], module->fdr_done);
				if ((module->fdr_done & (1 << CAMERA_IRQ_FDRL)) &&
					(module->fdr_done & (1 << CAMERA_IRQ_FDRH)))
					complete(&module->cap_thrd.thread_com);
				pframe->evt = IMG_TX_DONE;
				ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
				if (ret) {
					cam_buf_ionbuf_put(&pframe->buf);
					cam_queue_empty_frame_put(pframe);
				} else {
					complete(&module->frm_com);
					pr_debug("ch %d get out frame: %p, evt %d mfd %x\n",
						pframe->channel_id, pframe, pframe->evt, pframe->buf.mfd[0]);
				}
				return ret;
			}
			if (pframe->irq_property == CAM_FRAME_FDRL)
				isp_ctx_id = channel->isp_fdrl_ctx;
			else
				isp_ctx_id = channel->isp_fdrh_ctx;
			if (atomic_read(&dcam_sw_ctx->path[DCAM_PATH_BIN].is_shutoff) == 1 &&
				channel->dcam_path_id == DCAM_PATH_RAW) {
				re_patharg.idx = dcam_sw_ctx->hw_ctx_id;
				re_patharg.path_id = DCAM_PATH_BIN;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_RESTART, &re_patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF,
					DCAM_PATH_BIN, &shutoff);
			}
			pr_info("fdr %d mfd %x, ctx_id 0x%d\n", pframe->irq_property,
				pframe->buf.mfd[0], isp_ctx_id);
			ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe,
				isp_ctx_id);
			return ret;
		}
		if (channel->ch_id == CAM_CH_RAW) {
			/* RAW capture or test_dcam only */
			if (g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN) {
				if (module->dump_thrd.thread_task) {
					if (g_dbg_dumpswitch) {
						if (module->dump_base.dump_enqueue)
							ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
						else {
							camdump_start(&module->dump_thrd, &module->dump_base, module->dcam_idx);
							ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
						}
						if (ret == 0)
							return 0;
					}
					if (!g_dbg_dumpswitch && module->dump_base.dump_enqueue != NULL)
						camdump_stop(&module->dump_base);
				}
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF,
							channel->dcam_path_id, pframe);
				return 0;
			}
			if (module->cam_uinfo.is_4in1 == 0) {
				uint32_t capture = 0;
				if (module->cap_status == CAM_CAPTURE_START) {
					if (module->dcam_cap_status != DCAM_CAPTURE_START_FROM_NEXT_SOF) {
						capture = 1;
					} else if (pframe->boot_sensor_time > module->capture_times) {
						/* raw capture with flash */
						capture = 1;
					}
				}
				if (module->raw_callback == 1)
					capture = 1;
				pr_info("capture %d, fid %d, start %d  type %d\n", capture,
					pframe->fid, module->cap_status, module->dcam_cap_status);
				pr_info("cap time %lld, frame time %lld\n",
					module->capture_times, pframe->boot_sensor_time);
				if (channel->ch_uinfo.scene == DCAM_SCENE_MODE_CAPTURE
					&& capture == 0) {
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF,
							channel->dcam_path_id, pframe);
					return 0;
				}
				pframe->evt = IMG_TX_DONE;
				pframe->irq_type = CAMERA_IRQ_IMG;
				pframe->priv_data = module;
				ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
				complete(&module->frm_com);
				pr_info("get out raw frame: fd:%d\n", pframe->buf.mfd[0]);
				return 0;
			}

			pframe->evt = IMG_TX_DONE;
			if (pframe->irq_type != CAMERA_IRQ_4IN1_DONE)
				pframe->irq_type = CAMERA_IRQ_IMG;
			if (module->cam_uinfo.is_4in1) {
				pframe = camcore_4in1_raw_capture_deal(module, pframe);
				if (!pframe)
					return 0;
			}
			/* set width,heigth */
			pframe->width = channel->ch_uinfo.dst_size.w;
			pframe->height = channel->ch_uinfo.dst_size.h;
			pframe->priv_data = module;
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
			complete(&module->frm_com);
			pr_info("get out raw frame: fd:%d [%d %d]\n",
				pframe->buf.mfd[0], pframe->width, pframe->height);

		} else if (channel->ch_id == CAM_CH_PRE
			|| channel->ch_id == CAM_CH_VID) {

			pr_debug("proc isp path %d, ctx %d\n", channel->isp_path_id, channel->isp_ctx_id);
			/* ISP in_queue maybe overflow.
			 * If previous frame with size updating is dicarded by ISP,
			 * we should set it in current frame for ISP input
			 * If current frame also has new updating param,
			 * here will set it as previous one in a queue for ISP,
			 * ISP can check and free it.
			 */
			if (channel->isp_updata) {
				pr_info("next %p,  prev %p\n",
					pframe->param_data, channel->isp_updata);
				if (pframe->param_data) {
					cur = (struct isp_offline_param *)pframe->param_data;
					cur->prev = channel->isp_updata;
				} else {
					pframe->param_data = channel->isp_updata;
				}
				channel->isp_updata = NULL;
				pr_info("cur %p\n", pframe->param_data);
			}

			if ((module->flash_skip_fid == pframe->fid) && (module->flash_skip_fid != 0) && (!channel->ch_uinfo.is_high_fps)) {
				pr_debug("flash_skip_frame fd = %d\n", pframe->fid);
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_OUTPUT_BUF,
					channel->dcam_path_id, pframe);
				if (pframe->param_data) {
					cur = (struct isp_offline_param *)pframe->param_data;
					cur->prev = channel->isp_updata;
					channel->isp_updata = (void *)cur;
					pframe->param_data = NULL;
					pr_debug("store:  cur %p   prev %p\n", cur, cur->prev);
				}
				return ret;
			}

			if (module->capture_scene == CAPTURE_HW3DNR ||
				module->capture_scene == CAPTURE_FLASH ||
				module->capture_scene == CAPTURE_FDR) {
				pframe->need_gtm_hist = 0;
				pframe->need_gtm_map = 0;
				pframe->need_ltm_hist = 0;
				pframe->need_ltm_map = 0;
				pframe->gtm_mod_en = 0;
			}

			ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe,
					channel->isp_ctx_id);
			if (ret) {
				pr_warn_ratelimited("warning: isp proc frame failed.\n");
				/* ISP in_queue maybe overflow.
				 * If current frame taking (param_data) for size updating
				 * we should hold it here and set it in next frame for ISP
				 */
				if (pframe->param_data) {
					cur = (struct isp_offline_param *)pframe->param_data;
					cur->prev = channel->isp_updata;
					channel->isp_updata = (void *)cur;
					pframe->param_data = NULL;
					pr_info("store:  cur %p   prev %p\n", cur, cur->prev);
				}
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_OUTPUT_BUF,
					channel->dcam_path_id, pframe);
			}
		} else if (channel->ch_id == CAM_CH_CAP) {
			if (pframe->irq_property != CAM_FRAME_COMMON) {
				/* FDR frames should always be processed by ISP */
				int32_t isp_ctx_id;

				if (pframe->irq_property == CAM_FRAME_FDRL)
					isp_ctx_id = channel->isp_fdrl_ctx;
				else
					isp_ctx_id = channel->isp_fdrh_ctx;
				pr_info("fdr %d mfd %d, ctx_id 0x%x\n", pframe->irq_property,
					pframe->buf.mfd[0], isp_ctx_id);
				ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe,
					isp_ctx_id);
				return ret;
			}
			if ((module->cap_status != CAM_CAPTURE_START) &&
				(module->cap_status != CAM_CAPTURE_RAWPROC)) {
				if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1 &&
					pframe->img_fmt == IMG_PIX_FMT_GREY) {
					pr_info("FDR capture stopped, free buf fd %d\n", pframe->buf.mfd[0]);
					cam_buf_ionbuf_put(&pframe->buf);
					cam_queue_empty_frame_put(pframe);
					return ret;
				}

				if (module->cam_uinfo.is_dual) {
					mutex_lock(&grp->dual_deal_lock);
					if (module->master_flag == 0 && atomic_read(&module->cap_flag) == 1 && module->dual_frame == NULL) {
						module->dual_frame = pframe;
						mutex_unlock(&grp->dual_deal_lock);
						return 0;
					}
					mutex_unlock(&grp->dual_deal_lock);
					pframe = camcore_dual_fifo_queue(module,
						pframe, channel);
				} else if (channel->zsl_skip_num == module->cam_uinfo.zsk_skip_num) {
					if (channel->zsl_skip_num)
						channel->zsl_skip_num --;
					else
						channel->zsl_skip_num = module->cam_uinfo.zsk_skip_num;
					ret = cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
					if (channel->share_buf_queue.cnt > channel->zsl_buffer_num)
						pframe = cam_queue_dequeue(&channel->share_buf_queue,
							struct camera_frame, list);
					else
						return ret;
				} else {
					if (channel->zsl_skip_num)
						channel->zsl_skip_num --;
					else
						channel->zsl_skip_num = module->cam_uinfo.zsk_skip_num;
				}
				pr_debug("share buf queue cnt %d skip_cnt %d\n",
					channel->share_buf_queue.cnt, channel->zsl_skip_num);
				if (pframe) {
					uint32_t cmd;
					cmd = (pframe->img_fmt == IMG_PIX_FMT_GREY) ?
							DCAM_PATH_CFG_OUTPUT_ALTER_BUF :
							DCAM_PATH_CFG_OUTPUT_BUF;
					if (pframe->dcam_idx == DCAM_ID_1)
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
							cmd, channel->aux_dcam_path_id, pframe);
					else
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							cmd, channel->dcam_path_id, pframe);
				}
				return ret;
			}

			ret = camcore_rawalg_proc(module, channel, pframe, hw);
			if (ret) {
				pr_err("fail to process raw image.\n");
				return -EFAULT;
			}
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2 &&
				pframe->bpc_raw_flag)
				return ret;

			/* cap scene special process */
			if (DCAM_FETCH_TWICE(dcam_sw_aux_ctx) && DCAM_FIRST_FETCH(dcam_sw_aux_ctx)) {
				ret = camcore_frame_start_proc(module, pframe);
				if (ret)
					pr_err("fail to start dcam/isp for raw proc\n");
				return 0;
			} else if (module->dcam_cap_status == DCAM_CAPTURE_START_WITH_TIMESTAMP) {

				mutex_lock(&grp->dual_deal_lock);
				if (module->master_flag == 1) {
					if (pframe->boot_sensor_time < module->capture_times) {
						pframe = camcore_dual_fifo_queue(module, pframe, channel);
						mutex_unlock(&grp->dual_deal_lock);
						if (!pframe)
							return 0;
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
						return 0;
					}
					if (atomic_read(&module->cap_flag) == 0) {
						camcore_dual_frame_deal(module, pframe, channel);
						atomic_set(&module->cap_flag, 1);
					}
				}

				if (module->master_flag == 0) {
					if (atomic_read(&module->cap_flag) == 0) {
						pframe = camcore_dual_fifo_queue(module, pframe, channel);
						mutex_unlock(&grp->dual_deal_lock);
						if (!pframe)
							return 0;
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
						return 0;
					} else {
						if (module->dual_frame != NULL) {
							ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
								DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
							 pframe = module->dual_frame;
							 module->dual_frame = NULL;
						}
						atomic_set(&module->cap_flag, 0);
					}
				}

				mutex_unlock(&grp->dual_deal_lock);
				if (!pframe)
					return 0;

			} else if (module->cam_uinfo.is_4in1) {
				pframe = camcore_4in1_frame_deal(module,
						pframe, channel);
				if (!pframe)
					return 0;

			} else if (module->cam_uinfo.dcam_slice_mode) {
				pframe = camcore_supersize_frame_deal(module, pframe, channel);
				if (!pframe)
					return 0;
			} else if (module->dcam_cap_status == DCAM_CAPTURE_START_FROM_NEXT_SOF) {
				/* FDR catpure should wait for RAW buffer except time condition */
				if (camcore_rawalg_judge(module->capture_scene)) {
					if ((pframe->boot_sensor_time < module->capture_times) ||
						(pframe->img_fmt != IMG_PIX_FMT_GREY) ||
						(atomic_read(&module->capture_frames_dcam) < 1)) {
						uint32_t cmd, dcam_path_id;
						pr_info("discard fdr frame: fd %x\n", pframe->buf.mfd[0]);
						cmd = (pframe->img_fmt == IMG_PIX_FMT_GREY) ?
							DCAM_PATH_CFG_OUTPUT_ALTER_BUF :
							DCAM_PATH_CFG_OUTPUT_BUF;
						dcam_path_id = channel->dcam_path_id;
						if (module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR &&
							pframe->img_fmt == IMG_PIX_FMT_GREY)
							dcam_path_id = DCAM_PATH_RAW;
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, cmd,
							dcam_path_id, pframe);
						return ret;
					}

					atomic_dec(&module->capture_frames_dcam);
					if (module->grp->hw_info->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW && module->cam_uinfo.is_raw_alg &&
						atomic_read(&module->capture_frames_dcam) < 1 && module->cam_uinfo.raw_alg_type != RAW_ALG_MFNR) {
						shutoff = 1;
						patharg.path_id = DCAM_PATH_RAW;
						patharg.idx = dcam_sw_ctx->hw_ctx_id;
						patharg.raw_alg_type = module->cam_uinfo.raw_alg_type;
						hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_STOP, &patharg);
						module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF,
							DCAM_PATH_RAW, &shutoff);

						if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1) {
						       patharg.path_id = DCAM_PATH_BIN;
						       patharg.idx = dcam_sw_ctx->hw_ctx_id;
						       hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_STOP, &patharg);
						       hw->dcam_ioctl(hw, DCAM_HW_CFG_STOP_CAP_EB, &patharg.idx);
						       module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_SHUTOFF,
						               DCAM_PATH_BIN, &shutoff);
						}
					}
					if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) {
						if (channel->dcam_path_id == DCAM_PATH_RAW)
							ret = cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
						else {
							pframe->irq_property = CAM_FRAME_PRE_FDR;
							ret = camcore_frame_start_proc(module, pframe);
						}
						if (ret)
							pr_err("fail to start dcams for raw proc\n");
					} else {
						pframe->evt = IMG_TX_DONE;
						pframe->irq_type = CAMERA_IRQ_RAW_IMG;
						pframe->priv_data = module;
						ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
						complete(&module->frm_com);
					}
					pr_info("get fdr raw frame: fd %d\n", pframe->buf.mfd[0]);
					return ret;
				}
				if (module->cam_uinfo.is_dual) {
					while (1) {
						pframe_pre = cam_queue_dequeue(&module->zsl_fifo_queue,
							struct camera_frame, list);
						if (!pframe_pre)
							break;
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF,
							channel->dcam_path_id, pframe_pre);
					};
				}
				if (pframe->boot_sensor_time < module->capture_times) {
					pr_info("cam%d cap skip frame type[%d] cap_time[%lld] sof_time[%lld]\n",
						module->idx,
						module->dcam_cap_status,
						module->capture_times,
						pframe->boot_sensor_time);
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id, pframe);

					return ret;
				} else {
					pframe->not_use_isp_reserved_buf = 1;
					if (module->capture_scene == CAPTURE_HW3DNR ||
						module->capture_scene == CAPTURE_FLASH ||
						module->capture_scene == CAPTURE_FDR) {
						pframe->need_gtm_hist = 0;
						pframe->need_gtm_map = 0;
						pframe->need_ltm_hist = 0;
						pframe->need_ltm_map = 0;
						pframe->gtm_mod_en = 0;
					}

					cap_frame = atomic_read(&module->capture_frames_dcam);
					total_cap_num = atomic_read(&module->cap_total_frames);
					recycle = 0;
					if (cap_frame == 0)
						recycle = 1;
					if ((module->capture_scene == CAPTURE_MFSR) &&
						((module->cam_uinfo.is_pyr_dec && (pframe->width != channel->ch_uinfo.src_crop.w) &&
						(cap_frame == total_cap_num)) || channel->zoom_coeff_queue.cnt))
						recycle = 1;
					if (recycle) {
						pr_info("cam%d cap type[%d], fid %d, frame width %d, channal width src %d, cap frame %d, scene %d, zoom_q cnt %d\n",
							module->idx, module->dcam_cap_status, pframe->fid, pframe->width, channel->ch_uinfo.src_crop.w, cap_frame,
							module->capture_scene,channel->zoom_coeff_queue.cnt);
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF,
							channel->dcam_path_id, pframe);

						return ret;
					}
					pr_info("cam%d cap type[%d] num[%d] frame_id %d\n", module->idx, module->dcam_cap_status, cap_frame, pframe->fid);
				}
			} else if (module->dcam_cap_status == DCAM_CAPTURE_START && pframe->boot_sensor_time < module->capture_times) {
				pr_debug("cam%d cap skip frame type[%d] cap_time[%lld] sof_time[%lld]\n", module->idx, module->dcam_cap_status,
					module->capture_times, pframe->boot_sensor_time);
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF, channel->dcam_path_id, pframe);
				return ret;
			}
			/* to isp */
			/* skip first frame for online capture (in case of non-zsl) because lsc abnormal */
			if (camcore_dcam_capture_skip_condition(pframe, channel, module))
				ret = 1;
			else
				ret = cam_queue_enqueue(&channel->share_buf_queue, &pframe->list);
			if (ret) {
				pr_warn("warning: capture queue overflow\n");
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id, pframe);
			} else {
				if ((channel->share_buf_queue.cnt > channel->zsl_buffer_num) &&
					(module->cap_status != CAM_CAPTURE_RAWPROC) && channel->zsl_buffer_num) {
					pframe = cam_queue_dequeue(&channel->share_buf_queue,
						struct camera_frame, list);
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(
							dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF,
							channel->dcam_path_id, pframe);
				}
				if (atomic_read(&module->capture_frames_dcam) > 0)
					atomic_dec(&module->capture_frames_dcam);
				pr_debug("capture_frames_dcam = %d, share frame cnt %d, zsl num %d\n", atomic_read(&module->capture_frames_dcam),
					channel->share_buf_queue.cnt, channel->zsl_buffer_num);
				complete(&module->cap_thrd.thread_com);
			}
		} else {
			/* should not be here */
			pr_warn("warning: reset dcam path out %d for ch %d\n",
				channel->dcam_path_id, channel->ch_id);
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe);
		}
		break;
	case DCAM_CB_STATIS_DONE:
		pframe->evt = IMG_TX_DONE;
		pframe->irq_type = CAMERA_IRQ_STATIS;
		/* temp: statis/irq share same queue with frame data. */
		/* todo: separate statis/irq and frame queue. */
		if ((pframe->irq_property != STATIS_PARAM) && (module->flash_skip_fid != 0))
			pframe->is_flash_status = module->is_flash_status;

		pr_debug("pframe->fid %d is_flash_status %d irq_property %d\n",pframe->fid, pframe->is_flash_status, pframe->irq_property);
		if (g_dbg_dump.dump_en == DUMP_DCAM_PDAF &&
			module->dump_base.dump_enqueue != NULL
			&& pframe->irq_property == STATIS_PDAF) {
			ret = module->dump_base.dump_enqueue(&module->dump_base, pframe);
			if (ret == 0)
				return 0;
		}
		if (atomic_read(&module->state) == CAM_RUNNING) {
			pframe->priv_data = module;
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
			if (ret) {
				cam_queue_empty_frame_put(pframe);
			} else {
				complete(&module->frm_com);
				pr_debug("get statis frame: %p, type %d, %d\n",
				pframe, pframe->irq_type, pframe->irq_property);
			}
		} else {
			cam_queue_empty_frame_put(pframe);
		}
		break;

	case DCAM_CB_IRQ_EVENT:
		if (pframe->irq_property == IRQ_DCAM_SN_EOF) {
			cam_queue_empty_frame_put(pframe);
			break;
		}
		if (pframe->irq_property == IRQ_DCAM_SOF) {
			if ((module->flash_info.led0_ctrl && module->flash_info.led0_status < FLASH_STATUS_MAX) ||
				(module->flash_info.led1_ctrl && module->flash_info.led1_status < FLASH_STATUS_MAX)) {
				module->flash_core_handle->flash_core_ops->start_flash(module->flash_core_handle,
					&module->flash_info.set_param);
				if (module->flash_info.flash_last_status != module->flash_info.led0_status) {
					module->flash_skip_fid = pframe->fid;
					module->is_flash_status = module->flash_info.led0_status;
				} else
					pr_info("do not need skip");
				pr_info("skip_fram=%d\n", pframe->fid);
				module->flash_info.flash_last_status = module->flash_info.led0_status;
				module->flash_info.led0_ctrl = 0;
				module->flash_info.led1_ctrl = 0;
				module->flash_info.led0_status = 0;
				module->flash_info.led1_status = 0;
			}

		}
		/* temp: statis/irq share same queue with frame data. */
		/* todo: separate statis/irq and frame queue. */
		if (atomic_read(&module->state) == CAM_RUNNING) {
			ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
			if (ret) {
				cam_queue_empty_frame_put(pframe);
			} else {
				complete(&module->frm_com);
				pr_debug("get irq frame: %p, type %d, %d\n",
				pframe, pframe->irq_type, pframe->irq_property);
			}
		} else {
			cam_queue_empty_frame_put(pframe);
		}
		break;

	case DCAM_CB_RET_SRC_BUF:
		if (pframe->irq_property != CAM_FRAME_COMMON) {
			pr_info("fdr %d src return, mfd %d\n", pframe->irq_property, pframe->buf.mfd[0]);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			break;
		}

		pr_info("dcam ret src frame %px. module %p, %d\n", pframe, module, module->idx);

		if (module->cam_uinfo.is_4in1) {
			/* 4in1 capture case: dcam offline src buffer
			 * should be re-used for dcam online output (raw)
			 */
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe);
		} else if (module->cam_uinfo.virtualsensor) {
			/* for case raw capture post-proccessing
			 * and case 4in1 after stream off
			 * just release it, no need to return
			 */
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			pr_debug("virtual sensor ret buf\n");
		} else if ((module->cap_status == CAM_CAPTURE_RAWPROC) ||
			(atomic_read(&module->state) != CAM_RUNNING)) {
			/* for case raw capture post-proccessing
			 * and case 4in1 after stream off
			 * just release it, no need to return
			 */
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			pr_info("cap status %d, state %d\n", module->cap_status, atomic_read(&module->state));
		} else if ((module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) || module->cam_uinfo.dcam_slice_mode) {
			/* 4in1 capture case: dcam offline src buffer
			 * should be re-used for dcam online output (raw)
			 */
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe);

		} else {
			pr_err("fail to get cap status\n");
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
		}
		break;

	case DCAM_CB_VCH2_DONE:
		pframe->evt = IMG_TX_DONE;
		pframe->irq_type = CAMERA_IRQ_IMG;
		pframe->priv_data = module;
		ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
		if (ret) {
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
		} else {
			complete(&module->frm_com);
			pr_debug("get out sensor raw frame: fd:%d\n", pframe->buf.mfd[0]);
		}
		break;
	default:
		break;
	}

	return ret;
}

static int camcore_bigsize_aux_init(struct camera_module *module,
		struct channel_context *channel)
{
	int ret = 0;
	uint32_t dcam_path_id;
	uint32_t offline_fbc_mode;
	struct camera_group *grp = module->grp;
	struct dcam_path_cfg_param ch_desc;
	struct dcam_pipe_dev *dev = NULL;

	dev = module->aux_dcam_dev;
	if (dev == NULL) {
		dev = dcam_core_pipe_dev_get(grp->hw_info);
		if (IS_ERR_OR_NULL(dev)) {
			pr_err("fail to get dcam\n");
			return -EFAULT;
		}
		module->aux_dcam_dev = dev;
	}

	module->aux_dcam_id = DCAM_HW_CONTEXT_1;
	dev->sw_ctx[module->offline_cxt_id].dcam_slice_mode = module->cam_uinfo.dcam_slice_mode;
	dev->sw_ctx[module->offline_cxt_id].slice_count = 0;

	ret = module->dcam_dev_handle->dcam_pipe_ops->open(dev);
	if (ret < 0) {
		pr_err("fail to open aux dcam dev\n");
		ret = -EFAULT;
		goto exit_dev;
	}

	/* todo: will update after dcam offline ctx done. */
	offline_fbc_mode = dev->hw->ip_dcam[DCAM_HW_CONTEXT_1]->dcam_offline_fbc_mode;
	if (!offline_fbc_mode)
		dcam_path_id = dev->hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path;
	else
		dcam_path_id = DCAM_PATH_FULL;

	ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(&dev->sw_ctx[module->offline_cxt_id],
		dcam_path_id);
	if (ret < 0) {
		pr_err("fail to get dcam path %d\n", dcam_path_id);
		ret = -EFAULT;
		goto exit_close;
	} else {
		channel->aux_dcam_path_id = dcam_path_id;
		pr_info("get aux_dcam_path_id %d, cur_aux_sw_ctx_id %d\n", dcam_path_id, module->offline_cxt_id);
	}

	/* cfg dcam1 bin path */
	memset(&ch_desc, 0, sizeof(ch_desc));
	ch_desc.endian.y_endian = ENDIAN_LITTLE;

	if (channel->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10 && channel->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX)
		ch_desc.raw_fmt = channel->ch_uinfo.dcam_raw_fmt;
	else {
		ch_desc.raw_fmt = dev->hw->ip_dcam[0]->raw_fmt_support[0];
		if (dev->hw->ip_dcam[0]->save_band_for_bigsize)
			ch_desc.raw_fmt = DCAM_RAW_PACK_10;
		channel->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
	}

	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
	dev->sw_ctx[module->offline_cxt_id].fetch.fmt= DCAM_STORE_RAW_BASE;
	dev->sw_ctx[module->offline_cxt_id].pack_bits = channel->ch_uinfo.sensor_raw_fmt;

	if (module->dcam_dev_handle->hw->ip_isp->fetch_raw_support == 0)
		ch_desc.dcam_out_fmt = DCAM_STORE_YVU420;
	else
		ch_desc.dcam_out_fmt = DCAM_STORE_RAW_BASE;
	ch_desc.dcam_out_bits = channel->ch_uinfo.dcam_output_bit;

	pr_debug("fetch packbit %d, out fmt %d, packbit %d\n",
		dev->sw_ctx[module->offline_cxt_id].pack_bits,
		ch_desc.dcam_out_fmt,
		ch_desc.raw_fmt);
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(&dev->sw_ctx[module->offline_cxt_id],
		DCAM_PATH_CFG_BASE, channel->aux_dcam_path_id, &ch_desc);
	channel->dcam_out_fmt = ch_desc.dcam_out_fmt;

	pr_info("done\n");
	return ret;

exit_close:
	module->dcam_dev_handle->dcam_pipe_ops->close(dev);
exit_dev:
	dcam_core_pipe_dev_put(dev);
	module->aux_dcam_dev = NULL;
	return ret;
}

static int camcore_bigsize_aux_deinit(struct camera_module *module)
{
	int ret = 0;
	uint32_t dcam_path_id;
	uint32_t offline_fbc_mode;
	struct dcam_sw_context *sw_ctx = NULL;

	offline_fbc_mode = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_1]->dcam_offline_fbc_mode;
	if (!offline_fbc_mode)
		dcam_path_id = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_1]->aux_dcam_path;
	else
		dcam_path_id = DCAM_PATH_FULL;

	sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	pr_debug("aux_dcam_path id %d, cur_aux_sw_ctx_id %d\n", dcam_path_id, module->offline_cxt_id);

	ret = module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, DCAM_STOP);
	if (ret) {
		pr_err("fail to dcam_pipe_ops stop\n");
		goto exit;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx, dcam_path_id);
	if (ret) {
		pr_err("fail to dcam_pipe_ops put_path\n");
		goto exit;
	}

	ret = dcam_core_context_unbind(sw_ctx);
	if (ret) {
		pr_err("fail to context_unbind\n");
		goto exit;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->close(module->dcam_dev_handle);
	if (ret) {
		pr_err("fail to dcam_pipe_ops close\n");
		goto exit;
	}

	ret = dcam_core_pipe_dev_put(module->dcam_dev_handle);
	if (ret) {
		pr_err("fail to dcam_core_pipe_dev_put\n");
		goto exit;
	}

	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;

exit:
	return ret;
}

static int camcore_channel_swapsize_cal(struct camera_module *module)
{
	uint32_t src_binning = 0;
	uint32_t ratio_min, shift;
	uint32_t ratio_p_w, ratio_p_h;
	uint32_t ratio_v_w, ratio_v_h;
	uint32_t ratio_p_w1, ratio_p_h1;
	uint32_t ratio_v_w1, ratio_v_h1;
	uint32_t isp_linebuf_len = g_camctrl.isp_linebuf_len;
	struct channel_context *ch_prev = NULL;
	struct channel_context *ch_vid = NULL;
	struct channel_context *ch_cap = NULL;
	struct channel_context *ch_raw = NULL;
	struct img_size max_bypass, max_bin, max_rds, max_scaler, temp;
	struct img_size src_p, dst_p, dst_v, max;

	ch_prev = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
	ch_raw = &module->channel[CAM_CH_RAW];

	if (module->channel[CAM_CH_CAP_THM].enable || module->grp->camsec_cfg.camsec_mode != SEC_UNABLE)
		module->grp->hw_info->ip_dcam[0]->rds_en = 0;

	if (module->grp->hw_info->ip_dcam[0]->rds_en) {
		if (ch_vid->enable && (ch_prev->ch_uinfo.src_size.w <= CAM_VIDEO_LIMIT_W
			|| ch_prev->ch_uinfo.src_size.h <= CAM_VIDEO_LIMIT_H))
			module->zoom_solution = ZOOM_DEFAULT;
	}

	if (ch_cap->enable) {
		max.w = ch_cap->ch_uinfo.src_size.w;
		max.h = ch_cap->ch_uinfo.src_size.h;
		ch_cap->swap_size = max;
		pr_info("idx %d , cap swap size %d %d\n", module->idx, max.w, max.h);
	}

	if (ch_raw->enable) {
		max.w = ch_raw->ch_uinfo.src_size.w;
		max.h = ch_raw->ch_uinfo.src_size.h;
		ch_raw->swap_size = max;
		pr_info("idx %d , raw swap size %d %d\n", module->idx, max.w, max.h);
	}

	if (ch_prev->enable)
		ch_prev = &module->channel[CAM_CH_PRE];
	else if (!ch_prev->enable && ch_vid->enable)
		ch_prev = &module->channel[CAM_CH_VID];
	else
		return 0;

	if (module->cam_uinfo.is_4in1 || module->cam_uinfo.dcam_slice_mode) {
		ch_prev->ch_uinfo.src_size.w >>= 1;
		ch_prev->ch_uinfo.src_size.h >>= 1;
		ch_vid->ch_uinfo.src_size.w >>= 1;
		ch_vid->ch_uinfo.src_size.h >>= 1;
	}

	src_p.w = ch_prev->ch_uinfo.src_size.w;
	src_p.h = ch_prev->ch_uinfo.src_size.h;
	dst_p.w = ch_prev->ch_uinfo.dst_size.w;
	dst_p.h = ch_prev->ch_uinfo.dst_size.h;
	dst_v.w = dst_v.h = 1;
	if (ch_vid->enable) {
		dst_p.w = ch_vid->ch_uinfo.dst_size.w;
		dst_p.h = ch_vid->ch_uinfo.dst_size.h;
	}

	if ((src_p.w * 2) <= module->cam_uinfo.sn_max_size.w)
		src_binning = 1;

	/* bypass dcam scaler always */
	max_bypass = src_p;

	/* go through binning path */
	max_bin = src_p;
	shift = 0;
	if (src_binning == 1) {
		if ((max_bin.w > isp_linebuf_len) &&
			(dst_p.w <= isp_linebuf_len) &&
			(dst_v.w <= isp_linebuf_len))
			shift = 1;

		module->binning_limit = 0;
		if (module->zoom_solution == ZOOM_BINNING4)
			module->binning_limit = 1;
		else if (shift == 1)
			module->binning_limit = 1;

		pr_info("shift %d for binning, p=%d v=%d  src=%d, %d\n",
			shift, dst_p.w, dst_v.w, max_bin.w, isp_linebuf_len);
	} else {
		if ((max_bin.w >= (dst_p.w * 2)) &&
			(max_bin.w >= (dst_v.w * 2)))
			shift = 1;
		else if ((max_bin.w > isp_linebuf_len) &&
			(dst_p.w <= isp_linebuf_len) &&
			(dst_v.w <= isp_linebuf_len))
			shift = 1;

		module->binning_limit = 1;
		if (module->zoom_solution == ZOOM_BINNING4)
			module->binning_limit = 2;

		pr_info("shift %d for full, p=%d v=%d  src=%d, %d\n",
			shift, dst_p.w, dst_v.w, max_bin.w, isp_linebuf_len);
	}
	if (SEC_UNABLE != module->grp->camsec_cfg.camsec_mode)
		shift = 1;
	max_bin.w >>= shift;
	max_bin.h >>= shift;

	/* scaler */
	max_scaler.w = MAX(ch_prev->ch_uinfo.dst_size.w, ch_vid->ch_uinfo.dst_size.w);
	max_scaler.h = MAX(ch_prev->ch_uinfo.dst_size.h, ch_vid->ch_uinfo.dst_size.h);
	max_scaler.w = MAX(max_bin.w, max_scaler.w);
	max_scaler.h = MAX(max_bin.h, max_scaler.h);

	/* go through rds path */
	if ((dst_p.w == 0) || (dst_p.h == 0)) {
		pr_err("fail to get valid w %d h %d\n", dst_p.w, dst_p.h);
		return -EFAULT;
	}
	max = src_p;
	ratio_p_w = (1 << RATIO_SHIFT) * max.w / dst_p.w;
	ratio_p_h = (1 << RATIO_SHIFT) * max.h / dst_p.h;
	ratio_min = MIN(ratio_p_w, ratio_p_h);
	temp.w = ((max.h * dst_p.w) / dst_p.h) & (~3);
	temp.h = ((max.w * dst_p.h) / dst_p.w) & (~3);
	ratio_p_w1 = (1 << RATIO_SHIFT) * temp.w / dst_p.w;
	ratio_p_h1 = (1 << RATIO_SHIFT) * temp.h / dst_p.h;
	ratio_min = MIN(ratio_min, MIN(ratio_p_w1, ratio_p_h1));
	if (ch_vid->enable) {
		ratio_v_w = (1 << RATIO_SHIFT) * max.w / dst_v.w;
		ratio_v_h = (1 << RATIO_SHIFT) * max.h / dst_v.h;
		ratio_min = MIN(ratio_min, MIN(ratio_v_w, ratio_v_h));
		temp.w = ((max.h * dst_v.w) / dst_v.h) & (~3);
		temp.h = ((max.w * dst_v.h) / dst_v.w) & (~3);
		ratio_v_w1 = (1 << RATIO_SHIFT) * temp.w / dst_v.w;
		ratio_v_h1 = (1 << RATIO_SHIFT) * temp.h / dst_v.h;
		ratio_min = MIN(ratio_min, MIN(ratio_v_w1, ratio_v_h1));
	}
	ratio_min = MIN(ratio_min, ((module->rds_limit << RATIO_SHIFT) / 10));
	ratio_min = MAX(ratio_min, (1 << RATIO_SHIFT));
	max.w = camcore_scale_fix(max.w, ratio_min);
	max.h = camcore_scale_fix(max.h, ratio_min);
	if (max.w > DCAM_RDS_OUT_LIMIT) {
		max.w = DCAM_RDS_OUT_LIMIT;
		max.h = src_p.h * max.w / src_p.w;
	}
	max.w = ALIGN(max.w + ALIGN_OFFSET, ALIGN_OFFSET);
	max.h = ALIGN(max.h + ALIGN_OFFSET, ALIGN_OFFSET);
	max_rds = max;

	/* for adaptive solution, select max of rds/bin */
	switch (module->zoom_solution) {
	case ZOOM_DEFAULT:
		ch_prev->swap_size = max_bypass;
		break;
	case ZOOM_BINNING2:
	case ZOOM_BINNING4:
		ch_prev->swap_size = max_bin;
		break;
	case ZOOM_RDS:
		ch_prev->swap_size = max_rds;
		break;
	case ZOOM_ADAPTIVE:
		ch_prev->swap_size.w = MAX(max_bin.w, max_rds.w);
		ch_prev->swap_size.h = MAX(max_bin.h, max_rds.h);
		break;
	case ZOOM_SCALER:
		ch_prev->swap_size = max_scaler;
		break;
	default:
		pr_warn("warning: unknown zoom solution %d\n", module->zoom_solution);
		ch_prev->swap_size = max_bypass;
		break;
	}
	pr_info("prev bypass size (%d %d), bin size (%d %d)\n",
		max_bypass.w, max_bypass.h, max_bin.w, max_bin.h);
	pr_info("prev swap size (%d %d), rds size (%d %d)\n",
		ch_prev->swap_size.w, ch_prev->swap_size.h,
		max_rds.w, max_rds.h);

	return 0;
}

static int camcore_channel_size_binning_cal(
	struct camera_module *module, uint32_t bypass_always)
{
	uint32_t shift = 0, factor = 0, align_size = 0;
	uint32_t src_binning = 0;
	uint32_t ratio_w = 0, ratio_h = 0, ratio_min = 0;
	struct channel_context *ch_prev;
	struct channel_context *ch_vid;
	struct channel_context *ch_cap;
	struct channel_context *ch_cap_thm;
	struct sprd_img_rect *crop_p, *crop_v, *crop_c;
	struct sprd_img_rect *total_crop_p = NULL;
	struct sprd_img_rect crop_dst;
	struct sprd_img_rect total_crop_dst;
	struct img_trim total_trim_pv = {0};
	struct img_trim trim_pv = {0};
	struct img_trim trim_c = {0};
	struct img_trim *isp_trim;
	struct img_size src_p, dst_p, dst_v, dcam_out;

	ch_prev = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
	ch_cap_thm = &module->channel[CAM_CH_CAP_THM];
	if (!ch_prev->enable && !ch_cap->enable && !ch_vid->enable)
		return 0;

	dcam_out.w = dcam_out.h = 0;
	dst_p.w = dst_p.h = 1;
	dst_v.w = dst_v.h = 1;
	crop_p = crop_v = crop_c = NULL;
	if (ch_prev->enable || (!ch_prev->enable && ch_vid->enable)) {
		src_p.w = ch_prev->ch_uinfo.src_size.w;
		src_p.h = ch_prev->ch_uinfo.src_size.h;
		crop_p = &ch_prev->ch_uinfo.src_crop;
		total_crop_p = &ch_prev->ch_uinfo.total_src_crop;
		dst_p.w = ch_prev->ch_uinfo.dst_size.w;
		dst_p.h = ch_prev->ch_uinfo.dst_size.h;
		if ((src_p.w * 2) <= module->cam_uinfo.sn_max_size.w)
			src_binning = 1;
		pr_info("src crop prev %u %u %u %u\n",
			crop_p->x, crop_p->y, crop_p->w, crop_p->h);
	}
	if (ch_vid->enable) {
		crop_v = &ch_vid->ch_uinfo.src_crop;
		dst_v.w = ch_vid->ch_uinfo.dst_size.w;
		dst_v.h = ch_vid->ch_uinfo.dst_size.h;
		pr_info("src crop vid %u %u %u %u\n",
			crop_v->x, crop_v->y, crop_v->w, crop_v->h);
	}
	if (ch_prev->enable || (!ch_prev->enable && ch_vid->enable)) {
		crop_dst = *crop_p;
		camcore_largest_crop_get(&crop_dst, crop_v);
		trim_pv.start_x = crop_dst.x;
		trim_pv.start_y = crop_dst.y;
		trim_pv.size_x = crop_dst.w;
		trim_pv.size_y = crop_dst.h;
	}

	if (ch_cap->enable) {
		trim_c.start_x = ch_cap->ch_uinfo.src_crop.x;
		trim_c.start_y = ch_cap->ch_uinfo.src_crop.y;
		trim_c.size_x = ch_cap->ch_uinfo.src_crop.w;
		trim_c.size_y = ch_cap->ch_uinfo.src_crop.h;
	}
	pr_info("trim_pv: %u %u %u %u\n", trim_pv.start_x,
		trim_pv.start_y, trim_pv.size_x, trim_pv.size_y);
	pr_info("trim_c: %u %u %u %u\n", trim_c.start_x,
		trim_c.start_y, trim_c.size_x, trim_c.size_y);

	if (ch_prev->enable || (!ch_prev->enable && ch_vid->enable)) {
		shift = 0;
		if (bypass_always == 0) {
			factor = (src_binning ? 10 : 9);
			if ((trim_pv.size_x >= (dst_p.w * 4)) &&
				(trim_pv.size_x >= (dst_v.w * 4)) &&
				(trim_pv.size_y >= (dst_p.h * 4)) &&
				(trim_pv.size_y >= (dst_v.h * 4)))
				shift = 2;
			else if ((trim_pv.size_x >= (dst_p.w * 2 * factor / 10)) &&
				(trim_pv.size_x >= (dst_v.w * 2 * factor / 10)) &&
				(trim_pv.size_y >= (dst_p.h * 2 * factor / 10)) &&
				(trim_pv.size_y >= (dst_v.h * 2 * factor / 10)))
					shift = 1;
			if (((trim_pv.size_x >> shift) > ch_prev->swap_size.w) ||
				((trim_pv.size_y >> shift) > ch_prev->swap_size.h))
				shift++;
		}
		if (shift > 2) {
			pr_info("dcam binning should limit to 1/4\n");
			shift = 2;
		}
		if (shift > module->binning_limit) {
			pr_info("bin shift limit to %d\n", module->binning_limit);
			shift = module->binning_limit;
		}

		if (shift == 2) {
			/* make sure output 2 aligned and trim invalid */
			pr_debug("shift 2 trim_pv %d %d %d %d\n",
				trim_pv.start_x, trim_pv.start_y,
				trim_pv.size_x, trim_pv.size_y);
			if ((trim_pv.size_x >> 2) & 1) {
				trim_pv.size_x = (trim_pv.size_x + 4) & ~7;
				if ((trim_pv.start_x + trim_pv.size_x) > src_p.w)
					trim_pv.size_x -= 8;
				trim_pv.start_x = (src_p.w - trim_pv.size_x) >> 1;
			}
			if ((trim_pv.size_y >> 2) & 1) {
				trim_pv.size_y = (trim_pv.size_y + 4) & ~7;
				if ((trim_pv.start_y + trim_pv.size_y) > src_p.h)
					trim_pv.size_y -= 8;
				trim_pv.start_y = (src_p.h - trim_pv.size_y) >> 1;
			}
			pr_debug("shift 2 trim_pv final %d %d %d %d\n",
				trim_pv.start_x, trim_pv.start_y,
				trim_pv.size_x, trim_pv.size_y);
		}

		if (shift == 1)
			align_size = 8;
		else if (shift == 2)
			align_size = 16;
		else
			align_size = 4;

		trim_pv.size_x = ALIGN_DOWN(trim_pv.size_x, align_size);
		trim_pv.size_y = ALIGN_DOWN(trim_pv.size_y, align_size / 2);

		dcam_out.w = (trim_pv.size_x >> shift);
		dcam_out.w = ALIGN_DOWN(dcam_out.w, 2);
		dcam_out.h = (trim_pv.size_y >> shift);
		dcam_out.h = ALIGN_DOWN(dcam_out.h, 2);

		if (bypass_always == ZOOM_SCALER) {
			ratio_min = 1 << RATIO_SHIFT;
			if (trim_pv.size_x > ch_prev->swap_size.w || trim_pv.size_y > ch_prev->swap_size.h) {
				ratio_w = (1 << RATIO_SHIFT) * trim_pv.size_x / ch_prev->swap_size.w;
				ratio_h = (1 << RATIO_SHIFT) * trim_pv.size_y / ch_prev->swap_size.h;
				ratio_min = MAX(ratio_w, ratio_h);
				ratio_min = MAX(ratio_min, 1 << RATIO_SHIFT);
				dcam_out.w = camcore_ratio16_divide(trim_pv.size_x, ratio_min);
				dcam_out.h = camcore_ratio16_divide(trim_pv.size_y, ratio_min);
				dcam_out.w = ALIGN(dcam_out.w, 4);
				dcam_out.h = ALIGN(dcam_out.h, 2);
			}
		}

		if (dcam_out.w > DCAM_SCALER_MAX_WIDTH) {
			dcam_out.h = dcam_out.h * DCAM_SCALER_MAX_WIDTH / dcam_out.w;
			dcam_out.h = ALIGN_DOWN(dcam_out.h, 2);
			dcam_out.w = DCAM_SCALER_MAX_WIDTH;
			ratio_min = (1 << RATIO_SHIFT) * trim_pv.size_x / dcam_out.w;
		}

		if (ch_prev->compress_input)
			dcam_out.h = ALIGN_DOWN(dcam_out.h, DCAM_FBC_TILE_HEIGHT);

		pr_info("shift %d, dst_p %u %u, dst_v %u %u, dcam_out %u %u swap w:%d h:%d\n",
			shift, dst_p.w, dst_p.h, dst_v.w, dst_v.h, dcam_out.w, dcam_out.h, ch_prev->swap_size.w, ch_prev->swap_size.h);

		/* applied latest rect for aem */
		module->zoom_ratio = ZOOM_RATIO_DEFAULT * ch_prev->ch_uinfo.zoom_ratio_base.w / crop_p->w;
		ch_prev->trim_dcam = trim_pv;

		total_crop_dst = *total_crop_p;
		total_trim_pv.size_x = total_crop_dst.w;
		total_trim_pv.size_y = total_crop_dst.h;
		ch_prev->total_trim_dcam = total_trim_pv;

		ch_prev->rds_ratio = ((1 << shift) << RATIO_SHIFT);
		ch_prev->dst_dcam = dcam_out;

		isp_trim = &ch_prev->trim_isp;
		if (bypass_always == ZOOM_SCALER) {
			isp_trim->size_x =
				camcore_ratio16_divide(ch_prev->ch_uinfo.src_crop.w, ratio_min);
			isp_trim->size_y =
				camcore_ratio16_divide(ch_prev->ch_uinfo.src_crop.h, ratio_min);
			isp_trim->size_x = ALIGN(isp_trim->size_x, 4);
			isp_trim->size_y = ALIGN(isp_trim->size_y, 2);
		} else {
			isp_trim->size_x = ((ch_prev->ch_uinfo.src_crop.w >> shift) + 1) & ~1;
			isp_trim->size_y = ((ch_prev->ch_uinfo.src_crop.h >> shift) + 1) & ~1;
		}
		isp_trim->size_x = min(isp_trim->size_x, dcam_out.w);
		isp_trim->size_y = min(isp_trim->size_y, dcam_out.h);
		isp_trim->start_x = ((dcam_out.w - isp_trim->size_x) >> 1) & ~1;
		isp_trim->start_y = ((dcam_out.h - isp_trim->size_y) >> 1) & ~1;
		pr_info("trim isp, prev %u %u %u %u\n",
			isp_trim->start_x, isp_trim->start_y,
			isp_trim->size_x, isp_trim->size_y);
	}

	if (ch_vid->enable) {
		ch_vid->dst_dcam = dcam_out;
		ch_vid->trim_dcam = trim_pv;
		isp_trim = &ch_vid->trim_isp;
		if (bypass_always == ZOOM_SCALER) {
			isp_trim->size_x =
				camcore_ratio16_divide(ch_vid->ch_uinfo.src_crop.w, ratio_min);
			isp_trim->size_y =
				camcore_ratio16_divide(ch_vid->ch_uinfo.src_crop.h, ratio_min);
			isp_trim->size_x = ALIGN(isp_trim->size_x, 4);
			isp_trim->size_y = ALIGN(isp_trim->size_y, 2);
		} else {
			isp_trim->size_x = ((ch_vid->ch_uinfo.src_crop.w >> shift) + 1) & ~1;
			isp_trim->size_y = ((ch_vid->ch_uinfo.src_crop.h >> shift) + 1) & ~1;
		}
		isp_trim->size_x = min(isp_trim->size_x, dcam_out.w);
		isp_trim->size_y = min(isp_trim->size_y, dcam_out.h);
		isp_trim->start_x = ((dcam_out.w - isp_trim->size_x) >> 1) & ~1;
		isp_trim->start_y = ((dcam_out.h - isp_trim->size_y) >> 1) & ~1;
		pr_info("trim isp, vid %u %u %u %u\n",
			isp_trim->start_x, isp_trim->start_y,
			isp_trim->size_x, isp_trim->size_y);
	}

	if (ch_cap->enable) {
		ch_cap->trim_dcam = trim_c;
		camcore_diff_trim_get(&ch_cap->ch_uinfo.src_crop,
			(1 << RATIO_SHIFT), &trim_c, &ch_cap->trim_isp);
		ch_cap->trim_isp.start_x &= ~1;
		ch_cap->trim_isp.start_y &= ~1;
		ch_cap->trim_isp.size_x &= ~1;
		ch_cap->trim_isp.size_y &= ~1;
		if (ch_cap->trim_isp.size_x > trim_c.size_x)
			ch_cap->trim_isp.size_x = trim_c.size_x;
		if (ch_cap->trim_isp.size_y > trim_c.size_y)
			ch_cap->trim_isp.size_y = trim_c.size_y;
		pr_info("trim isp, cap %d %d %d %d\n",
			ch_cap->trim_isp.start_x, ch_cap->trim_isp.start_y,
			ch_cap->trim_isp.size_x, ch_cap->trim_isp.size_y);
		if (ch_cap_thm->enable) {
			ch_cap_thm->trim_dcam = ch_cap->trim_dcam;
			ch_cap_thm->trim_isp = ch_cap->trim_isp;
		}
	}

	pr_info("done\n");
	return 0;
}

static int camcore_channel_size_rds_cal(struct camera_module *module)
{
	uint32_t ratio_min, is_same_fov = 0;
	uint32_t ratio_p_w, ratio_p_h;
	uint32_t ratio_v_w, ratio_v_h;
	uint32_t ratio16_w, ratio16_h;
	uint32_t align_w, align_h;
	struct channel_context *ch_prev;
	struct channel_context *ch_vid;
	struct channel_context *ch_cap;
	struct sprd_img_rect *crop_p, *crop_v, *crop_c;
	struct sprd_img_rect crop_dst;
	struct img_trim trim_pv = {0};
	struct img_trim trim_c = {0};
	struct img_size src_p, dst_p, dst_v, dcam_out;
	struct cam_hw_info *hw = NULL;

	ch_prev = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
	if (!ch_prev->enable && !ch_cap->enable && !ch_vid->enable)
		return 0;

	dst_p.w = dst_p.h = 1;
	dst_v.w = dst_v.h = 1;
	ratio_v_w = ratio_v_h = 1;
	crop_p = crop_v = crop_c = NULL;
	if (ch_prev->enable) {
		src_p.w = ch_prev->ch_uinfo.src_size.w;
		src_p.h = ch_prev->ch_uinfo.src_size.h;
		crop_p = &ch_prev->ch_uinfo.src_crop;
		dst_p.w = ch_prev->ch_uinfo.dst_size.w;
		dst_p.h = ch_prev->ch_uinfo.dst_size.h;
		pr_info("src crop prev %u %u %u %u\n",
			crop_p->x, crop_p->y, crop_p->w, crop_p->h);
	}

	if (ch_vid->enable) {
		crop_v = &ch_vid->ch_uinfo.src_crop;
		dst_v.w = ch_vid->ch_uinfo.dst_size.w;
		dst_v.h = ch_vid->ch_uinfo.dst_size.h;
		pr_info("src crop vid %u %u %u %u\n",
			crop_v->x, crop_v->y, crop_v->w, crop_v->h);
	}

	if (ch_cap->enable && ch_prev->enable &&
		(ch_cap->mode_ltm == MODE_LTM_PRE)) {
		crop_c = &ch_cap->ch_uinfo.src_crop;
		is_same_fov = 1;
		pr_info("src crop cap %d %d %d %d\n", crop_c->x, crop_c->y,
			crop_c->x + crop_c->w, crop_c->y + crop_c->h);
	}

	if (ch_prev->enable) {
		crop_dst = *crop_p;
		camcore_largest_crop_get(&crop_dst, crop_v);
		camcore_largest_crop_get(&crop_dst, crop_c);
		trim_pv.start_x = crop_dst.x;
		trim_pv.start_y = crop_dst.y;
		trim_pv.size_x = crop_dst.w;
		trim_pv.size_y = crop_dst.h;
	}

	if (is_same_fov)
		trim_c = trim_pv;
	else if (ch_cap->enable) {
		trim_c.start_x = ch_cap->ch_uinfo.src_crop.x & ~1;
		trim_c.start_y = ch_cap->ch_uinfo.src_crop.y & ~1;
		trim_c.size_x = (ch_cap->ch_uinfo.src_crop.w + 1) & ~1;
		trim_c.size_y = (ch_cap->ch_uinfo.src_crop.h + 1) & ~1;

		if (ch_cap->compress_input) {
			uint32_t aligned_y;

			aligned_y = ALIGN_DOWN(trim_c.start_y, 4);
			trim_c.size_y += trim_c.start_y - aligned_y;
			trim_c.start_y = aligned_y;
		}
	}

	pr_info("trim_pv: %u %u %u %u\n", trim_pv.start_x, trim_pv.start_y,
		trim_pv.size_x, trim_pv.size_y);
	pr_info("trim_c: %u %u %u %u\n", trim_c.start_x, trim_c.start_y,
		trim_c.size_x, trim_c.size_y);

	if (ch_prev->enable) {
		ratio_min = 1 << RATIO_SHIFT;
		if (module->zoom_solution >= ZOOM_RDS) {
			ratio_p_w = (1 << RATIO_SHIFT) * crop_p->w / dst_p.w;
			ratio_p_h = (1 << RATIO_SHIFT) * crop_p->h / dst_p.h;
			ratio_min = MIN(ratio_p_w, ratio_p_h);
			if (ch_vid->enable) {
				ratio_v_w = (1 << RATIO_SHIFT) * crop_v->w / dst_v.w;
				ratio_v_h = (1 << RATIO_SHIFT) * crop_v->h / dst_v.h;
				ratio_min = MIN(ratio_min, MIN(ratio_v_w, ratio_v_h));
			}
			ratio_min = MIN(ratio_min, ((module->rds_limit << RATIO_SHIFT) / 10));
			ratio_min = MAX(ratio_min, (1 << RATIO_SHIFT));
			pr_info("ratio_p %d %d, ratio_v %d %d ratio_min %d\n",
				ratio_p_w, ratio_p_h, ratio_v_w, ratio_v_h, ratio_min);
		}

		/* align bin path output size */
		align_w = align_h = DCAM_RDS_OUT_ALIGN;
		align_w = MAX(align_w, DCAM_OUTPUT_DEBUG_ALIGN);
		dcam_out.w = camcore_ratio16_divide(trim_pv.size_x, ratio_min);
		dcam_out.h = camcore_ratio16_divide(trim_pv.size_y, ratio_min);

		/* avoid isp fetch fbd timeout when isp src width > 1856 */
		hw = module->dcam_dev_handle->hw;
		if ((dcam_out.w > ISP_FBD_MAX_WIDTH) && (hw->prj_id == SHARKL5pro))
			ch_prev->compress_input = 0;

		if (ch_prev->compress_input)
			align_h = MAX(align_h, DCAM_FBC_TILE_HEIGHT);

		dcam_out.w = ALIGN(dcam_out.w, align_w);
		dcam_out.h = ALIGN(dcam_out.h, align_h);

		/* keep same ratio between width and height */
		ratio16_w = (uint32_t)div_u64((uint64_t)trim_pv.size_x << RATIO_SHIFT, dcam_out.w);
		ratio16_h = (uint32_t)div_u64((uint64_t)trim_pv.size_y << RATIO_SHIFT, dcam_out.h);
		ratio_min = min(ratio16_w, ratio16_h);

		/* if sensor size is too small */
		if (src_p.w < dcam_out.w || src_p.h < dcam_out.h) {
			dcam_out.w = src_p.w;
			dcam_out.h = src_p.h;
			if (ch_prev->compress_input)
				dcam_out.h = ALIGN_DOWN(dcam_out.h,
							DCAM_FBC_TILE_HEIGHT);
			ratio_min = 1 << RATIO_SHIFT;
		}

		if ((1 << RATIO_SHIFT) >= ratio_min) {
			/* enlarge @trim_pv and crop it in isp */
			uint32_t align = 2;/* TODO set to 4 for zzhdr */

			trim_pv.size_x = max(trim_pv.size_x, dcam_out.w);
			trim_pv.size_y = max(trim_pv.size_y, dcam_out.h);
			trim_pv.size_x = ALIGN(trim_pv.size_x, align >> 1);
			trim_pv.size_y = ALIGN(trim_pv.size_y, align >> 1);
			if (src_p.w >= trim_pv.size_x)
				trim_pv.start_x = (src_p.w - trim_pv.size_x) >> 1;
			else
				trim_pv.start_x = 0;
			if (src_p.h >= trim_pv.size_y)
				trim_pv.start_y = (src_p.h - trim_pv.size_y) >> 1;
			else
				trim_pv.start_y = 0;
			trim_pv.start_x = ALIGN_DOWN(trim_pv.start_x, align);
			trim_pv.start_y = ALIGN_DOWN(trim_pv.start_y, align);

			ratio_min = 1 << RATIO_SHIFT;
			pr_info("trim_pv aligned %u %u %u %u\n",
				trim_pv.start_x, trim_pv.start_y,
				trim_pv.size_x, trim_pv.size_y);
		} else {
			dcam_out.w = camcore_ratio16_divide(trim_pv.size_x, ratio_min);
			dcam_out.h = camcore_ratio16_divide(trim_pv.size_y, ratio_min);
			dcam_out.w = ALIGN(dcam_out.w, align_w);
			dcam_out.h = ALIGN(dcam_out.h, align_h);
		}

		/* check rds out limit if rds used */
		if (dcam_out.w > DCAM_RDS_OUT_LIMIT) {
			dcam_out.w = DCAM_RDS_OUT_LIMIT;
			dcam_out.h = dcam_out.w * trim_pv.size_y / trim_pv.size_x;
			dcam_out.w = ALIGN(dcam_out.w, align_w);
			dcam_out.h = ALIGN(dcam_out.h, align_h);

			/* keep same ratio between width and height */
			ratio16_w = (uint32_t)div_u64((uint64_t)trim_pv.size_x << RATIO_SHIFT, dcam_out.w);
			ratio16_h = (uint32_t)div_u64((uint64_t)trim_pv.size_y << RATIO_SHIFT, dcam_out.h);
			ratio_min = min(ratio16_w, ratio16_h);
		}

		pr_info("dst_p %u %u, dst_v %u %u, dcam_out %u %u, ratio %u\n",
			dst_p.w, dst_p.h, dst_v.w, dst_v.h,
			dcam_out.w, dcam_out.h, ratio_min);

		/* applied latest rect for aem */
		module->zoom_ratio = src_p.w * ZOOM_RATIO_DEFAULT / crop_p->w;
		ch_prev->trim_dcam = trim_pv;
		ch_prev->rds_ratio = ratio_min;/* rds_ratio is not used */
		ch_prev->dst_dcam = dcam_out;

		ch_prev->trim_isp.size_x =
			camcore_ratio16_divide(ch_prev->ch_uinfo.src_crop.w, ratio_min);
		ch_prev->trim_isp.size_y =
			camcore_ratio16_divide(ch_prev->ch_uinfo.src_crop.h, ratio_min);
		ch_prev->trim_isp.size_x = min(ch_prev->trim_isp.size_x, dcam_out.w);
		ch_prev->trim_isp.size_y = min(ch_prev->trim_isp.size_y, dcam_out.h);
		ch_prev->trim_isp.start_x =
			(dcam_out.w - ch_prev->trim_isp.size_x) >> 1;
		ch_prev->trim_isp.start_y =
			(dcam_out.h - ch_prev->trim_isp.size_y) >> 1;

		pr_info("trim isp, prev %u %u %u %u\n",
			ch_prev->trim_isp.start_x, ch_prev->trim_isp.start_y,
			ch_prev->trim_isp.size_x, ch_prev->trim_isp.size_y);

		if (ch_vid->enable) {
			ch_vid->trim_isp.size_x =
				camcore_ratio16_divide(ch_vid->ch_uinfo.src_crop.w, ratio_min);
			ch_vid->trim_isp.size_y =
				camcore_ratio16_divide(ch_vid->ch_uinfo.src_crop.h, ratio_min);
			ch_vid->trim_isp.size_x = min(ch_vid->trim_isp.size_x, dcam_out.w);
			ch_vid->trim_isp.size_y = min(ch_vid->trim_isp.size_y, dcam_out.h);
			ch_vid->trim_isp.start_x =
				(dcam_out.w - ch_vid->trim_isp.size_x) >> 1;
			ch_vid->trim_isp.start_y =
				(dcam_out.h - ch_vid->trim_isp.size_y) >> 1;

			pr_info("trim isp, vid %d %d %d %d\n",
				ch_vid->trim_isp.start_x, ch_vid->trim_isp.start_y,
				ch_vid->trim_isp.size_x, ch_vid->trim_isp.size_y);
		}
	}

	if (ch_cap->enable) {
		ch_cap->trim_dcam = trim_c;
		camcore_diff_trim_get(&ch_cap->ch_uinfo.src_crop,
			(1 << RATIO_SHIFT), &trim_c, &ch_cap->trim_isp);
		ch_cap->trim_isp.start_x &= ~1;
		ch_cap->trim_isp.start_y &= ~1;
		ch_cap->trim_isp.size_x &= ~1;
		ch_cap->trim_isp.size_y &= ~1;
		if (ch_cap->trim_isp.size_x > trim_c.size_x)
			ch_cap->trim_isp.size_x = trim_c.size_x;
		if (ch_cap->trim_isp.size_y > trim_c.size_y)
			ch_cap->trim_isp.size_y = trim_c.size_y;
		pr_info("trim isp, cap %d %d %d %d\n",
			ch_cap->trim_isp.start_x, ch_cap->trim_isp.start_y,
			ch_cap->trim_isp.size_x, ch_cap->trim_isp.size_y);
	}

	pr_info("done.\n");

	return 0;
}

static int camcore_channel_bigsize_config(
	struct camera_module *module,
	struct channel_context *channel)
{
	int ret = 0;
	int i = 0, total = 0, iommu_enable = 0;
	uint32_t width = 0, height = 0, pack_bits = 0, size = 0, is_pack = 0, dcam_out_bits = 0;
	struct camera_uchannel *ch_uinfo = NULL;
	struct dcam_path_cfg_param ch_desc;
	struct camera_frame *pframe = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct dcam_path_desc *path = NULL;
	struct dcam_compress_info fbc_info;
	struct dcam_compress_cal_para cal_fbc = {0};

	ch_uinfo = &channel->ch_uinfo;
	iommu_enable = module->iommu_enable;
	width = channel->swap_size.w;
	height = channel->swap_size.h;

	pack_bits = channel->ch_uinfo.dcam_raw_fmt;
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	path = &dcam_sw_aux_ctx->path[channel->aux_dcam_path_id];
	dcam_out_bits = channel->ch_uinfo.dcam_output_bit;
	is_pack = channel->ch_uinfo.dcam_out_pack;

	if (channel->compress_offline) {
		cal_fbc.compress_4bit_bypass = channel->compress_4bit_bypass;
		cal_fbc.data_bits = dcam_out_bits;
		cal_fbc.fbc_info = &fbc_info;
		cal_fbc.fmt = channel->dcam_out_fmt;
		cal_fbc.height = height;
		cal_fbc.width = width;
		size = dcam_if_cal_compressed_size (&cal_fbc);
	} else if (path->out_fmt & DCAM_STORE_RAW_BASE)
		size = cal_sprd_raw_pitch(width, pack_bits) * height;
	else if (path->out_fmt == DCAM_STORE_YUV420 || path->out_fmt == DCAM_STORE_YVU420)
		size = cal_sprd_yuv_pitch(width, dcam_out_bits, is_pack) * height * 3 / 2;

	size = ALIGN(size, CAM_BUF_ALIGN_SIZE);

	/* dcam1 alloc memory */
	total = 5;

	/* non-zsl capture for single frame */
	if (channel->ch_id == CAM_CH_CAP &&
		module->channel[CAM_CH_PRE].enable == 0 &&
		module->channel[CAM_CH_VID].enable == 0)
		total = 1;

	pr_info("ch %d alloc shared buffer size: %u (w %u h %u), num %d\n",
		channel->ch_id, size, width, height, total);

	for (i = 0; i < total; i++) {
		do {
			pframe = cam_queue_empty_frame_get();
			pframe->channel_id = channel->ch_id;
			pframe->is_compressed = channel->compress_offline;
			pframe->compress_4bit_bypass =
					channel->compress_4bit_bypass;
			pframe->width = width;
			pframe->height = height;
			pframe->endian = ENDIAN_LITTLE;
			pframe->pattern = module->cam_uinfo.sensor_if.img_ptn;
			pframe->buf.buf_sec = 0;
			if (module->cam_uinfo.is_pyr_rec && channel->ch_id != CAM_CH_CAP)
				pframe->need_pyr_rec = 1;
			if (module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP)
				pframe->need_pyr_dec = 1;
			ret = cam_buf_alloc(&pframe->buf, size, iommu_enable);
			if (ret) {
				pr_err("fail to alloc buf: %d ch %d\n",
					i, channel->ch_id);
				cam_queue_empty_frame_put(pframe);
				atomic_inc(&channel->err_status);
				break;
			}

			/* cfg aux_dcam out_buf */
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->aux_dcam_path_id,
				pframe);
		} while (0);
	}

	/* dcam1 cfg path size */
	memset(&ch_desc, 0, sizeof(ch_desc));
	ch_desc.input_size.w = ch_uinfo->src_size.w;
	ch_desc.input_size.h = ch_uinfo->src_size.h;
	ch_desc.output_size = ch_desc.input_size;
	ch_desc.input_trim.start_x = 0;
	ch_desc.input_trim.start_y = 0;
	ch_desc.input_trim.size_x = ch_desc.input_size.w;
	ch_desc.input_trim.size_y = ch_desc.input_size.h;
	ch_desc.raw_fmt= pack_bits;
	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
	if (module->cam_uinfo.is_pyr_dec) {
		ch_desc.input_size.w = ch_uinfo->src_crop.w;
		ch_desc.input_size.h = ch_uinfo->src_crop.h;
		ch_desc.input_trim.start_x= 0;
		ch_desc.input_trim.start_y= 0;
		ch_desc.input_trim.size_x= ch_uinfo->src_crop.w;
		ch_desc.input_trim.size_y= ch_uinfo->src_crop.h;
		ch_desc.output_size.w = ch_uinfo->src_crop.w;
		ch_desc.output_size.h = ch_uinfo->src_crop.h;
	}

	pr_info("update dcam path %d size for channel %d packbit %d 4in1 %d\n",
		channel->aux_dcam_path_id, channel->ch_id, ch_desc.raw_fmt, ch_desc.is_4in1);

	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SIZE, channel->aux_dcam_path_id, &ch_desc);

	pr_info("update channel size done for CAP\n");
	return ret;
}

static int camcore_pyr_info_config(
	struct camera_module *module,
	struct channel_context *channel)
{
	int ret = 0;
	uint32_t is_pyr_rec = 0;
	uint32_t pyr_layer_num = 0;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	if (module->cam_uinfo.is_pyr_rec && (!channel->ch_uinfo.is_high_fps))
		is_pyr_rec = 1;
	else
		is_pyr_rec = 0;
	module->dcam_dev_handle->dcam_pipe_ops->ioctl(dcam_sw_ctx,
		DCAM_IOCTL_CFG_PYR_DEC_EN, &is_pyr_rec);

	if (module->cam_uinfo.is_pyr_rec && channel->ch_id == CAM_CH_PRE) {
		channel->pyr_layer_num = DCAM_PYR_DEC_LAYER_NUM;
		pyr_layer_num = DCAM_PYR_DEC_LAYER_NUM;
	}

	if (module->cam_uinfo.is_pyr_dec && channel->ch_id == CAM_CH_CAP) {
		channel->pyr_layer_num = ISP_PYR_DEC_LAYER_NUM;
		pyr_layer_num = ISP_PYR_DEC_LAYER_NUM;
		if (module->cam_uinfo.is_raw_alg)
			module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
				channel->isp_fdrh_ctx, ISP_IOCTL_CFG_PYR_REC_NUM,
				&pyr_layer_num);
	}

	if (channel->ch_id < CAM_CH_PRE_THM)
		module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
				channel->isp_ctx_id, ISP_IOCTL_CFG_PYR_REC_NUM,
				&pyr_layer_num);

	return ret;
}

static int camcore_vir_channel_config(
	struct camera_module *module,
	struct channel_context *channel)
{
	int ret = 0;
	struct camera_uchannel *ch_uinfo = NULL;
	struct isp_path_base_desc path_desc;
	ch_uinfo = &channel->ch_uinfo;

	/* pre path config */
	ret = module->isp_dev_handle->isp_ops->get_path(module->isp_dev_handle,
		channel->isp_ctx_id, channel->isp_path_id);
	if (ret < 0) {
		pr_err("fail to get isp path %d from context %d\n",
			channel->isp_path_id, channel->isp_ctx_id);
		return ret;
	}

	memset(&path_desc, 0, sizeof(path_desc));
	path_desc.slave_type = ISP_PATH_SLAVE;
	path_desc.out_fmt = ch_uinfo->slave_img_fmt;
	path_desc.endian.y_endian = ENDIAN_LITTLE;
	path_desc.endian.uv_endian = ENDIAN_LITTLE;
	path_desc.output_size.w = ch_uinfo->vir_channel[0].dst_size.w;
	path_desc.output_size.h = ch_uinfo->vir_channel[0].dst_size.h;
	path_desc.data_bits = ch_uinfo->vir_channel[0].dump_isp_out_fmt;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_PATH_BASE, channel->isp_ctx_id, channel->isp_path_id, &path_desc);

	/* cap path config */
	ret = module->isp_dev_handle->isp_ops->get_path(module->isp_dev_handle,
		channel->slave_isp_ctx_id, channel->isp_path_id);
	if (ret < 0) {
		pr_err("fail to get isp path %d from context %d\n",
			channel->isp_path_id, channel->slave_isp_ctx_id);
		return ret;
	}

	path_desc.output_size.w = ch_uinfo->vir_channel[1].dst_size.w;
	path_desc.output_size.h = ch_uinfo->vir_channel[1].dst_size.h;
	path_desc.data_bits = ch_uinfo->vir_channel[1].dump_isp_out_fmt;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_PATH_BASE, channel->slave_isp_ctx_id, channel->isp_path_id, &path_desc);

	return 0;
}

static int camcore_channel_size_config(
	struct camera_module *module,
	struct channel_context *channel)
{
	int ret = 0;
	int is_zoom, loop_count;
	uint32_t isp_ctx_id, isp_path_id;
	struct isp_offline_param *isp_param = NULL;
	struct channel_context *vid;
	struct camera_uchannel *ch_uinfo = NULL;
	struct cam_hw_info *hw = NULL;
	struct dcam_path_cfg_param ch_desc;
	struct isp_ctx_size_desc ctx_size;
	struct img_trim path_trim;
	struct camera_frame *alloc_buf = NULL;
	struct channel_context *ch_pre = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	if (!module || !channel) {
		pr_err("fail to get valid param %p %p\n", module, channel);
		return -EFAULT;
	}

	if (atomic_read(&module->state) == CAM_RUNNING) {
		is_zoom = 1;
		loop_count = 8;
	} else if (atomic_read(&module->state) == CAM_STREAM_ON) {
		is_zoom = 0;
		loop_count = 1;
	} else {
		pr_warn("warning: cam%d state:%d\n", module->idx, atomic_read(&module->state));
		return 0;
	}

	hw = module->grp->hw_info;
	ch_uinfo = &channel->ch_uinfo;
	ch_pre = &module->channel[CAM_CH_PRE];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	if (!is_zoom && (channel->ch_id != CAM_CH_RAW))
		camcore_pyr_info_config(module, channel);
	/* DCAM full path not updating for zoom. */
	if (is_zoom && channel->ch_id == CAM_CH_CAP && !module->cam_uinfo.is_pyr_dec)
		goto cfg_isp;

	if (!is_zoom && (channel->swap_size.w > 0) && (channel->ch_id != CAM_CH_RAW)) {
		cam_queue_init(&channel->share_buf_queue,
			CAM_SHARED_BUF_NUM, camcore_k_frame_put);

		/* alloc middle buffer for channel */
		mutex_lock(&module->buf_lock[channel->ch_id]);
		channel->alloc_start = 1;
		mutex_unlock(&module->buf_lock[channel->ch_id]);

		alloc_buf = cam_queue_empty_frame_get();
		alloc_buf->priv_data = (void *)channel;
		cam_queue_enqueue(&module->alloc_queue, &alloc_buf->list);
		complete(&module->buf_thrd.thread_com);
	}

	memset(&ch_desc, 0, sizeof(ch_desc));
	ch_desc.input_size.w = ch_uinfo->src_size.w;
	ch_desc.input_size.h = ch_uinfo->src_size.h;
	ch_desc.zoom_ratio_base = ch_uinfo->zoom_ratio_base;
	if ((channel->ch_id == CAM_CH_CAP) || (channel->ch_id == CAM_CH_RAW)) {
		/* PYR_DEC: crop by dcam; Normal:no trim in dcam full path. */
		if (camcore_capture_sizechoice(module, channel)) {
			ch_desc.input_trim = channel->trim_dcam;
			ch_desc.output_size.w = channel->trim_dcam.size_x;
			ch_desc.output_size.h = channel->trim_dcam.size_y;
		} else {
			ch_desc.output_size = ch_desc.input_size;
			ch_desc.input_trim.start_x = 0;
			ch_desc.input_trim.start_y = 0;
			ch_desc.input_trim.size_x = ch_desc.input_size.w;
			ch_desc.input_trim.size_y = ch_desc.input_size.h;
		}
	} else {
		if (channel->rds_ratio & ((1 << RATIO_SHIFT) - 1))
			ch_desc.force_rds = 1;
		else
			ch_desc.force_rds = 0;
		ch_desc.input_trim = channel->trim_dcam;
		ch_desc.total_input_trim = channel->total_trim_dcam;
		ch_desc.output_size = channel->dst_dcam;
	}

	pr_info("update dcam path %d size for channel %d\n", channel->dcam_path_id, channel->ch_id);

	if (channel->ch_id == CAM_CH_PRE || channel->ch_id == CAM_CH_VID) {
		isp_param = kzalloc(sizeof(struct isp_offline_param), GFP_KERNEL);
		if (isp_param == NULL) {
			pr_err("fail to alloc memory.\n");
			return -ENOMEM;
		}
		ch_desc.priv_size_data = (void *)isp_param;
		isp_param->valid |= ISP_SRC_SIZE;
		isp_param->src_info.src_size = ch_desc.input_size;
		isp_param->src_info.src_trim = ch_desc.input_trim;
		isp_param->src_info.dst_size = ch_desc.output_size;
		isp_param->valid |= ISP_PATH0_TRIM;
		isp_param->trim_path[0] = channel->trim_isp;
		vid = &module->channel[CAM_CH_VID];
		if (vid->enable) {
			isp_param->valid |= ISP_PATH1_TRIM;
			isp_param->trim_path[1] = vid->trim_isp;
		}
		pr_debug("isp_param %p\n", isp_param);
	}

	do {
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_SIZE, channel->dcam_path_id, &ch_desc);
		if (ret) {
			/* todo: if previous updating is not applied yet.
			 * this case will happen.
			 * (zoom ratio changes in short gap)
			 * wait here and retry(how long?)
			 */
			pr_info("wait to update dcam path %d size, zoom %d, lp %d\n", channel->dcam_path_id, is_zoom, loop_count);
			msleep(20);
		} else {
			break;
		}
	} while (--loop_count);

	if (channel->ch_id == CAM_CH_RAW)
		return ret;
	if (ret && ch_desc.priv_size_data) {
		kfree(ch_desc.priv_size_data);
		ch_desc.priv_size_data = NULL;
		isp_param = NULL;
	}
	if (!is_zoom && (channel->ch_id == CAM_CH_PRE ||
		(!ch_pre->enable && channel->ch_id == CAM_CH_VID))) {
		isp_ctx_id = channel->isp_ctx_id;
		isp_path_id = channel->isp_path_id;
		ctx_size.src.w = channel->dst_dcam.w;
		ctx_size.src.h = channel->dst_dcam.h;
		ctx_size.crop.start_x = 0;
		ctx_size.crop.start_y = 0;
		ctx_size.crop.size_x = channel->dst_dcam.w;
		ctx_size.crop.size_y = channel->dst_dcam.h;
		ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_CTX_SIZE, isp_ctx_id, 0, &ctx_size);
		if (ret != 0)
			goto exit;
		path_trim = channel->trim_isp;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_SIZE,
			isp_ctx_id, isp_path_id, &path_trim);
		if (ret != 0)
			goto exit;
		if (vid->enable) {
			path_trim = vid->trim_isp;
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_SIZE,
				isp_ctx_id, ISP_SPATH_VID, &path_trim);
		}
	}

	/* isp path for prev/video will update from input frame. */
	if (channel->ch_id == CAM_CH_PRE) {
		pr_info("update channel size done for preview\n");
		return ret;
	}

cfg_isp:
	isp_ctx_id = channel->isp_ctx_id;
	isp_path_id = channel->isp_path_id;

	if (channel->ch_id == CAM_CH_CAP) {
		if (module->cam_uinfo.is_pyr_dec) {
			ctx_size.src.w = channel->trim_dcam.size_x;
			ctx_size.src.h = channel->trim_dcam.size_y;
			ctx_size.crop.start_x = 0;
			ctx_size.crop.start_y = 0;
			ctx_size.crop.size_x = ctx_size.src.w;
			ctx_size.crop.size_y = ctx_size.src.h;
			ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
			if (module->cam_uinfo.dcam_slice_mode) {
				ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
					ISP_PATH_CFG_CTX_SIZE, isp_ctx_id, 0, &ctx_size);
				if (ret != 0)
					goto exit;
			}
		} else {
			ctx_size.src.w = ch_uinfo->src_size.w;
			ctx_size.src.h = ch_uinfo->src_size.h;
			ctx_size.crop = channel->trim_dcam;
			ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_SIZE, isp_ctx_id, 0, &ctx_size);
			if (ret != 0)
				goto exit;
		}
		pr_debug("cfg isp sw %d size src w %d, h %d, crop %d %d %d %d\n",
			isp_ctx_id, ctx_size.src.w, ctx_size.src.h,
			ctx_size.crop.start_x, ctx_size.crop.start_y, ctx_size.crop.size_x, ctx_size.crop.size_y);
		if ((module->cam_uinfo.raw_alg_type > RAW_ALG_FDR_V1) &&
			(module->cam_uinfo.raw_alg_type < RAW_ALG_TYPE_MAX) &&
			module->fdr_init) {
			ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_SIZE, channel->isp_fdrh_ctx, 0, &ctx_size);
			if (ret != 0)
				goto exit;
		}
	}
	path_trim = channel->trim_isp;

cfg_path:
	pr_info("cfg isp ctx sw %d path %d size, path trim %d %d %d %d\n",
		isp_ctx_id, isp_path_id, path_trim.start_x, path_trim.start_y, path_trim.size_x, path_trim.size_y);
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_PATH_SIZE, isp_ctx_id, isp_path_id, &path_trim);
	if (ret != 0)
		goto exit;
	if ((module->cam_uinfo.raw_alg_type > RAW_ALG_FDR_V1) &&
		(module->cam_uinfo.raw_alg_type < RAW_ALG_TYPE_MAX) &&
		module->fdr_init) {
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_SIZE, channel->isp_fdrh_ctx, isp_path_id, &path_trim);
		if (ret != 0)
			goto exit;
	}
	if (channel->ch_id == CAM_CH_CAP && is_zoom) {
		channel = &module->channel[CAM_CH_CAP_THM];
		if (channel->enable) {
			isp_path_id = channel->isp_path_id;
			goto cfg_path;
		}
	}
	pr_info("update channel size done for CAP\n");

exit:
	if (isp_param != NULL) {
		kfree(isp_param);
		isp_param = NULL;
	}
	return ret;
}

/* set capture channel size for isp fetch and crop, scaler
 * lowlux: 1: size / 2, 0: full size
 */
static int camcore_4in1_channel_size_config(struct camera_module *module,
		uint32_t lowlux_flag)
{
	struct channel_context *ch;
	struct channel_context ch_tmp;
	struct camera_uchannel *p;

	ch  = &module->channel[CAM_CH_CAP];
	/* backup */
	memcpy(&ch_tmp, ch, sizeof(struct channel_context));
	p = &ch_tmp.ch_uinfo;
	if (lowlux_flag) {
		/* bin-sum image, size should /2 */
		p->src_size.w /= 2;
		p->src_size.h /= 2;
		if ((p->src_size.w & 0x1) || (p->src_size.h & 0x1))
			pr_warn("warning: Some problem with sensor size in lowlux\n");
		p->src_crop.x /= 2;
		p->src_crop.y /= 2;
		p->src_crop.w /= 2;
		p->src_crop.h /= 2;
		/* check zoom, low lux not support zoom now 190306 */
		if (p->src_crop.w != p->src_size.w ||
			p->src_crop.h != p->src_size.h) {
			pr_warn("warning: lowlux capture not support zoom now\n");
			p->src_crop.x = 0;
			p->src_crop.y = 0;
			p->src_crop.w = p->src_size.w;
			p->src_crop.h = p->src_size.h;
		}
		p->src_crop.x = ALIGN(p->src_crop.x, 2);
		p->src_crop.y = ALIGN(p->src_crop.y, 2);
		p->src_crop.w = ALIGN(p->src_crop.w, 2);
		p->src_crop.h = ALIGN(p->src_crop.h, 2);
	}
	pr_info("src[%d %d], crop[%d %d %d %d] dst[%d %d]\n",
		p->src_size.w, p->src_size.h,
		p->src_crop.x, p->src_crop.y, p->src_crop.w, p->src_crop.h,
		p->dst_size.w, p->dst_size.h);
	ch_tmp.trim_dcam.start_x = p->src_crop.x;
	ch_tmp.trim_dcam.start_y = p->src_crop.y;
	ch_tmp.trim_dcam.size_x = p->src_crop.w;
	ch_tmp.trim_dcam.size_y = p->src_crop.h;
	ch_tmp.swap_size.w = p->src_size.w;
	ch_tmp.swap_size.h = p->src_size.h;
	camcore_diff_trim_get(&ch_tmp.ch_uinfo.src_crop,
		(1 << RATIO_SHIFT), &ch_tmp.trim_dcam, &ch_tmp.trim_isp);
	pr_info("trim_isp[%d %d %d %d]\n", ch_tmp.trim_isp.start_x,
		ch_tmp.trim_isp.start_y, ch_tmp.trim_isp.size_x,
		ch_tmp.trim_isp.size_y);
	camcore_channel_size_config(module, &ch_tmp);

	return 0;
}

static int camcore_channels_size_init(struct camera_module *module)
{
	uint32_t format = module->cam_uinfo.sensor_if.img_fmt;
	/* bypass RDS if sensor output binning size for image quality */
	if (g_camctrl.dcam_zoom_mode >= ZOOM_DEBUG_DEFAULT)
		module->zoom_solution = g_camctrl.dcam_zoom_mode - ZOOM_DEBUG_DEFAULT;
	else if (module->grp->hw_info->ip_dcam[0]->dcam_zoom_mode == ZOOM_SCALER)
		module->zoom_solution = ZOOM_SCALER;
	else
		module->zoom_solution = g_camctrl.dcam_zoom_mode;
	module->rds_limit = g_camctrl.dcam_rds_limit;

	/* force binning as smaller as possible for security */
	if (module->grp->camsec_cfg.camsec_mode != SEC_UNABLE)
		module->zoom_solution = ZOOM_BINNING4;

	if (format == DCAM_CAP_MODE_YUV)
		module->zoom_solution = ZOOM_DEFAULT;

	camcore_channel_swapsize_cal(module);

	pr_info("zoom_solution %d, limit %d %d\n",
		module->zoom_solution,
		module->rds_limit, module->binning_limit);

	return 0;
}

uint32_t camcore_format_dcam_translate(enum dcam_store_format forcc)
{
	uint32_t dcam_format = 0;

	switch (forcc) {
	case DCAM_STORE_RAW_BASE:
		dcam_format = IMG_PIX_FMT_GREY;
		break;
	case DCAM_STORE_YUV420:
		dcam_format = IMG_PIX_FMT_NV12;
		break;
	case DCAM_STORE_YVU420:
		dcam_format = IMG_PIX_FMT_NV21;
		break;
	case DCAM_STORE_YVU422:
		dcam_format = IMG_PIX_FMT_YVYU;
		break;
	case DCAM_STORE_YUV422:
		dcam_format = IMG_PIX_FMT_YUYV;
		break;
	case DCAM_STORE_FRGB:
		dcam_format = IMG_PIX_FMT_RGB565;
		break;
	default:
		pr_err("fail to get common format %d\n", dcam_format);
	}

	return dcam_format;
}

/* for offline simulator */
static int camcore_channels_set(struct camera_module *module,
		struct sprd_dcam_path_size *in)
{
	int ret = 0;
	struct img_size swap_size, size0 = { 0, 0 };
	struct sprd_dcam_path_size param;
	struct channel_context *ch = NULL;
	struct channel_context *ch_pre = NULL, *ch_vid = NULL;

	memset(&param, 0, sizeof(struct sprd_dcam_path_size));
	pr_debug("cam%d simu %d\n", module->idx, module->simulator);

	ret = camcore_channels_size_init(module);
	if (module->zoom_solution == ZOOM_DEFAULT)
		camcore_channel_size_binning_cal(module, 1);
	else if (module->zoom_solution == ZOOM_BINNING2 ||
		module->zoom_solution == ZOOM_BINNING4)
		camcore_channel_size_binning_cal(module, 0);
	else if (module->zoom_solution == ZOOM_SCALER)
		camcore_channel_size_binning_cal(module, ZOOM_SCALER);
	else
		camcore_channel_size_rds_cal(module);

	camcore_compression_config(module);

	ch_pre = &module->channel[CAM_CH_PRE];
	if (ch_pre->enable) {
		swap_size = ch_pre->swap_size;
		ch_pre->swap_size = size0;
		cam_queue_init(&ch_pre->share_buf_queue,
			CAM_SHARED_BUF_NUM, camcore_k_frame_put);
		camcore_channel_size_config(module, ch_pre);
		ch_pre->swap_size = swap_size;
	}

	ch_vid = &module->channel[CAM_CH_VID];
	if (ch_vid->enable && !ch_pre->enable)
		camcore_channel_size_config(module, ch_vid);

	ch = &module->channel[CAM_CH_CAP];
	if (ch->enable) {
		swap_size = ch->swap_size;
		ch->swap_size = size0;
		cam_queue_init(&ch->share_buf_queue,
			CAM_SHARED_BUF_NUM, camcore_k_frame_put);
		camcore_channel_size_config(module, ch);
		ch->swap_size = swap_size;
	}

	param.dcam_in_w = module->cam_uinfo.sn_size.w;
	param.dcam_in_h = module->cam_uinfo.sn_size.h;
	param.pre_dst_w = ch_pre->swap_size.w;
	param.pre_dst_h = ch_pre->swap_size.h;
	param.vid_dst_w = ch_vid->swap_size.w;
	param.vid_dst_h = ch_vid->swap_size.h;
	param.dcam_out_w = ch->swap_size.w;
	param.dcam_out_h = ch->swap_size.w;
	*in = param;

	return ret;
}

static int camcore_aux_dcam_init(struct camera_module *module,
		struct channel_context *channel)
{
	int ret = 0;
	uint32_t dcam_path_id, opened = 0, newdcam = 0;
	struct dcam_pipe_dev *dcam = NULL;
	struct camera_group *grp = NULL;
	struct dcam_sw_context *dcam_sw = NULL;
	struct dcam_sw_context *dcam_aux_sw = NULL;
	struct dcam_path_cfg_param ch_desc;
	uint32_t dcam_idx = DCAM_HW_CONTEXT_0;

	if (!module) {
		pr_info("fail to get camera module.\n");
		return -1;
	}

	dcam = module->aux_dcam_dev;
	if (dcam) {
		pr_info("aux dcam%d already init\n", module->aux_dcam_id);
		return 0;
	}

	grp = module->grp;
	dcam_sw = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	if (module->paused) {
		dcam = module->dcam_dev_handle;
		module->aux_dcam_dev = dcam;
		module->aux_dcam_id = module->dcam_idx;
		pr_info("use current dcam%d: %px, cur_aux_sw_ctx_id: %d\n", module->dcam_idx, dcam, module->offline_cxt_id);
		goto get_path;
	}

	dcam = dcam_core_pipe_dev_get(grp->hw_info);
	if (IS_ERR_OR_NULL(dcam)) {
		pr_err("fail to get dcam\n");
		return -EFAULT;
	}
	dcam_aux_sw = &dcam->sw_ctx[module->offline_cxt_id];
	dcam_aux_sw->is_raw_alg = dcam_sw->is_raw_alg;
	module->aux_dcam_dev = dcam;
	for (; dcam_idx < DCAM_HW_CONTEXT_MAX; dcam_idx++) {
		if (dcam_idx != module->dcam_idx) {
			module->aux_dcam_id = dcam_idx;
			break;
		}
	}

	if (dcam == NULL || dcam_idx == DCAM_HW_CONTEXT_MAX) {
		pr_err("fail to get aux dcam\n");
		return -EFAULT;
	}

	pr_debug("New a aux_dcam, cur_aux_sw_ctx_id: %d, module->dcam_idx:%d\n",
		module->offline_cxt_id, dcam_aux_sw->hw_ctx_id);

	newdcam = 1;

	ret = module->dcam_dev_handle->dcam_pipe_ops->open(dcam);
	if (ret < 0) {
		pr_err("fail to open aux dcam dev\n");
		ret = -EFAULT;
		goto exit_dev;
	}
	opened = 1;

get_path:
	if (grp->hw_info->prj_id == QOGIRL6 || grp->hw_info->prj_id == SHARKL5pro ||
		grp->hw_info->prj_id == QOGIRN6pro || grp->hw_info->prj_id == QOGIRN6L)
		dcam_path_id = DCAM_PATH_FULL;
	else
		dcam_path_id = DCAM_PATH_BIN;

	ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_aux_sw, dcam_path_id);
	if (ret < 0) {
		pr_err("fail to get dcam path %d\n", dcam_path_id);
		ret = -EFAULT;
		goto exit_close;
	}
	channel->aux_dcam_path_id = dcam_path_id;
	pr_debug("get aux dcam path %d\n", dcam_path_id);

	dcam_aux_sw->pack_bits = DCAM_RAW_14;
	dcam_aux_sw->fetch.fmt = DCAM_STORE_RAW_BASE;
	/* cfg dcam_aux bin path */
	memset(&ch_desc, 0, sizeof(ch_desc));
	ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
	ch_desc.endian.y_endian = ENDIAN_LITTLE;
	ch_desc.raw_fmt = DCAM_RAW_14;
	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
	ch_desc.dcam_out_fmt = DCAM_STORE_YUV_BASE;
	ch_desc.pyr_data_bits = DCAM_STORE_10_BIT;
	ch_desc.dcam_out_bits = DCAM_STORE_10_BIT;
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(&dcam->sw_ctx[module->offline_cxt_id],
		DCAM_PATH_CFG_BASE, channel->aux_dcam_path_id, &ch_desc);

	if (grp->hw_info->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW &&
		(module->cam_uinfo.raw_alg_type == RAW_ALG_MFNR || module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2)) {
		dcam_path_id = DCAM_PATH_RAW;
		channel->aux_raw_path_id = dcam_path_id;
		module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_aux_sw, dcam_path_id);
		ch_desc.raw_src = BPC_RAW_SRC_SEL;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_aux_sw,
			DCAM_PATH_CFG_BASE, channel->aux_raw_path_id, &ch_desc);
	}

	ch_desc.input_size.w = channel->ch_uinfo.src_size.w;
	ch_desc.input_size.h = channel->ch_uinfo.src_size.h;
	ch_desc.input_trim.size_x = channel->ch_uinfo.src_size.w;
	ch_desc.input_trim.size_y = channel->ch_uinfo.src_size.h;
	ch_desc.output_size.w = ch_desc.input_trim.size_x;
	ch_desc.output_size.h = ch_desc.input_trim.size_y;
	if (module->cam_uinfo.raw_alg_type != RAW_ALG_AI_SFNR) {
		if (grp->hw_info->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW && module->cam_uinfo.raw_alg_type) {
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_aux_sw,
				DCAM_PATH_CFG_SIZE, channel->aux_raw_path_id, &ch_desc);
		} else {
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_aux_sw,
				DCAM_PATH_CFG_SIZE, channel->aux_dcam_path_id, &ch_desc);
		}
		dcam_aux_sw->is_raw_alg = module->cam_uinfo.is_raw_alg;
	}

	return ret;

exit_close:
	if (opened)
		module->dcam_dev_handle->dcam_pipe_ops->close(module->aux_dcam_dev);
exit_dev:
	if (newdcam)
		dcam_core_pipe_dev_put(module->aux_dcam_dev);
	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;
	return ret;
}

static int camcore_aux_dcam_deinit(struct camera_module *module)
{
	int ret = 0;
	int pause = DCAM_STOP;
	int32_t path_id;
	struct dcam_pipe_dev *dcam = NULL;
	struct dcam_sw_context *sw_ctx = NULL;

	pr_debug("cam%d, fdr dcam %px, main dcam %px\n", module->idx,
		module->aux_dcam_dev, module->dcam_dev_handle);

	dcam = module->aux_dcam_dev;
	if (dcam == NULL)
		return ret;

	if (dcam == module->dcam_dev_handle)
		pause = DCAM_PAUSE_OFFLINE;

	sw_ctx = &dcam->sw_ctx[module->offline_cxt_id];
	ret = module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, pause);

	path_id = module->channel[CAM_CH_CAP].aux_dcam_path_id;
	if (module->channel[CAM_CH_CAP].enable && path_id >= 0) {
		ret = module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx, path_id);
		module->channel[CAM_CH_CAP].aux_dcam_path_id = -1;
	}

	path_id = module->channel[CAM_CH_CAP].aux_raw_path_id;
	if (module->grp->hw_info->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW
		&& (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2 ||
		module->cam_uinfo.raw_alg_type == RAW_ALG_MFNR) && path_id >= 0) {
		module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx, path_id);
		module->channel[CAM_CH_CAP].aux_raw_path_id = -1;
	}

	if (!module->paused) {
		pr_debug("close and put dcam %d\n", module->aux_dcam_id);
		ret += module->dcam_dev_handle->dcam_pipe_ops->close(dcam);
		ret += dcam_core_pipe_dev_put(dcam);
	}

	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;
	pr_info("Done, ret = %d\n", ret);

	return ret;
}

static int camcore_dcam_pmbuf_init(struct dcam_sw_context *sw_ctx)
{
	int ret = 0;
	uint32_t i = 0, iommu_enable = 0 ;
	struct camera_frame * param_frm = NULL;

	if (!sw_ctx) {
		pr_err("fail to get ptr handle:%p.\n", sw_ctx);
		return -1;
	}

	for (i = 0; i < DCAM_OFFLINE_PARAM_Q_LEN; i++) {
		param_frm = cam_queue_empty_frame_get();
		param_frm->pm = vzalloc(sizeof(struct dcam_dev_param));
		param_frm->fid = 0xffff;
		if (param_frm->pm) {
			init_dcam_pm(param_frm->pm);
			if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
				iommu_enable = 1;

			ret = cam_buf_alloc(&param_frm->pm->lsc.buf, DCAM_LSC_BUF_SIZE, iommu_enable);
			if (ret)
				goto alloc_fail;

			ret = cam_buf_kmap(&param_frm->pm->lsc.buf);
			if (ret)
				goto map_fail;
			ret = cam_queue_enqueue(&sw_ctx->blk_param_queue, &param_frm->list);
			if (ret) {
				pr_warn("Warning:fail to enqueue empty queue.\n");
				cam_buf_kunmap(&param_frm->pm->lsc.buf);
				cam_buf_free(&param_frm->pm->lsc.buf);
				kfree(param_frm->pm);
				cam_queue_empty_frame_put(param_frm);
			}
		} else {
			cam_queue_empty_frame_put(param_frm);
			pr_warn("Warning: alloc dcam pm buf fail.\n");
			return -1;
		}
	}

	return 0;

map_fail:
	pr_err("map lsc buf fail.\n");
	cam_buf_free(&param_frm->pm->lsc.buf);
alloc_fail:
	pr_err("alloc lsc buf fail.\n");
	kfree(param_frm->pm);
	cam_queue_empty_frame_put(param_frm);

	return -1;
}

static int camcore_fdr_context_init(struct camera_module *module,
		struct channel_context *ch)
{
	int ret = 0;
	int i = 0;
	int isp_ctx_id = 0, isp_path_id = 0;
	int32_t *cur_ctx;
	int32_t *cur_path;
	struct camera_uchannel *ch_uinfo;
	struct isp_ctx_base_desc ctx_desc;
	struct isp_ctx_size_desc ctx_size;
	struct isp_path_base_desc path_desc;
	struct img_trim path_trim;
	struct isp_init_param init_param;
	struct dcam_pipe_dev *dcam = NULL;
	struct cam_data_ctrl_in ctrl_in;
	struct isp_data_ctrl_cfg *fdrl_ctrl = NULL;

	pr_debug("cam%d enter\n", module->idx);

	module->fdr_done = 0;
	ret = camcore_aux_dcam_init(module, ch);
	if (ret) {
		pr_err("fail to init aux dcam\n");
		goto init_isp;
	}

	if (ret < 0) {
		pr_err("fail to start dcam cfg, ret %d\n", ret);
		goto exit;
	}

	pr_debug("cam%d, dcam %px %px, idx %d %d\n", module->idx,
		module->dcam_dev_handle, module->aux_dcam_dev,
		module->dcam_idx, module->aux_dcam_id);

	dcam = (struct dcam_pipe_dev *)module->aux_dcam_dev;
	ret = dcam->dcam_pipe_ops->start(&dcam->sw_ctx[module->offline_cxt_id], 0);
	if (ret < 0) {
		pr_err("fail to start dcam dev, ret %d\n", ret);
		goto exit;
	}
	if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) {
		dcam->sw_ctx[module->offline_cxt_id].is_raw_alg = 1;
		dcam->sw_ctx[module->offline_cxt_id].raw_alg_type = module->cam_uinfo.raw_alg_type;
	}

init_isp:
	ch_uinfo = &ch->ch_uinfo;
	ctrl_in.scene_type = CAM_SCENE_CTRL_FDR_L;
	ctrl_in.src.w = ch_uinfo->src_size.w;
	ctrl_in.src.h = ch_uinfo->src_size.h;
	ctrl_in.crop.start_x = ch_uinfo->src_crop.x;
	ctrl_in.crop.start_y = ch_uinfo->src_crop.y;
	ctrl_in.crop.size_x = ch_uinfo->src_crop.w;
	ctrl_in.crop.size_y = ch_uinfo->src_crop.h;
	ctrl_in.dst.w = ch_uinfo->dst_size.w;
	ctrl_in.dst.h = ch_uinfo->dst_size.h;
	ctrl_in.raw_alg_type = module->cam_uinfo.raw_alg_type;
	ret = module->isp_dev_handle->isp_ops->set_datactrl(module->isp_dev_handle,
				&ctrl_in, &ch->isp_scene_ctrl);
	for (i = 0; i < 2; i++) {
		cur_ctx = (i == 0) ? &ch->isp_fdrl_ctx : &ch->isp_fdrh_ctx;
		cur_path = (i == 0) ? &ch->isp_fdrl_path : &ch->isp_fdrh_path;
		fdrl_ctrl = (i == 0) ? &ch->isp_scene_ctrl.fdrl_ctrl : &ch->isp_scene_ctrl.fdrh_ctrl;
		if (((*cur_ctx >= 0) && (*cur_path >= 0)) || !i)
			continue;
		/* get context id and config context */
		memset(&init_param, 0, sizeof(struct isp_init_param));
		init_param.cam_id = module->idx;
		init_param.blkparam_node_num = CAM_SCENE_CTRL_MAX;
		ret = module->isp_dev_handle->isp_ops->get_context(module->isp_dev_handle, &init_param);
		if (ret < 0) {
			pr_err("fail to get isp context for cam%d ch %d\n",
				module->idx, ch->ch_id);
			goto exit;
		}
		isp_ctx_id = ret;
		module->isp_dev_handle->isp_ops->set_callback(module->isp_dev_handle,
			isp_ctx_id, camcore_isp_callback, module);

		memset(&ctx_desc, 0, sizeof(struct isp_ctx_base_desc));
		ctx_desc.in_fmt = fdrl_ctrl->in_format;
		ctx_desc.pack_bits= ch_uinfo->dcam_raw_fmt;
		ctx_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
		ctx_desc.ch_id = ch->ch_id;
		ctx_desc.is_pack = 1;
		ctx_desc.pyr_is_pack = 1;
		ctx_desc.data_in_bits = DCAM_STORE_10_BIT;
		ctx_desc.pyr_data_bits = DCAM_STORE_10_BIT;
		if (ctx_desc.ch_id == CAM_CH_CAP) {
			ctx_desc.mode_gtm = MODE_GTM_CAP;
		} else if (ctx_desc.ch_id == CAM_CH_PRE) {
			ctx_desc.mode_gtm = MODE_GTM_PRE;
		}
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_CTX_BASE, isp_ctx_id, 0, &ctx_desc);
		ctx_size.src = fdrl_ctrl->src;
		ctx_size.crop = fdrl_ctrl->crop;
		ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_CTX_SIZE, isp_ctx_id, 0, &ctx_size);

		/* get path id and config path */
		isp_path_id = ISP_SPATH_CP;
		ret = module->isp_dev_handle->isp_ops->get_path(
			module->isp_dev_handle, isp_ctx_id, isp_path_id);

		memset(&path_desc, 0, sizeof(path_desc));
		path_desc.out_fmt = fdrl_ctrl->out_format;
		path_desc.endian.y_endian = ENDIAN_LITTLE;
		path_desc.endian.uv_endian = ENDIAN_LITTLE;
		path_desc.output_size.w = fdrl_ctrl->dst.w;
		path_desc.output_size.h = fdrl_ctrl->dst.h;
		path_desc.data_bits = DCAM_STORE_10_BIT;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_BASE,
			isp_ctx_id, isp_path_id, &path_desc);
		/* zoom in ISP : fetch trim, scaler no trim  */
		path_trim.start_x = 0;
		path_trim.start_y = 0;
		path_trim.size_x = fdrl_ctrl->crop.size_x;
		path_trim.size_y = fdrl_ctrl->crop.size_y;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_SIZE,
				isp_ctx_id, isp_path_id, &path_trim);

		*cur_ctx = (int32_t)isp_ctx_id;
		*cur_path = (int32_t)isp_path_id;
		pr_debug("init fdrl path %x\n", *cur_ctx);
	}
	module->fdr_init = 1;
	module->fdr_done = 0;

	return 0;

exit:
	pr_err("failed %d\n", ret);
	camcore_aux_dcam_deinit(module);
	return ret;
}

static int camcore_channel_init(struct camera_module *module,
		struct channel_context *channel)
{
	int ret = 0;
	int isp_ctx_id = 0, isp_path_id = 0, dcam_path_id = 0;
	int slave_path_id = 0, bpc_raw_flag = 0;
	int new_isp_ctx, new_isp_path, new_dcam_path;
	struct channel_context *channel_prev = NULL;
	struct channel_context *channel_cap = NULL;
	struct channel_context *channel_vir = NULL;
	struct camera_uchannel *ch_uinfo;
	struct isp_path_base_desc path_desc;
	struct dcam_path_cfg_param ch_desc = {0};
	struct isp_init_param init_param;
	struct cam_hw_info *hw = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	uint32_t format = 0;

	hw = module->grp->hw_info;
	ch_uinfo = &channel->ch_uinfo;
	ch_uinfo->src_size.w = module->cam_uinfo.sn_rect.w;
	ch_uinfo->src_size.h = module->cam_uinfo.sn_rect.h;
	new_isp_ctx = 0;
	new_isp_path = 0;
	new_dcam_path = 0;

	memset(&init_param, 0, sizeof(struct isp_init_param));
	format = module->cam_uinfo.sensor_if.img_fmt;

	switch (channel->ch_id) {
	case CAM_CH_PRE:
		if (format == DCAM_CAP_MODE_YUV)
			dcam_path_id = DCAM_PATH_FULL;
		else
			dcam_path_id = DCAM_PATH_BIN;
		isp_path_id = ISP_SPATH_CP;
		new_isp_ctx = 1;
		new_isp_path = 1;
		new_dcam_path = 1;
		break;

	case CAM_CH_VID:
		/* only consider video/pre share same
		 * dcam path and isp ctx now.
		 */
		channel_prev = &module->channel[CAM_CH_PRE];
		if (channel_prev->enable) {
			channel->dcam_path_id = channel_prev->dcam_path_id;
			isp_ctx_id = channel_prev->isp_ctx_id;
		} else {
			dcam_path_id = DCAM_PATH_BIN;
			new_dcam_path = 1;
			new_isp_ctx = 1;
			pr_info("vid channel enable without preview\n");
		}
		isp_path_id = ISP_SPATH_VID;
		new_isp_path = 1;
		break;

	case CAM_CH_CAP:
		if (module->grp->hw_info->prj_id == SHARKL5pro && ch_uinfo->src_size.w >= DCAM_HW_SLICE_WIDTH_MAX) {
			dcam_path_id = DCAM_PATH_VCH2;
			module->auto_3dnr = 0;
		} else
			dcam_path_id = DCAM_PATH_FULL;
		if (module->simulator &&
			!module->channel[CAM_CH_PRE].enable &&
			!module->channel[CAM_CH_VID].enable)
			dcam_path_id = DCAM_PATH_BIN;
		isp_path_id = ISP_SPATH_CP;
		new_isp_ctx = 1;
		new_isp_path = 1;
		new_dcam_path = 1;
		break;

	case CAM_CH_PRE_THM:
		channel_prev = &module->channel[CAM_CH_PRE];
		if (channel_prev->enable == 0) {
			pr_err("fail to get preview channel enable status\n");
			return -EINVAL;
		}
		channel->dcam_path_id = channel_prev->dcam_path_id;
		isp_ctx_id = channel_prev->isp_ctx_id;
		isp_path_id = ISP_SPATH_FD;
		new_isp_path = 1;
		break;

	case CAM_CH_CAP_THM:
		channel_cap = &module->channel[CAM_CH_CAP];
		if (channel_cap->enable == 0) {
			pr_err("fail to get capture channel enable status\n");
			return -EINVAL;
		}
		channel->dcam_path_id = channel_cap->dcam_path_id;
		isp_ctx_id = channel_cap->isp_ctx_id;
		if (module->isp_dev_handle->isp_hw->ip_isp->capture_thumb_support)
			isp_path_id = ISP_SPATH_FD;
		else
			isp_path_id = ISP_SPATH_VID;
		new_isp_path = 1;
		break;

	case CAM_CH_RAW:
		if (( module->grp->hw_info->prj_id == SHARKL5pro && ch_uinfo->src_size.w >= DCAM_HW_SLICE_WIDTH_MAX)
			|| module->raw_callback)
			dcam_path_id = DCAM_PATH_VCH2;
		else
			dcam_path_id = hw->ip_dcam[0]->dcam_raw_path_id;
		module->cam_uinfo.raw_alg_type = 0;
		module->cam_uinfo.is_raw_alg = 0;
		new_dcam_path = 1;
		break;

	case CAM_CH_VIRTUAL:
		channel->isp_ctx_id = module->channel[CAM_CH_PRE].isp_ctx_id;
		channel->slave_isp_ctx_id = module->channel[CAM_CH_CAP].isp_ctx_id;
		channel->isp_path_id = ISP_SPATH_VID;
		channel_vir = &module->channel[CAM_CH_VIRTUAL];
		ret = camcore_vir_channel_config(module, channel_vir);
		if (ret < 0) {
			pr_err("fail to virtual config, ret = %d\n", ret);
			return -EINVAL;
		}
		break;
	case CAM_CH_DCAM_VCH:
		dcam_path_id = DCAM_PATH_VCH2;
		new_dcam_path = 1;
		break;

	default:
		pr_err("fail to get channel id %d\n", channel->ch_id);
		return -EINVAL;
	}
	if (module->paused) {
		new_isp_ctx = 0;
		new_isp_path = 0;
		isp_ctx_id = channel->isp_ctx_id;
		isp_path_id = channel->isp_path_id;
	}

	pr_info("ch %d dcam_path(new %d path_id %d) isp_path(new %d path_id %d) isp_ctx(new %d)\n",
		channel->ch_id, new_dcam_path, dcam_path_id, new_isp_path, isp_path_id, new_isp_ctx);
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	if (channel->ch_id == CAM_CH_RAW && module->cam_uinfo.need_dcam_raw && (module->channel[CAM_CH_CAP].enable
		|| module->channel[CAM_CH_VID].enable)) {
		if (module->grp->hw_info->prj_id == QOGIRN6pro) {
			module->cam_uinfo.need_dcam_raw = 0;
			bpc_raw_flag = 1;
		} else {
			dcam_path_id = DCAM_PATH_RAW;
			dcam_sw_ctx->need_dcam_raw = 1;
		}
	}
	pr_info("cam%d, simulator=%d\n", module->idx, module->simulator);
	if (new_dcam_path) {
		memset(&ch_desc, 0, sizeof(ch_desc));
		ch_desc.is_raw = 0;
		ch_desc.raw_src = PROCESS_RAW_SRC_SEL;
		if (channel->ch_id == CAM_CH_RAW && module->cam_uinfo.need_dcam_raw == 0)
			ch_desc.is_raw = 1;
		if (channel->ch_id == CAM_CH_DCAM_VCH)
			ch_desc.is_raw = 1;
		if ((channel->ch_id == CAM_CH_CAP) && module->cam_uinfo.is_4in1)
			ch_desc.is_raw = 1;
		if ((channel->ch_id == CAM_CH_CAP) && (module->cam_uinfo.dcam_slice_mode) && (!module->cam_uinfo.virtualsensor))
			ch_desc.is_raw = 1;
		if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.is_raw_alg &&
			(module->cam_uinfo.param_frame_sync || module->cam_uinfo.raw_alg_type != RAW_ALG_AI_SFNR) && (!module->cam_uinfo.virtualsensor)) {
			/* config full path to raw */
			ch_desc.is_raw = 1;
			dcam_sw_ctx->is_raw_alg = 1;
			dcam_sw_ctx->raw_alg_type = module->cam_uinfo.raw_alg_type;
		}
		if (bpc_raw_flag) {
			ch_desc.is_raw = 0;
			ch_desc.raw_src = BPC_RAW_SRC_SEL;
		}
		if (ch_desc.is_raw && module->grp->hw_info->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW &&
			!module->raw_callback && (channel->ch_id != CAM_CH_DCAM_VCH))
			dcam_path_id = DCAM_PATH_RAW;
		ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_sw_ctx, dcam_path_id);
		if (ret < 0) {
			pr_err("fail to get dcam path %d\n", dcam_path_id);
			return -EFAULT;
		}
		if (module->cam_uinfo.is_raw_alg && channel->ch_id == CAM_CH_CAP &&
			module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR && !module->cam_uinfo.param_frame_sync) {
			dcam_sw_ctx->raw_alg_type = module->cam_uinfo.raw_alg_type;
			ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(dcam_sw_ctx, DCAM_PATH_RAW);
			if (ret < 0) {
				pr_err("fail to get dcam path %d\n", dcam_path_id);
				return -EFAULT;
			}
		}
		channel->dcam_path_id = dcam_path_id;
		pr_debug("get dcam path : %d\n", channel->dcam_path_id);

		/* todo: cfg param to user setting. */
		if (!ch_desc.is_raw) {
			if ((channel->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10) && (channel->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX))
				ch_desc.raw_fmt = channel->ch_uinfo.dcam_raw_fmt;
			else
				ch_desc.raw_fmt = hw->ip_dcam[0]->raw_fmt_support[0];
			channel->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
		} else {
			if ((channel->ch_uinfo.sensor_raw_fmt >= DCAM_RAW_PACK_10) && (channel->ch_uinfo.sensor_raw_fmt < DCAM_RAW_MAX))
				ch_desc.raw_fmt = channel->ch_uinfo.sensor_raw_fmt;
			else {
				ch_desc.raw_fmt = hw->ip_dcam[0]->sensor_raw_fmt;
				if (module->cam_uinfo.dcam_slice_mode && hw->ip_dcam[0]->save_band_for_bigsize)
					/* for save data band, ultra res not need sensor raw of raw14*/
					ch_desc.raw_fmt = DCAM_RAW_PACK_10;
				if (dcam_path_id == 0 && module->cam_uinfo.is_4in1 == 1)
					ch_desc.raw_fmt = DCAM_RAW_PACK_10;
			}
			if (module->cam_uinfo.is_raw_alg && channel->ch_id == CAM_CH_CAP && module->cam_uinfo.need_dcam_raw == 0) {
				ch_desc.raw_fmt = DCAM_RAW_14;
				if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2)
					channel->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
			}
			channel->ch_uinfo.sensor_raw_fmt = ch_desc.raw_fmt;
		}

		if (dcam_path_id == 0 && module->cam_uinfo.is_4in1 == 1) {
			ch_desc.raw_fmt = DCAM_RAW_PACK_10;
			channel->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
			channel->ch_uinfo.sensor_raw_fmt = ch_desc.raw_fmt;
			pr_debug("sensor_raw_fmt:%d raw_fmt %d\n", channel->ch_uinfo.sensor_raw_fmt, ch_desc.raw_fmt);
		}

		ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
		/*
		 * Configure slow motion for BIN path. HAL must set @is_high_fps
		 * and @high_fps_skip_num for both preview channel and video
		 * channel so BIN path can enable slow motion feature correctly.
		 */
		ch_desc.slowmotion_count = ch_uinfo->high_fps_skip_num;

		ch_desc.endian.y_endian = ENDIAN_LITTLE;
		ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
		ch_desc.input_trim.start_x = module->cam_uinfo.sn_rect.x;
		ch_desc.input_trim.start_y = module->cam_uinfo.sn_rect.y;
		ch_desc.input_trim.size_x = module->cam_uinfo.sn_rect.w;
		ch_desc.input_trim.size_y = module->cam_uinfo.sn_rect.h;
		/*
		*   default dcam out bit: N6pro input raw10bit output YUV 10bit
		*   other chip input raw10 output raw 10bit
		*   YUV sensor input 8bit output 8bit
		*/

		if ((channel->ch_uinfo.dcam_output_bit >= DCAM_STORE_8_BIT) && (channel->ch_uinfo.dcam_output_bit < DCAM_STORE_BIT_MAX))
			ch_desc.dcam_out_bits = channel->ch_uinfo.dcam_output_bit;
		else
			ch_desc.dcam_out_bits = hw->ip_dcam[0]->dcam_output_support[0];
		channel->ch_uinfo.dcam_output_bit = ch_desc.dcam_out_bits;
		/* hw limit:pyr output must 10bit; control pyr output switch, pyr_is_pack 1:mipi; 0:half word; */
		channel->ch_uinfo.pyr_data_bits = DCAM_STORE_10_BIT;
		channel->ch_uinfo.pyr_is_pack = PYR_IS_PACK;
		ch_desc.pyr_data_bits = channel->ch_uinfo.pyr_data_bits;
		ch_desc.pyr_is_pack = channel->ch_uinfo.pyr_is_pack;
		if ((hw->ip_isp->fetch_raw_support == 0) || (format == DCAM_CAP_MODE_YUV))
			ch_desc.dcam_out_fmt = DCAM_STORE_YVU420;
		else
			ch_desc.dcam_out_fmt = DCAM_STORE_RAW_BASE;
		if (channel->ch_id == CAM_CH_RAW || (channel->ch_id == CAM_CH_CAP && channel->dcam_path_id == DCAM_PATH_RAW))
			ch_desc.dcam_out_fmt = DCAM_STORE_RAW_BASE;
		if ((dcam_path_id == DCAM_PATH_RAW) && (format == DCAM_CAP_MODE_YUV))
			ch_desc.dcam_out_fmt = DCAM_STORE_YUV422;

		pr_debug("ch%d, dcam path%d, cap fmt%d, is raw %d, slice mode %d, dcam out format 0x%lx, bits %d, raw fmt %d raw_src %d"
			", sensor raw fmt %d, dcam raw fmt %d\n",
			channel->ch_id, dcam_path_id, format, ch_desc.is_raw, module->cam_uinfo.dcam_slice_mode,
			ch_desc.dcam_out_fmt, ch_desc.dcam_out_bits, ch_desc.raw_fmt, ch_desc.raw_src,
			channel->ch_uinfo.sensor_raw_fmt, channel->ch_uinfo.dcam_raw_fmt);

		/* auto_3dnr:hw enable, channel->uinfo_3dnr == 1: hw enable */
		ch_desc.enable_3dnr = (module->auto_3dnr | channel->uinfo_3dnr);
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_BASE, channel->dcam_path_id, &ch_desc);
		channel->dcam_out_fmt = ch_desc.dcam_out_fmt;

		if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR && !module->cam_uinfo.param_frame_sync) {
			struct dcam_path_cfg_param tmp = {0};
			tmp.output_size.w = channel->ch_uinfo.src_size.w;
			tmp.output_size.h = channel->ch_uinfo.src_size.h;
			tmp.input_size.w = channel->ch_uinfo.src_size.w;
			tmp.input_size.h = channel->ch_uinfo.src_size.h;
			tmp.input_trim.start_x = 0;
			tmp.input_trim.start_y = 0;
			tmp.input_trim.size_x = channel->ch_uinfo.src_size.w;
			tmp.input_trim.size_y = channel->ch_uinfo.src_size.h;
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_SIZE, DCAM_PATH_RAW, &tmp);

			ch_desc.raw_fmt = DCAM_RAW_14;
			ch_desc.is_raw = 1;
			ch_desc.raw_src = ORI_RAW_SRC_SEL;
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_BASE, DCAM_PATH_RAW, &ch_desc);
		}
	}

	if (new_isp_ctx) {
		struct isp_ctx_base_desc ctx_desc;
		uint32_t format = module->cam_uinfo.sensor_if.img_fmt;
		memset(&ctx_desc, 0, sizeof(struct isp_ctx_base_desc));
		init_param.is_high_fps = ch_uinfo->is_high_fps;
		init_param.cam_id = module->idx;
		if (channel->ch_uinfo.is_high_fps)
			init_param.blkparam_node_num = CAM_SHARED_BUF_NUM / ch_uinfo->high_fps_skip_num;
		else
			init_param.blkparam_node_num = camcore_buffers_alloc_num(channel, module) + 1;
		ret = module->isp_dev_handle->isp_ops->get_context(module->isp_dev_handle, &init_param);
		if (ret < 0) {
			pr_err("fail to get isp context for cam%d ch %d\n", module->idx, channel->ch_id);
			goto exit;
		}
		isp_ctx_id = ret;
		module->isp_dev_handle->isp_ops->set_callback(module->isp_dev_handle,
			isp_ctx_id, camcore_isp_callback, module);

		/* todo: cfg param to user setting. */
		/* cfg in fmt */
		if (format == DCAM_CAP_MODE_YUV)
			ctx_desc.in_fmt = ch_uinfo->dst_fmt;
		else {
			if (dcam_path_id == DCAM_PATH_RAW)
				ctx_desc.in_fmt = camcore_format_dcam_translate(DCAM_STORE_YVU420);
			else
				ctx_desc.in_fmt = camcore_format_dcam_translate(ch_desc.dcam_out_fmt);
		}

		ctx_desc.pyr_data_bits = ch_desc.pyr_data_bits;
		ctx_desc.pyr_is_pack = ch_desc.pyr_is_pack;
		ctx_desc.data_in_bits = ch_desc.dcam_out_bits;
		/* cfg pack bit */
		if (channel->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10 && channel->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX)
			ctx_desc.pack_bits = channel->ch_uinfo.dcam_raw_fmt;
		else {
			ctx_desc.pack_bits = hw->ip_dcam[0]->raw_fmt_support[0];
			if (hw->ip_dcam[0]->save_band_for_bigsize)
				ctx_desc.pack_bits = DCAM_RAW_PACK_10;
		}

		/* cfg is pack*/
		if ((hw->ip_isp->fetch_raw_support == 0) || (format == DCAM_CAP_MODE_YUV)) {
			if (ctx_desc.data_in_bits == DCAM_STORE_10_BIT)
				channel->ch_uinfo.dcam_out_pack = 1;
			else
				channel->ch_uinfo.dcam_out_pack = 0;
		} else {
			if (ctx_desc.pack_bits == DCAM_RAW_PACK_10)
				channel->ch_uinfo.dcam_out_pack = 1;
			else
				channel->ch_uinfo.dcam_out_pack = 0;
		}
		ctx_desc.is_pack = channel->ch_uinfo.dcam_out_pack;
		pr_debug("isp sw ctx id %d, dcam_out_fmt %d, raw_fmt %d, data_in_bits %d, is pack %d, dcam_path_id %d\n",
			isp_ctx_id, ch_desc.dcam_out_fmt, ch_desc.raw_fmt, ctx_desc.data_in_bits, ctx_desc.is_pack, dcam_path_id);

		ctx_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
		ctx_desc.mode_ltm = MODE_LTM_OFF;
		ctx_desc.mode_gtm = MODE_GTM_OFF;
		ctx_desc.mode_3dnr = MODE_3DNR_OFF;
		ctx_desc.enable_slowmotion = ch_uinfo->is_high_fps;
		ctx_desc.slowmotion_count = ch_uinfo->high_fps_skip_num;
		ctx_desc.slw_state = CAM_SLOWMOTION_OFF;
		ctx_desc.ch_id = channel->ch_id;
		ctx_desc.sn_size = module->cam_uinfo.sn_size;

		/* 20190614: have some change for auto 3dnr, maybe some code
		 * will be refined laster. below show how to use now
		 * 1: ch->type_3dnr, flag for hw 3dnr(dcam) alloc buffer
		 * 2: module->auto_3dnr: 1: alloc buffer for prev,cap,
		 *    later will enable/disable by ch->uinfo_3dnr
		 * scene1: nightshot:module->auto_3dnr==0,prev_ch->type_3dnr==1
		 *         cap_ch->type_3dnr == 0,prev hw, cap sw
		 * scene2: auto_3dnr:module->auto_3dnr==1,ch->type_3dnr==x
		 *         dynamical enable/disable(before start_capture)
		 * scene3: off: module->auto_3dnr == 0, ch->type_3dnr == 0
		 */
		ctx_desc.mode_3dnr = MODE_3DNR_OFF;
		if (module->auto_3dnr) {
			if (channel->uinfo_3dnr) {
				if (channel->ch_id == CAM_CH_CAP)
					ctx_desc.mode_3dnr = MODE_3DNR_CAP;
				else
					ctx_desc.mode_3dnr = MODE_3DNR_PRE;
			}
			channel->type_3dnr = CAM_3DNR_HW;
		} else {
			channel->type_3dnr = CAM_3DNR_OFF;
			if (channel->uinfo_3dnr) {
				channel->type_3dnr = CAM_3DNR_HW;
				if (channel->ch_id == CAM_CH_CAP)
					ctx_desc.mode_3dnr = MODE_3DNR_CAP;
				else
					ctx_desc.mode_3dnr = MODE_3DNR_PRE;
			}
		}

		if (module->cam_uinfo.is_rgb_ltm) {
			channel->ltm_rgb = 1;
			ctx_desc.ltm_rgb = 1;
			if (channel->ch_id == CAM_CH_CAP) {
				channel->mode_ltm = MODE_LTM_CAP;
				ctx_desc.mode_ltm = MODE_LTM_CAP;
			} else if (channel->ch_id == CAM_CH_PRE) {
				channel->mode_ltm = MODE_LTM_PRE;
				ctx_desc.mode_ltm = MODE_LTM_PRE;
			}
		} else {
			channel->ltm_rgb = 0;
			ctx_desc.ltm_rgb = 0;
		}

		if (module->cam_uinfo.is_yuv_ltm) {
			channel->ltm_yuv = 1;
			ctx_desc.ltm_yuv = 1;
			if (channel->ch_id == CAM_CH_CAP) {
				channel->mode_ltm = MODE_LTM_CAP;
				ctx_desc.mode_ltm = MODE_LTM_CAP;
			} else if (channel->ch_id == CAM_CH_PRE) {
				channel->mode_ltm = MODE_LTM_PRE;
				ctx_desc.mode_ltm = MODE_LTM_PRE;
			}
		} else {
			channel->ltm_yuv = 0;
			ctx_desc.ltm_yuv = 0;
		}

		if (module->cam_uinfo.is_rgb_gtm) {
			channel->gtm_rgb = 1;
			ctx_desc.gtm_rgb = 1;
			if (channel->ch_id == CAM_CH_CAP) {
				channel->mode_gtm = MODE_GTM_CAP;
				ctx_desc.mode_gtm = MODE_GTM_CAP;
			} else if (channel->ch_id == CAM_CH_PRE) {
				channel->mode_gtm = MODE_GTM_PRE;
				ctx_desc.mode_gtm = MODE_GTM_PRE;
			}
		} else {
			channel->gtm_rgb = 0;
			ctx_desc.gtm_rgb = 0;
		}
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_CTX_BASE, isp_ctx_id, 0, &ctx_desc);
	}

	if (new_isp_path) {
		ret = module->isp_dev_handle->isp_ops->get_path(module->isp_dev_handle, isp_ctx_id, isp_path_id);
		if (ret < 0) {
			pr_err("fail to get isp path %d from context %d\n", isp_path_id, isp_ctx_id);
			if (new_isp_ctx)
				module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);
			goto exit;
		}
		channel->isp_ctx_id = (int32_t)(isp_ctx_id);
		channel->isp_path_id = (int32_t)(isp_path_id);
		pr_debug("isp sw ctx id %d, get path : 0x%x\n", isp_ctx_id, channel->isp_path_id);

		memset(&path_desc, 0, sizeof(path_desc));
		if (channel->ch_uinfo.slave_img_en) {
			slave_path_id = ISP_SPATH_VID;
			path_desc.slave_type = ISP_PATH_MASTER;
			path_desc.slave_path_id = slave_path_id;
		}
		path_desc.out_fmt = ch_uinfo->dst_fmt;
		path_desc.endian.y_endian = ENDIAN_LITTLE;
		path_desc.endian.uv_endian = ENDIAN_LITTLE;
		path_desc.output_size.w = ch_uinfo->dst_size.w;
		path_desc.output_size.h = ch_uinfo->dst_size.h;
		path_desc.regular_mode = ch_uinfo->regular_desc.regular_mode;
		path_desc.data_bits = DCAM_STORE_10_BIT;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_BASE, isp_ctx_id, isp_path_id, &path_desc);

		if (ch_uinfo->is_high_fps) {
			struct isp_ctx_base_desc slw_desc;
			memset(&slw_desc, 0, sizeof(struct isp_ctx_base_desc));
			slw_desc.slowmotion_stage_a_num = ch_uinfo->frame_num;
			slw_desc.slowmotion_stage_a_valid_num = ch_uinfo->high_fps_skip_num1;
			slw_desc.slowmotion_stage_b_num = ch_uinfo->frame_num1;
			ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_SLW, isp_ctx_id, isp_path_id, &slw_desc);
		}
	}

	if (new_isp_path && channel->ch_uinfo.slave_img_en) {
		ret = module->isp_dev_handle->isp_ops->get_path(module->isp_dev_handle,
			isp_ctx_id, slave_path_id);
		if (ret < 0) {
			pr_err("fail to get isp path %d from context %d\n", slave_path_id, isp_ctx_id);
			module->isp_dev_handle->isp_ops->put_path(module->isp_dev_handle, isp_ctx_id, isp_path_id);
			if (new_isp_ctx)
				module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);
			goto exit;
		}
		channel->slave_isp_ctx_id = (int32_t)(isp_ctx_id);
		channel->slave_isp_path_id = (int32_t)(slave_path_id);
		path_desc.slave_type = ISP_PATH_SLAVE;
		path_desc.out_fmt = ch_uinfo->slave_img_fmt;
		path_desc.endian.y_endian = ENDIAN_LITTLE;
		path_desc.endian.uv_endian = ENDIAN_LITTLE;
		path_desc.output_size.w = ch_uinfo->slave_img_size.w;
		path_desc.output_size.h = ch_uinfo->slave_img_size.h;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_BASE, isp_ctx_id, slave_path_id, &path_desc);
	}

	if (module->paused)
		goto exit;

	/* 4in1 setting */
	if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.is_4in1) {
		ret = camcore_4in1_aux_init(module, channel);
		if (ret < 0) {
			pr_err("fail to init dcam for 4in1, ret = %d\n", ret);
			goto exit;
		}
	}
	if (channel->ch_id == CAM_CH_RAW && module->cam_uinfo.is_4in1) {
		ret = camcore_4in1_secondary_path_init(module, channel);
		if (ret)
			pr_err("fail to init 4in1 raw capture for bin sum\n");
	}
	/* bigsize setting */
	if (channel->ch_id == CAM_CH_CAP && module->cam_uinfo.dcam_slice_mode && !module->cam_uinfo.is_4in1) {
		ret = camcore_bigsize_aux_init(module, channel);
		if (ret < 0) {
			pr_err("fail to init dcam for 4in1, ret = %d\n", ret);
			goto exit;
		}
	}

	if (module->cam_uinfo.is_raw_alg && module->channel[CAM_CH_CAP].enable && channel->ch_id == CAM_CH_CAP) {
		mutex_lock(&module->fdr_lock);
		if (module->fdr_init == 0) {
			ret = camcore_fdr_context_init(module, &module->channel[CAM_CH_CAP]);
			if (unlikely(ret)) {
				pr_err("fail to init fdr\n");
				mutex_unlock(&module->fdr_lock);
				return 0;
			}
		}
		mutex_unlock(&module->fdr_lock);
	}

exit:
	pr_info("dcam(sw_ctx %d hw_ctx %d), path_id(dcam %d, aux dcam %d, isp %d)\n",
		dcam_sw_ctx->sw_ctx_id, dcam_sw_ctx->hw_ctx_id, channel->dcam_path_id, channel->aux_dcam_path_id, channel->isp_path_id);
	pr_debug("ch %d done. ret = %d\n", channel->ch_id, ret);
	return ret;
}

static int camcore_fdr_context_deinit(struct camera_module *module, struct channel_context *ch)
{
	int ret = 0;
	int isp_ctx_id = 0, isp_path_id = 0;

	pr_info("enter\n");
	camcore_aux_dcam_deinit(module);

	if (ch->isp_fdrl_path >= 0 && ch->isp_fdrl_ctx >= 0) {
		isp_path_id = ch->isp_fdrl_path;
		isp_ctx_id = ch->isp_fdrl_ctx;
		module->isp_dev_handle->isp_ops->put_path(module->isp_dev_handle, isp_ctx_id, isp_path_id);
		module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);
		if (ch->fdrl_zoom_buf) {
			cam_buf_ionbuf_put(&ch->fdrl_zoom_buf->buf);
			cam_queue_empty_frame_put(ch->fdrl_zoom_buf);
			ch->fdrl_zoom_buf = NULL;
		}
		pr_info("put 0x%x done\n", ch->isp_fdrl_path);
	}

	if (ch->isp_fdrh_path >= 0 && ch->isp_fdrh_ctx >= 0) {
		isp_path_id = ch->isp_fdrh_path;
		isp_ctx_id = ch->isp_fdrh_ctx;
		module->isp_dev_handle->isp_ops->put_path(module->isp_dev_handle, isp_ctx_id, isp_path_id);
		module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);
		if (ch->fdrh_zoom_buf) {
			cam_buf_ionbuf_put(&ch->fdrh_zoom_buf->buf);
			cam_queue_empty_frame_put(ch->fdrh_zoom_buf);
			ch->fdrh_zoom_buf = NULL;
		}
		pr_info("put 0x%x done\n", ch->isp_fdrh_path);
	}

	ch->isp_fdrl_path = -1;
	ch->isp_fdrh_path = -1;
	ch->isp_fdrh_ctx = -1;
	ch->isp_fdrl_ctx = -1;
	module->fdr_init = 0;
	module->fdr_done = 0;
	pr_info("done\n");

	return ret;
}

static int camcore_mes_proc(void *param)
{
	struct camera_module *module = NULL;
	struct cam_mes_ctx *mes_base = NULL;
	struct channel_context *channel = NULL;
	struct channel_context *channel_raw = NULL, *channel_cap = NULL;
	struct camera_frame *temp_pframe = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_path_desc *path = NULL;
	struct camera_frame *raw_frame = NULL;
	int ret = 0, dump_path = 0;

	module = (struct camera_module *)param;
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	mes_base = &module->mes_base;
	channel_raw = &module->channel[CAM_CH_RAW];
	channel_cap = &module->channel[CAM_CH_CAP];
	path = &dcam_sw_ctx->path[DCAM_PATH_RAW];
	if (wait_for_completion_interruptible(&mes_base->mes_com) == 0) {

		temp_pframe = cam_queue_dequeue(&mes_base->mes_queue,struct camera_frame, list);
		if (temp_pframe == NULL) {
			pr_err("fail to get temp_pframe buf\n");
			return -EFAULT;
		}
		channel = &module->channel[temp_pframe->channel_id];
		if (channel_raw->ch_uinfo.dst_size.w == channel_cap->ch_uinfo.dst_size.w)
			dump_path = CAM_CH_CAP;
		else
			dump_path = CAM_CH_PRE;
		pr_info("current id %d, width %d, raw width %d\n", temp_pframe->channel_id, temp_pframe->width, channel_raw->ch_uinfo.dst_size.w);
		if (channel_raw->ch_uinfo.dst_size.w != temp_pframe->width) {
			if ((channel_raw->ch_uinfo.dst_size.w > temp_pframe->width) && (dump_path == CAM_CH_PRE))
				pr_debug("pre zoom scene\n");
			else {
				pr_debug("Size no match, temp w %d and raw w %d\n",
					temp_pframe->width, channel_raw->ch_uinfo.dst_size.w);
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id,
						temp_pframe);
				return 0;
			}
		}
		raw_frame = cam_queue_dequeue(&path->out_buf_queue, struct camera_frame, list);
		if (raw_frame == NULL) {
			pr_debug("raw path fail to get queue\n");
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_OUTPUT_BUF,
					channel->dcam_path_id,
					temp_pframe);
		} else {
			pr_debug("start copy temp queue data\n");
			if (temp_pframe->buf.size[0] > raw_frame->buf.size[0]) {
				pr_err("fail to raw buff is small  temp buf size %d and raw buf size %d\n",
					temp_pframe->buf.size[0], raw_frame->buf.size[0]);
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id,
						temp_pframe);
				return -EFAULT;
			}
			if (cam_buf_kmap(&temp_pframe->buf)) {
				pr_err("fail to kmap temp buf\n");
				return -EFAULT;
			}
			cam_buf_iommu_unmap(&raw_frame->buf);
			if (cam_buf_kmap(&raw_frame->buf)) {
				pr_err("fail to kmap raw buf\n");
				return -EFAULT;
			}
			memcpy((char *)raw_frame->buf.addr_k[0], (char *)temp_pframe->buf.addr_k[0], temp_pframe->buf.size[0]);
			/* use SOF time instead of ISP time for better accuracy */
			raw_frame->width = temp_pframe->width;
			raw_frame->height = temp_pframe->height;
			raw_frame->fid = temp_pframe->fid;
			raw_frame->sensor_time.tv_sec = temp_pframe->sensor_time.tv_sec;
			raw_frame->sensor_time.tv_usec = temp_pframe->sensor_time.tv_usec;
			raw_frame->boot_sensor_time = temp_pframe->boot_sensor_time;
			cam_buf_kunmap(&temp_pframe->buf);
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					DCAM_PATH_CFG_OUTPUT_BUF,
					channel->dcam_path_id,
					temp_pframe);
			cam_buf_kunmap(&raw_frame->buf);
			raw_frame->evt = IMG_TX_DONE;
			raw_frame->irq_type = CAMERA_IRQ_IMG;
			raw_frame->priv_data = module;
			raw_frame->channel_id = CAM_CH_RAW;
			ret = cam_queue_enqueue(&module->frm_queue, &raw_frame->list);
			if (ret) {
				cam_buf_ionbuf_put(&raw_frame->buf);
				cam_queue_empty_frame_put(raw_frame);
			} else {
				pr_debug("raw frame fid %d mfd 0x%x\n", raw_frame->fid, raw_frame->buf.mfd[0]);
				complete(&module->frm_com);
			}
		}
	}
	return ret;
}

static void camcore_mes_init(struct camera_module *module)
{
	struct cam_mes_ctx *mes_base = NULL;

	mes_base = &module->mes_base;
	cam_queue_init(&mes_base->mes_queue, DCAM_MID_BUF, camcore_k_frame_put);
	init_completion(&mes_base->mes_com);
}

static void camcore_mes_deinit(struct camera_module *module)
{
	struct cam_mes_ctx *mes_base = NULL;

	mes_base = &module->mes_base;
	cam_queue_clear(&mes_base->mes_queue, struct camera_frame, list);
}


static int camcore_dumpraw_proc(void *param)
{
	uint32_t idx = 0, cnt = 0, ret = 0;
	struct camera_module *module = NULL;
	struct channel_context *channel = NULL;
	struct camera_frame *pframe = NULL;
	struct cam_dbg_dump *dbg = &g_dbg_dump;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct cam_dump_ctx *dump_base = NULL;
	unsigned long flag = 0;
	pr_info("enter. %p\n", param);
	module = (struct camera_module *)param;
	idx = module->dcam_idx;
	if (idx > DCAM_ID_MAX || !module->dcam_dev_handle)
		return 0;
	dump_base = &module->dump_base;

	spin_lock_irqsave(&dbg->dump_lock, flag);
	dbg->dump_ongoing |= (1 << idx);
	dump_base->dump_count = dbg->dump_count;
	init_completion(&dump_base->dump_com);
	spin_unlock_irqrestore(&dbg->dump_lock, flag);

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_aux_sw_ctx_id];

	pr_info("start dump count: %d\n", dump_base->dump_count);
	while (dump_base->dump_count) {
		dump_base->in_dump = 1;
		ktime_get_ts(&dump_base->cur_dump_ts);
		if (wait_for_completion_interruptible(
			&dump_base->dump_com) == 0) {
			if ((atomic_read(&module->state) != CAM_RUNNING)) {
				pr_info("dump raw proc exit, %d %u\n",
					atomic_read(&module->state),
					dump_base->dump_count);
				break;
			}
			while (1) {
				pframe = cam_queue_dequeue(&dump_base->dump_queue,
					struct camera_frame, list);
				if (!pframe)
					break;
				if (dump_base->dump_count == 0)
					camdump_stop(dump_base);
				else
					dump_base->dump_count--;
				spin_lock_irqsave(&dbg->dump_lock, flag);
				camcore_dump_config(module, pframe);
				spin_unlock_irqrestore(&dbg->dump_lock, flag);
				if (dump_base->dump_file != NULL)
					dump_base->dump_file(dump_base, pframe);
				if (dbg->dump_en == DUMP_DCAM_PDAF) {
					if (atomic_read(&module->state) == CAM_RUNNING) {
						pframe->priv_data = module;
						ret = cam_queue_enqueue(&module->frm_queue, &pframe->list);
						if (ret) {
							cam_queue_empty_frame_put(pframe);
						} else {
							complete(&module->frm_com);
							pr_debug("get statis frame: %p, type %d, %d\n",
								pframe, pframe->irq_type, pframe->irq_property);
						}
					} else {
						cam_queue_empty_frame_put(pframe);
					}
					continue;
				}
				if (dbg->dump_en == DUMP_ISP_PYR_REC)
					continue;
				channel = &module->channel[pframe->channel_id];
				if (dbg->dump_en == DUMP_ISP_PYR_DEC){
					ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle,
						pframe, channel->isp_ctx_id);
					if (ret)
						module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
							ISP_PATH_CFG_PYR_DEC_BUF, channel->isp_ctx_id, channel->isp_path_id, pframe);
					continue;
				}
				if (module->cam_uinfo.dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
					struct channel_context *ch = NULL;

					pr_debug("slice %d %p\n", module->cam_uinfo.slice_count, pframe);
					module->cam_uinfo.slice_count++;
					ch = &module->channel[CAM_CH_CAP];
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
							DCAM_PATH_CFG_OUTPUT_BUF, ch->dcam_path_id, pframe);
					if (module->cam_uinfo.slice_count >= module->cam_uinfo.slice_num)
						module->cam_uinfo.slice_count = 0;
					else
						camcore_frame_start_proc(module, pframe);
					continue;
				}
				if (dbg->dump_en == DUMP_DCAM_OFFLINE && module->cap_status == CAM_CAPTURE_RAWPROC) {
					cam_buf_free(&pframe->buf);
					cam_queue_empty_frame_put(pframe);
					continue;
				}
				/* return it to dcam output queue */
				if ((module->cam_uinfo.is_4in1 && channel->aux_dcam_path_id == DCAM_PATH_BIN && pframe->buf.type == CAM_BUF_KERNEL)
					|| dbg->dump_en == DUMP_DCAM_OFFLINE)
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->aux_dcam_path_id, pframe);
				else
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						channel->dcam_path_id, pframe);
			}
		} else {
			pr_info("dump raw proc exit.");
			break;
		}
	}

	dump_base->dump_count = 0;
	dump_base->in_dump = 0;
	pr_info("end dump, real cnt %d\n", cnt);

	spin_lock_irqsave(&dbg->dump_lock, flag);
	dbg->dump_count = 0;
	dbg->dump_ongoing &= ~(1 << idx);
	spin_unlock_irqrestore(&dbg->dump_lock, flag);
	return 0;
}

static void camcore_dumpraw_init(struct camera_module *module)
{
	uint32_t i = 0;
	struct cam_dump_ctx *dump_base = NULL;

	dump_base = &module->dump_base;
	cam_queue_init(&dump_base->dump_queue, DUMP_Q_LEN, camcore_k_frame_put);
	init_completion(&dump_base->dump_com);
	spin_lock(&g_dbg_dump.dump_lock);
	i = module->dcam_idx;
	if (i < DCAM_ID_MAX) {
		g_dbg_dump.dump_start[i] = &module->dump_thrd.thread_com;
		g_dbg_dump.dump_count = 0;
	}
	spin_unlock(&g_dbg_dump.dump_lock);
	dump_base->is_pyr_rec = module->cam_uinfo.is_pyr_rec;
	dump_base->is_pyr_dec = module->cam_uinfo.is_pyr_dec;
}

static void camcore_dumpraw_deinit(struct camera_module *module)
{
	uint32_t i = 0, j = 0;
	struct cam_dump_ctx *dump_base = NULL;
	unsigned long flag = 0;
	dump_base = &module->dump_base;
	if (dump_base->in_dump)
		complete(&dump_base->dump_com);
	spin_lock_irqsave(&g_dbg_dump.dump_lock, flag);
	i = module->dcam_idx;
	if (i < DCAM_ID_MAX) {
		g_dbg_dump.dump_start[i] = NULL;
		g_dbg_dump.dump_count = 0;
	}
	spin_unlock_irqrestore(&g_dbg_dump.dump_lock, flag);
	while (dump_base->in_dump && (j++ < THREAD_STOP_TIMEOUT)) {
		pr_debug("camera%d in dump, wait...%d\n", module->idx, j);
		msleep(10);
	}
	cam_queue_clear(&dump_base->dump_queue, struct camera_frame, list);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void camcore_timer_callback(struct timer_list *t)
{
	struct camera_module *module = from_timer(module, t, cam_timer);
#else
static void camcore_timer_callback(unsigned long data)
{
	struct camera_module *module = (struct camera_module *)data;
#endif

	struct camera_frame *frame;
	int ret = 0;

	if (!module || atomic_read(&module->state) != CAM_RUNNING) {
		pr_err("fail to get valid module %p or error state\n", module);
		return;
	}

	if (atomic_read(&module->timeout_flag) == 1) {
		pr_err("fail to get frame data, CAM%d timeout.\n", module->idx);
		frame = cam_queue_empty_frame_get();
		if (module->cap_status == CAM_CAPTURE_RAWPROC) {
			module->cap_status = CAM_CAPTURE_RAWPROC_DONE;
			frame->evt = IMG_TX_DONE;
			frame->irq_type = CAMERA_IRQ_DONE;
			frame->irq_property = IRQ_RAW_PROC_TIMEOUT;
		} else {
			frame->evt = IMG_TIMEOUT;
			frame->irq_type = CAMERA_IRQ_IMG;
			frame->irq_property = IRQ_MAX_DONE;
		}
		ret = cam_queue_enqueue(&module->frm_queue, &frame->list);
		complete(&module->frm_com);
		if (ret)
			pr_err("fail to enqueue.\n");
	}
}

static void camcore_timer_init(struct timer_list *cam_timer,
		unsigned long data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	timer_setup(cam_timer, camcore_timer_callback, 0);
#else
	setup_timer(cam_timer, camcore_timer_callback, data);
#endif
}

static int camcore_timer_start(struct timer_list *cam_timer,
		uint32_t time_val)
{
	int ret = 0;

	pr_debug("starting timer %ld\n", jiffies);
	ret = mod_timer(cam_timer, jiffies + msecs_to_jiffies(time_val));

	return ret;
}

static int camcore_timer_stop(struct timer_list *cam_timer)
{
	pr_debug("stop timer\n");
	del_timer_sync(cam_timer);
	return 0;
}

static int camcore_thread_loop(void *arg)
{
	struct cam_thread_info *thrd = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	thrd = (struct cam_thread_info *)arg;
	pr_info("%s loop starts %px\n", thrd->thread_name, thrd);
	while (1) {
		if (!IS_ERR_OR_NULL(thrd) && wait_for_completion_interruptible(
			&thrd->thread_com) == 0) {
			if (atomic_cmpxchg(&thrd->thread_stop, 1, 0) == 1) {
				pr_info("thread %s should stop.\n", thrd->thread_name);
				break;
			}
			pr_info("thread %s trigger\n", thrd->thread_name);
			thrd->proc_func(thrd->ctx_handle);
		} else {
			pr_debug("thread %s exit!", thrd->thread_name);
			break;
		}
	}
	pr_info("%s thread stop.\n", thrd->thread_name);
	complete(&thrd->thread_stop_com);

	return 0;
}

static int camcore_thread_create(void *ctx_handle,
	struct cam_thread_info *thrd, proc_func func)
{
	thrd->ctx_handle = ctx_handle;
	thrd->proc_func = func;
	atomic_set(&thrd->thread_stop, 0);
	init_completion(&thrd->thread_com);
	init_completion(&thrd->thread_stop_com);
	thrd->thread_task = kthread_run(camcore_thread_loop,
		thrd, "%s", thrd->thread_name);
	if (IS_ERR_OR_NULL(thrd->thread_task)) {
		pr_err("fail to start thread %s\n", thrd->thread_name);
		thrd->thread_task = NULL;
		return -EFAULT;
	}
	return 0;
}

static void camcore_thread_stop(struct cam_thread_info *thrd)
{
	if (thrd->thread_task) {
		atomic_set(&thrd->thread_stop, 1);
		complete(&thrd->thread_com);
		wait_for_completion(&thrd->thread_stop_com);
		thrd->thread_task = NULL;
	}
}

static int camcore_raw_proc_done(struct camera_module *module)
{
	int ret = 0;
	int isp_ctx_id, isp_path_id;
	unsigned long flag = 0;
	struct camera_group *grp = module->grp;
	struct channel_context *ch;
	struct channel_context *ch_raw;
	struct dcam_pipe_dev *dev = NULL;

	pr_info("cam%d start\n", module->idx);

	module->cap_status = CAM_CAPTURE_STOP;
	module->dcam_cap_status = DCAM_CAPTURE_STOP;
	atomic_set(&module->state, CAM_STREAM_OFF);
	dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;

	if (atomic_read(&module->timeout_flag) == 1)
		pr_err("fail to raw proc, timeout\n");

	ret = module->dcam_dev_handle->dcam_pipe_ops->stop(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], DCAM_STOP);
	camcore_timer_stop(&module->cam_timer);

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
		DCAM_IOCTL_DEINIT_STATIS_Q, NULL);
	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
		DCAM_IOCTL_PUT_RESERV_STATSBUF, NULL);

	ch_raw = &module->channel[CAM_CH_RAW];
	if (ch_raw->enable !=0) {

		module->dcam_dev_handle->dcam_pipe_ops->put_path(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			ch_raw->dcam_path_id);

		isp_ctx_id = ch_raw->isp_ctx_id;
		isp_path_id = ch_raw->isp_path_id;

		module->isp_dev_handle->isp_ops->put_path(module->isp_dev_handle,
			isp_ctx_id, isp_path_id);
		module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);

		ch_raw->enable = 0;
		ch_raw->dcam_path_id = -1;
		ch_raw->isp_ctx_id = -1;
		ch_raw->isp_path_id = -1;
		ch_raw->aux_dcam_path_id = -1;
		ch_raw->ch_uinfo.dcam_raw_fmt = -1;
		ch_raw->ch_uinfo.sensor_raw_fmt = -1;
	}

	ch = &module->channel[CAM_CH_CAP];
	module->dcam_dev_handle->dcam_pipe_ops->put_path(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
		ch->dcam_path_id);

	if (dev->hw->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW)
		module->dcam_dev_handle->dcam_pipe_ops->put_path(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
			DCAM_PATH_RAW);

	isp_ctx_id = ch->isp_ctx_id;
	isp_path_id = ch->isp_path_id;
	module->isp_dev_handle->isp_ops->put_path(module->isp_dev_handle,
		isp_ctx_id, isp_path_id);
	module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, isp_ctx_id);

	if (module->cam_uinfo.is_pyr_dec) {
		if (ch->pyr_dec_buf) {
			camcore_k_frame_put(ch->pyr_dec_buf);
			ch->pyr_dec_buf = NULL;
		}
		if (ch->pyr_rec_buf) {
			camcore_k_frame_put(ch->pyr_rec_buf);
			ch->pyr_rec_buf = NULL;
		}
	}

	ch->enable = 0;
	ch->dcam_path_id = -1;
	ch->isp_path_id = -1;
	ch->aux_dcam_path_id = -1;
	ch->ch_uinfo.dcam_raw_fmt = -1;
	ch->ch_uinfo.sensor_raw_fmt = -1;

	cam_queue_clear(&ch->share_buf_queue, struct camera_frame, list);
	module->cam_uinfo.dcam_slice_mode = CAM_SLICE_NONE;
	module->cam_uinfo.slice_num = 0;


	dev->sw_ctx[module->offline_cxt_id].raw_fetch_num = 0;
	dev->sw_ctx[module->offline_cxt_id].raw_fetch_count = 0;
	atomic_set(&module->state, CAM_IDLE);
	if (dev->sw_ctx[module->offline_cxt_id].rps == 1)
		dcam_core_context_unbind(&dev->sw_ctx[module->offline_cxt_id]);
	pr_info("camera%d rawproc done.\n", module->idx);

	spin_lock_irqsave(&grp->rawproc_lock, flag);
	if (grp->rawproc_in == 0)
		pr_err("fail to raw proc, cam%d rawproc_in should be 1 here.\n", module->idx);
	grp->rawproc_in = 0;
	spin_unlock_irqrestore(&grp->rawproc_lock, flag);

	/* stop raw dump */
	if (module->dump_thrd.thread_task) {
		camcore_dumpraw_deinit(module);
		complete(&module->dump_thrd.thread_com);
		/* default 0, hal set 1 when needed */
	}

	module->dcam_dev_handle->dcam_pipe_ops->close(module->aux_dcam_dev);

	ret = dcam_core_pipe_dev_put(module->dcam_dev_handle);
	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;

	return ret;
}

/* build channel/path in pre-processing */
static int camcore_raw_pre_proc(
		struct camera_module *module,
		struct isp_raw_proc_info *proc_info)
{
	int ret = 0;
	int ctx_id = 0, dcam_path_id = 0, isp_path_id = 0;
	uint32_t loop = 0, pyr_layer_num = 0;
	unsigned long flag = 0;
	struct camera_group *grp = module->grp;
	struct cam_hw_info *hw = grp->hw_info;
	struct channel_context *ch = NULL;
	struct img_trim path_trim;
	struct dcam_path_cfg_param ch_desc = {0};
	struct dcam_path_cfg_param ch_raw_path_desc;
	struct isp_ctx_compress_desc ctx_compression_desc;
	struct isp_ctx_base_desc ctx_desc;
	struct isp_ctx_size_desc ctx_size;
	struct isp_path_base_desc isp_path_desc;
	struct isp_init_param init_param;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct camera_frame *pframe = NULL;

	pr_info("cam%d in. module:%px,  grp %px, %px\n",
		module->idx, module, grp, &grp->rawproc_in);

	do {
		spin_lock_irqsave(&grp->rawproc_lock, flag);
		if (grp->rawproc_in == 0) {
			grp->rawproc_in = 1;
			spin_unlock_irqrestore(&grp->rawproc_lock, flag);
			pr_info("cam%d get rawproc_in\n", module->idx);
			break;
		} else {
			spin_unlock_irqrestore(&grp->rawproc_lock, flag);
			pr_info("cam%d will wait. loop %d\n", module->idx, loop);
			loop++;
			msleep(10);
		}
	} while (loop < 2000);

	if (loop >= 1000) {
		pr_err("fail to raw proc, wait another camera raw proc\n");
		return -EFAULT;
	}
	/* not care 4in1 */
	ch = &module->channel[CAM_CH_CAP];
	ch->dcam_path_id = -1;
	ch->isp_ctx_id = -1;
	ch->isp_path_id = -1;
	ch->aux_dcam_path_id = -1;
	ch->ch_uinfo.dcam_raw_fmt = -1;
	ch->ch_uinfo.sensor_raw_fmt = -1;
	dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;
	sw_ctx = &dev->sw_ctx[module->offline_cxt_id];
	if (proc_info->scene == RAW_PROC_SCENE_HWSIM) {
		while (cam_queue_cnt_get(&sw_ctx->proc_queue)) {
			pframe = cam_queue_dequeue_tail(&sw_ctx->proc_queue);
			cam_buf_iommu_unmap(&pframe->buf);
		}
		while (cam_queue_cnt_get(&sw_ctx->in_queue)) {
			pframe = cam_queue_dequeue_tail(&sw_ctx->in_queue);
		}
	}

	dev = module->aux_dcam_dev;
	if (dev == NULL) {
		dev = dcam_core_pipe_dev_get(grp->hw_info);
		if (IS_ERR_OR_NULL(dev)) {
			pr_err("fail to get dcam\n");
			return -EFAULT;
		}
		module->aux_dcam_dev = dev;
	}
	module->aux_dcam_id = module->dcam_idx;

	ret = module->dcam_dev_handle->dcam_pipe_ops->open(dev);
	if (ret < 0) {
		pr_err("fail to open aux dcam dev\n");
		ret = -EFAULT;
		goto open_fail;
	}

	if ((module->grp->hw_info->prj_id == SHARKL3)
		&& proc_info->src_size.width > ISP_WIDTH_MAX
		&& proc_info->dst_size.width > ISP_WIDTH_MAX) {

		ch->ch_uinfo.src_size.w = proc_info->src_size.width;
		ch->ch_uinfo.src_size.h = proc_info->src_size.height;
		ch->ch_uinfo.dst_size.w = proc_info->dst_size.width;
		ch->ch_uinfo.dst_size.h = proc_info->dst_size.height;
		module->cam_uinfo.dcam_slice_mode = CAM_OFFLINE_SLICE_SW;
		module->cam_uinfo.slice_num = camcore_slice_num_info_get(&ch->ch_uinfo.src_size,
			&ch->ch_uinfo.dst_size);
		module->cam_uinfo.slice_count = 0;
		module->auto_3dnr = 0;

		sw_ctx->dcam_slice_mode = module->cam_uinfo.dcam_slice_mode;
		sw_ctx->slice_num = module->cam_uinfo.slice_num;
		sw_ctx->slice_count = 0;
		pr_debug("slice_num %d\n", module->cam_uinfo.slice_num);
	}

	/* specify dcam path */
	dcam_path_id = module->dcam_dev_handle->hw->ip_dcam[DCAM_HW_CONTEXT_0]->aux_dcam_path;
	ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(
		&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], dcam_path_id);
	if (ret < 0) {
		pr_err("fail to get dcam path %d\n", dcam_path_id);
		ret = -EFAULT;
		goto get_dcam_path_fail;
	}
		ch->dcam_path_id = dcam_path_id;

	dev->sw_ctx[module->offline_cxt_id].fetch.fmt= DCAM_STORE_RAW_BASE;
	if ((ch->ch_uinfo.sensor_raw_fmt >= DCAM_RAW_PACK_10) && (ch->ch_uinfo.sensor_raw_fmt < DCAM_RAW_MAX))
		dev->sw_ctx[module->offline_cxt_id].pack_bits = ch->ch_uinfo.sensor_raw_fmt;
	else {
		dev->sw_ctx[module->offline_cxt_id].pack_bits = hw->ip_dcam[0]->sensor_raw_fmt;
		if (module->cam_uinfo.dcam_slice_mode && hw->ip_dcam[0]->save_band_for_bigsize) {
			/* for save data band, ultra res not need sensor raw of raw14*/
			dev->sw_ctx[module->offline_cxt_id].pack_bits = DCAM_RAW_PACK_10;
			if (module->raw_cap_fetch_fmt != DCAM_RAW_MAX && proc_info->src_size.width <= DCAM_64M_WIDTH)
				dev->sw_ctx[module->offline_cxt_id].pack_bits = module->raw_cap_fetch_fmt;
		}
		if (dcam_path_id == 0 && module->cam_uinfo.is_4in1 == 1)
			dev->sw_ctx[module->offline_cxt_id].pack_bits = DCAM_RAW_PACK_10;
	}
	ch->ch_uinfo.sensor_raw_fmt = dev->sw_ctx[module->offline_cxt_id].pack_bits;

	/* config dcam path  */
	memset(&ch_desc, 0, sizeof(ch_desc));

	if(ch->dcam_path_id == 0 && module->cam_uinfo.is_4in1 == 1)
		ch_desc.raw_fmt = DCAM_RAW_PACK_10;
 	else if ((ch->ch_uinfo.dcam_raw_fmt >= DCAM_RAW_PACK_10) && (ch->ch_uinfo.dcam_raw_fmt < DCAM_RAW_MAX))
		ch_desc.raw_fmt = ch->ch_uinfo.dcam_raw_fmt;
	else {
		ch_desc.raw_fmt = dev->hw->ip_dcam[0]->raw_fmt_support[0];
		if (dev->sw_ctx[module->cur_sw_ctx_id].dcam_slice_mode && dev->hw->ip_dcam[0]->save_band_for_bigsize)
			ch_desc.raw_fmt = DCAM_RAW_PACK_10;
		ch->ch_uinfo.dcam_raw_fmt = ch_desc.raw_fmt;
	}
	ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
	ch_desc.raw_cap = 1;
	ch_desc.endian.y_endian = ENDIAN_LITTLE;
	if (hw->prj_id == QOGIRN6pro || hw->prj_id == QOGIRN6L) {
		ch_desc.dcam_out_bits = module->grp->hw_info->ip_dcam[0]->dcam_output_support[0];
		ch_desc.dcam_out_fmt = DCAM_STORE_YVU420;
	} else
		ch_desc.dcam_out_fmt = DCAM_STORE_RAW_BASE;

	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
		DCAM_PATH_CFG_BASE, ch->dcam_path_id, &ch_desc);
	pr_debug("dcam fetch packbit %d path%d raw fmt %d, default raw fmt %d\n",
		dev->sw_ctx[module->offline_cxt_id].pack_bits, ch->dcam_path_id, ch->ch_uinfo.dcam_raw_fmt,
		 dev->hw->ip_dcam[0]->raw_fmt_support[0]);

	ch_desc.input_size.w = proc_info->src_size.width;
	ch_desc.input_size.h = proc_info->src_size.height;
	ch_desc.input_trim.start_x = 0;
	ch_desc.input_trim.start_y = 0;
	ch_desc.input_trim.size_x = ch_desc.input_size.w;
	ch_desc.input_trim.size_y = ch_desc.input_size.h;
	ch->trim_dcam.size_x = ch_desc.input_size.w;
	ch->trim_dcam.size_y = ch_desc.input_size.h;
	ch_desc.output_size = ch_desc.input_size;
	ch_desc.priv_size_data = NULL;
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
		DCAM_PATH_CFG_SIZE, ch->dcam_path_id, &ch_desc);

	if (hw->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW){
		ret = module->dcam_dev_handle->dcam_pipe_ops->get_path(
			&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], DCAM_PATH_RAW);
		if (ret) {
			pr_err("fail to get dcam raw path\n");
			ret = -EFAULT;
			goto get_dcam_path_fail;
		}
		memcpy(&ch_raw_path_desc, &ch_desc, sizeof(struct dcam_path_cfg_param));

		if (dev->sw_ctx[module->cur_sw_ctx_id].dcam_slice_mode && dev->hw->ip_dcam[0]->save_band_for_bigsize) {
			ch_raw_path_desc.raw_fmt = DCAM_RAW_PACK_10;
			if (module->raw_cap_fetch_fmt != DCAM_RAW_MAX)
				ch_raw_path_desc.raw_fmt = module->raw_cap_fetch_fmt;
		}
		ch->ch_uinfo.dcam_raw_fmt = ch_raw_path_desc.raw_fmt;

		ch_raw_path_desc.dcam_out_fmt = DCAM_STORE_RAW_BASE;
		ch_raw_path_desc.is_raw = 0;
		if ((g_dcam_raw_src >= ORI_RAW_SRC_SEL) && (g_dcam_raw_src < MAX_RAW_SRC_SEL))
			ch_raw_path_desc.raw_src = g_dcam_raw_src;
		else
			ch_raw_path_desc.raw_src = PROCESS_RAW_SRC_SEL;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_BASE, DCAM_PATH_RAW, &ch_raw_path_desc);
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_SIZE, DCAM_PATH_RAW, &ch_raw_path_desc);
	}
	if (sw_ctx->rps == 1){
		ret = dcam_core_context_bind(sw_ctx, hw->csi_connect_type, module->aux_dcam_id);
		if (ret)
			pr_err("fail to get hw_ctx_id\n");
	}
	/* specify isp context & path */
	init_param.is_high_fps = 0;/* raw capture + slow motion ?? */
	init_param.cam_id = module->idx;
	init_param.blkparam_node_num = 1;
	ret = module->isp_dev_handle->isp_ops->get_context(module->isp_dev_handle, &init_param);
	if (ret < 0) {
		pr_err("fail to get isp context\n");
		goto fail_ispctx;
	}
	ctx_id = ret;
	module->isp_dev_handle->isp_ops->set_callback(module->isp_dev_handle,
		ctx_id, camcore_isp_callback, module);

	/* config isp context base */
	memset(&ctx_desc, 0, sizeof(ctx_desc));
	ctx_desc.pack_bits = ch->ch_uinfo.dcam_raw_fmt;
	ctx_desc.in_fmt = proc_info->src_format;
	if (hw->prj_id == QOGIRN6pro || hw->prj_id == QOGIRN6L) {
		ctx_desc.in_fmt = camcore_format_dcam_translate(ch_desc.dcam_out_fmt);
		ctx_desc.data_in_bits = ch_desc.dcam_out_bits;
	}
	ch->ch_uinfo.dcam_out_pack= 0;
	if ((ch_desc.dcam_out_fmt == DCAM_STORE_RAW_BASE) && (ctx_desc.pack_bits == DCAM_RAW_PACK_10))
		ch->ch_uinfo.dcam_out_pack = 1;
	if ((ch_desc.dcam_out_fmt & DCAM_STORE_YUV_BASE) && (ch_desc.dcam_out_bits == DCAM_STORE_10_BIT))
		ch->ch_uinfo.dcam_out_pack = 1;
	ctx_desc.pyr_data_bits = DCAM_STORE_10_BIT;
	ctx_desc.pyr_is_pack = PYR_IS_PACK;
	ctx_desc.is_pack = ch->ch_uinfo.dcam_out_pack;
	ctx_desc.bayer_pattern = proc_info->src_pattern;
	ctx_desc.mode_ltm = MODE_LTM_OFF;
	ctx_desc.mode_gtm = MODE_GTM_OFF;
	if (sw_ctx->rps == 1){
		if (module->cam_uinfo.is_rgb_gtm) {
			ch->gtm_rgb = 1;
			ctx_desc.gtm_rgb = 1;
			ctx_desc.mode_gtm = MODE_GTM_PRE;
			module->simulator = 1;
			module->isp_dev_handle->sw_ctx[ctx_id]->rps = 1;
		}
	}

	ctx_desc.mode_3dnr = MODE_3DNR_OFF;
	ctx_desc.ch_id = CAM_CH_CAP;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_CTX_BASE, ctx_id, 0, &ctx_desc);

	ctx_compression_desc.fetch_fbd = 0;
	ctx_compression_desc.fetch_fbd_4bit_bypass = 0;
	ctx_compression_desc.nr3_fbc_fbd = 0;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_CTX_COMPRESSION, ctx_id, 0, &ctx_compression_desc);

	isp_path_id = ISP_SPATH_CP;
	ret = module->isp_dev_handle->isp_ops->get_path(
		module->isp_dev_handle, ctx_id, isp_path_id);
	if (ret < 0) {
		pr_err("fail to get isp path %d from context %d\n",
			isp_path_id, ctx_id);
		goto fail_isppath;
	}
	ch->isp_ctx_id = (int32_t)(ctx_id);
	ch->isp_path_id = (int32_t)(isp_path_id);
	pr_info("get isp path : 0x%x\n", ch->isp_path_id);

	memset(&isp_path_desc, 0, sizeof(isp_path_desc));
	isp_path_desc.out_fmt = IMG_PIX_FMT_NV21;
	isp_path_desc.endian.y_endian = ENDIAN_LITTLE;
	isp_path_desc.endian.uv_endian = ENDIAN_LITTLE;
	isp_path_desc.output_size.w = proc_info->dst_size.width;
	isp_path_desc.output_size.h = proc_info->dst_size.height;
	isp_path_desc.data_bits = DCAM_STORE_10_BIT;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_PATH_BASE, ctx_id, isp_path_id, &isp_path_desc);

	/* config isp input/path size */
	ctx_size.src.w = proc_info->src_size.width;
	ctx_size.src.h = proc_info->src_size.height;
	ctx_size.crop.start_x = 0;
	ctx_size.crop.start_y = 0;
	ctx_size.crop.size_x = ctx_size.src.w;
	ctx_size.crop.size_y = ctx_size.src.h;
	ctx_size.zoom_conflict_with_ltm = module->cam_uinfo.zoom_conflict_with_ltm;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_CTX_SIZE, ctx_id, 0, &ctx_size);

	path_trim.start_x = 0;
	path_trim.start_y = 0;
	path_trim.size_x = proc_info->src_size.width;
	path_trim.size_y = proc_info->src_size.height;
	ch->trim_isp.size_x = path_trim.size_x;
	ch->trim_isp.size_y = path_trim.size_y;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_PATH_SIZE, ctx_id, isp_path_id, &path_trim);

	if (module->cam_uinfo.is_pyr_dec) {
		pyr_layer_num = ISP_PYR_DEC_LAYER_NUM;
		module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
			ctx_id, ISP_IOCTL_CFG_PYR_REC_NUM, &pyr_layer_num);
	}

	ch->enable = 1;
	ch->ch_uinfo.dst_fmt = isp_path_desc.out_fmt;
	atomic_set(&module->state, CAM_CFG_CH);
	pr_info("done, dcam path %d, isp_path 0x%x\n",
		ch->dcam_path_id, ch->isp_path_id);
	pr_debug("cam%d sw_id %d is_pyr_rec %d\n", module->idx,
			sw_ctx->sw_ctx_id, module->cam_uinfo.is_pyr_rec);
	return 0;

fail_isppath:
	module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle, ctx_id);
fail_ispctx:
	module->dcam_dev_handle->dcam_pipe_ops->put_path(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], ch->dcam_path_id);
	ch->dcam_path_id = -1;
	ch->isp_ctx_id = -1;
	ch->isp_path_id = -1;
	ch->aux_dcam_path_id = -1;
get_dcam_path_fail:
	module->dcam_dev_handle->dcam_pipe_ops->close(module->aux_dcam_dev);
open_fail:
	dcam_core_pipe_dev_put(module->aux_dcam_dev);
	module->aux_dcam_dev = NULL;
	module->aux_dcam_id = DCAM_HW_CONTEXT_MAX;

	pr_err("fail to call pre raw proc\n");
	return ret;
}

static int camcore_raw_post_proc(struct camera_module *module,
		struct isp_raw_proc_info *proc_info)
{
	int ret = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t pack_bits = 0, dcam_out_bits = 0;
	uint32_t size = 0;
	struct channel_context *ch = NULL;
	struct camera_frame *src_frame = NULL;
	struct camera_frame *mid_frame = NULL;
	struct camera_frame *mid_yuv_frame = NULL;
	struct camera_frame *dst_frame = NULL;
	struct camera_frame *pframe = NULL;
	struct dcam_pipe_dev *dev = NULL;
	timespec cur_ts;
	struct dcam_sw_context *sw_ctx = NULL;

	memset(&cur_ts, 0, sizeof(timespec));
	pr_info("start\n");

	dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;
	sw_ctx = &dev->sw_ctx[module->offline_cxt_id];

	ch = &module->channel[CAM_CH_CAP];
	if (ch->enable == 0) {
		pr_err("fail to get channel enable state\n");
		return -EFAULT;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->start(sw_ctx, 0);
	if (ret < 0) {
		pr_err("fail to start dcam dev, ret %d\n", ret);
		return -EFAULT;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx,
		DCAM_IOCTL_INIT_STATIS_Q, NULL);

	pr_info("src %d 0x%x, mid %d, 0x%x, dst %d, 0x%x\n",
		proc_info->fd_src, proc_info->src_offset,
		proc_info->fd_dst0, proc_info->dst0_offset,
		proc_info->fd_dst1, proc_info->dst1_offset);
	src_frame = cam_queue_empty_frame_get();
	src_frame->buf.type = CAM_BUF_USER;
	src_frame->buf.mfd[0] = proc_info->fd_src;
	src_frame->buf.offset[0] = proc_info->src_offset;
	src_frame->channel_id = ch->ch_id;
	src_frame->width = proc_info->src_size.width;
	src_frame->height = proc_info->src_size.height;
	src_frame->endian = proc_info->src_y_endian;
	src_frame->pattern = proc_info->src_pattern;
	ktime_get_ts(&cur_ts);
	src_frame->sensor_time.tv_sec = cur_ts.tv_sec;
	src_frame->sensor_time.tv_usec = cur_ts.tv_nsec / NSEC_PER_USEC;
	src_frame->time = src_frame->sensor_time;
	src_frame->boot_time = ktime_get_boottime();
	src_frame->boot_sensor_time = src_frame->boot_time;
	ret = cam_buf_ionbuf_get(&src_frame->buf);
	if (ret)
		goto src_fail;

	dst_frame = cam_queue_empty_frame_get();
	dst_frame->buf.type = CAM_BUF_USER;
	dst_frame->buf.mfd[0] = proc_info->fd_dst1;
	dst_frame->buf.offset[0] = proc_info->dst1_offset;
	dst_frame->channel_id = ch->ch_id;
	dst_frame->img_fmt = ch->ch_uinfo.dst_fmt;
	dst_frame->sensor_time = src_frame->sensor_time;
	dst_frame->time = src_frame->time;
	dst_frame->boot_time = src_frame->boot_time;
	dst_frame->boot_sensor_time = src_frame->boot_sensor_time;
	ret = cam_buf_ionbuf_get(&dst_frame->buf);
	if (ret)
		goto dst_fail;

	if (module->grp->hw_info->prj_id == SHARKL3
		&& module->dcam_idx == DCAM_ID_1)
		sw_ctx->raw_fetch_num = 2;
	else
		sw_ctx->raw_fetch_num = 1;
	sw_ctx->raw_fetch_count = 0;

	mid_frame = cam_queue_empty_frame_get();
	mid_frame->channel_id = ch->ch_id;
	/* if user set this buffer, we use it for dcam output
	 * or else we will allocate one for it.
	 */
	if(ch->dcam_path_id == 0 && module->cam_uinfo.is_4in1 == 1)
		pack_bits = DCAM_RAW_PACK_10;
	else
		pack_bits = ch->ch_uinfo.dcam_raw_fmt;

	pr_info("day raw_proc_post pack_bits %d", pack_bits);
	if (proc_info->fd_dst0 > 0) {
		mid_frame->buf.type = CAM_BUF_USER;
		mid_frame->buf.mfd[0] = proc_info->fd_dst0;
		mid_frame->buf.offset[0] = proc_info->dst0_offset;
		ret = cam_buf_ionbuf_get(&mid_frame->buf);
		if (ret)
			goto mid_fail;
	} else {
		width = proc_info->src_size.width;
		height = proc_info->src_size.height;
		if (module->cam_uinfo.dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
			width = width / module->cam_uinfo.slice_num;
			if (proc_info->dst_size.height > DCAM_SW_SLICE_HEIGHT_MAX)
				width *= 2;
			width = ALIGN(width, 4);
		}

		if (proc_info->src_format == IMG_PIX_FMT_GREY)
			size = cal_sprd_raw_pitch(width, pack_bits) * height;
		else
			size = width * height * 3;
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		ret = cam_buf_alloc(&mid_frame->buf, (size_t)size, module->iommu_enable);
		if (ret)
			goto mid_fail;
	}
	mid_frame->sensor_time = src_frame->sensor_time;
	mid_frame->time = src_frame->time;
	mid_frame->boot_time = src_frame->boot_time;
	mid_frame->boot_sensor_time = src_frame->boot_sensor_time;

	mid_frame->need_gtm_hist = module->cam_uinfo.is_rgb_gtm;
	mid_frame->need_gtm_map = module->cam_uinfo.is_rgb_gtm;
	mid_frame->gtm_mod_en = module->cam_uinfo.is_rgb_gtm;

	if (dev->hw->ip_dcam[0]->dcam_raw_path_id == DCAM_PATH_RAW) {
		width = cal_sprd_yuv_pitch(proc_info->src_size.width, 10 , 1);
		height = proc_info->src_size.height;
		size = width * height * 3 / 2;
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		mid_yuv_frame = cam_queue_empty_frame_get();
		memcpy(mid_yuv_frame, mid_frame, sizeof(struct camera_frame));
		memset(&mid_yuv_frame->buf, 0, sizeof(struct camera_buf));
		ret = cam_buf_alloc(&mid_yuv_frame->buf, (size_t)size, module->iommu_enable);
		if (ret) {
			pr_err("fail to alloc mid yuv buffer\n");
			goto dcam_out_fail;
		}
		mid_yuv_frame->width = width;
		mid_yuv_frame->height = height;
		if (module->cam_uinfo.is_pyr_dec)
			mid_yuv_frame->need_pyr_dec = 1;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_OUTPUT_BUF, ch->dcam_path_id, mid_yuv_frame);
		ret |= module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_OUTPUT_BUF, DCAM_PATH_RAW, mid_frame);
	} else
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_OUTPUT_BUF, ch->dcam_path_id, mid_frame);

	if (ret) {
		pr_err("fail to cfg dcam out buffer.\n");
		goto dcam_out_fail;
	}

	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_OUTPUT_BUF, ch->isp_ctx_id, ch->isp_path_id, dst_frame);
	if (ret)
		pr_err("fail to cfg isp out buffer.\n");

	pr_info("raw proc, src %px, mid %px, dst %px, mid yuv %px\n",
		src_frame, mid_frame, dst_frame, mid_yuv_frame);
	cam_queue_init(&ch->share_buf_queue,
			CAM_SHARED_BUF_NUM, camcore_k_frame_put);
	module->cap_status = CAM_CAPTURE_RAWPROC;
	module->dcam_cap_status = DCAM_CAPTURE_START;
	atomic_set(&module->state, CAM_RUNNING);

	if (module->dump_thrd.thread_task) {
		camcore_dumpraw_init(module);
		camdump_start(&module->dump_thrd, &module->dump_base, module->dcam_idx);
		pr_debug("cam%d_dumpraw start\n", module->idx);
	}

	if (module->cam_uinfo.is_pyr_dec) {
		/* dec out buf for raw capture */
		width = proc_info->src_size.width;
		height = proc_info->src_size.height;
		dcam_out_bits = module->grp->hw_info->ip_dcam[0]->dcam_output_support[0];
		size = dcam_if_cal_pyramid_size(width, height, dcam_out_bits, 1, 0, ISP_PYR_DEC_LAYER_NUM);
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		pframe = cam_queue_empty_frame_get();
		pframe->width = width;
		pframe->height = height;
		pframe->channel_id = ch->ch_id;
		pframe->data_src_dec = 1;
		ret = cam_buf_alloc(&pframe->buf, size, module->iommu_enable);
		if (ret) {
			pr_err("fail to alloc raw dec buf\n");
			cam_queue_empty_frame_put(pframe);
			goto dcam_out_fail;
		}
		ch->pyr_dec_buf = pframe;
		pr_debug("hw_ctx_id %d, pyr_dec size %d, buf %p, w %d h %d\n",
			sw_ctx->hw_ctx_id, size, pframe, width, height);
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PYR_DEC_BUF,
				ch->isp_ctx_id, ch->isp_path_id, ch->pyr_dec_buf);

		/* rec temp buf for raw capture */
		width = proc_info->src_size.width;
		height = proc_info->src_size.height;
		width = isp_rec_layer0_width(width, ISP_PYR_DEC_LAYER_NUM);
		height = isp_rec_layer0_heigh(height, ISP_PYR_DEC_LAYER_NUM);
		size = dcam_if_cal_pyramid_size(width, height, dcam_out_bits, 1, 1, ISP_PYR_DEC_LAYER_NUM - 1);
		size = ALIGN(size, CAM_BUF_ALIGN_SIZE);
		pframe = cam_queue_empty_frame_get();
		pframe->width = width;
		pframe->height = height;
		ret = cam_buf_alloc(&pframe->buf, size, module->iommu_enable);
		if (ret) {
			pr_err("fail to alloc rec buf\n");
			cam_queue_empty_frame_put(pframe);
			atomic_inc(&ch->err_status);
			goto dec_fail;
		}
		ch->pyr_rec_buf = pframe;
		pr_debug("hw_ctx_id %d, pyr_rec w %d, h %d, buf %p\n",
				sw_ctx->hw_ctx_id, width, height, pframe);
		module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
					ISP_PATH_CFG_PYR_REC_BUF,
					ch->isp_ctx_id, ch->isp_path_id, ch->pyr_rec_buf);
	}

	ret = camcore_frame_start_proc(module, src_frame);
	if (ret)
		pr_err("fail to start dcam/isp for raw proc\n");

	atomic_set(&module->timeout_flag, 1);
	ret = camcore_timer_start(&module->cam_timer, CAMERA_TIMEOUT);

	return ret;

dec_fail:
	if (ch->pyr_dec_buf) {
		if (ch->pyr_dec_buf->buf.dmabuf_p[0])
			cam_buf_free(&ch->pyr_dec_buf->buf);
		cam_queue_empty_frame_put(ch->pyr_dec_buf);
	}
dcam_out_fail:
	if (mid_yuv_frame) {
		if (mid_yuv_frame->buf.dmabuf_p[0])
			cam_buf_free(&mid_yuv_frame->buf);
		cam_queue_empty_frame_put(mid_yuv_frame);
	}

	if (mid_frame->buf.type == CAM_BUF_USER)
		cam_buf_ionbuf_put(&mid_frame->buf);
	else
		cam_buf_free(&mid_frame->buf);
mid_fail:

	cam_queue_empty_frame_put(mid_frame);
	cam_buf_ionbuf_put(&dst_frame->buf);
dst_fail:
	cam_queue_empty_frame_put(dst_frame);
	cam_buf_ionbuf_put(&src_frame->buf);
src_fail:
	cam_queue_empty_frame_put(src_frame);
	ret = module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, DCAM_STOP);
	pr_err("fail to call post raw proc\n");
	return ret;
}

static int camcore_raw_post_proc_new(
		struct camera_module *module,
		struct isp_raw_proc_info *proc_info)
{
	int ret = 0;
	uint32_t pack_bits;
	struct channel_context *ch;
	struct camera_frame *src_frame;
	struct camera_frame *mid_frame;
	struct camera_frame *dst_frame;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	timespec cur_ts;

	memset(&cur_ts, 0, sizeof(timespec));
	pr_info("cam%d start\n", module->idx);

	ch = &module->channel[CAM_CH_CAP];
	if (ch->enable == 0) {
		pr_err("fail to get channel enable state\n");
		return -EFAULT;
	}

	pr_info("src %d 0x%x, mid %d, 0x%x, dst %d, 0x%x\n",
		proc_info->fd_src, proc_info->src_offset,
		proc_info->fd_dst0, proc_info->dst0_offset,
		proc_info->fd_dst1, proc_info->dst1_offset);
	src_frame = cam_queue_empty_frame_get();
	src_frame->buf.type = CAM_BUF_USER;
	src_frame->buf.mfd[0] = proc_info->fd_src;
	src_frame->buf.offset[0] = proc_info->src_offset;
	src_frame->channel_id = ch->ch_id;
	src_frame->width = proc_info->src_size.width;
	src_frame->height = proc_info->src_size.height;
	src_frame->endian = proc_info->src_y_endian;
	src_frame->pattern = proc_info->src_pattern;
	ret = cam_buf_ionbuf_get(&src_frame->buf);
	if (ret)
		goto src_fail;

	src_frame->fid = module->simu_fid++;
	ktime_get_ts(&cur_ts);
	src_frame->sensor_time.tv_sec = cur_ts.tv_sec;
	src_frame->sensor_time.tv_usec = cur_ts.tv_nsec / NSEC_PER_USEC;
	src_frame->time = src_frame->sensor_time;
	src_frame->boot_time = ktime_get_boottime();
	src_frame->boot_sensor_time = src_frame->boot_time;

	dst_frame = cam_queue_empty_frame_get();
	dst_frame->buf.type = CAM_BUF_USER;
	dst_frame->buf.mfd[0] = proc_info->fd_dst1;
	dst_frame->buf.offset[0] = proc_info->dst1_offset;
	dst_frame->channel_id = ch->ch_id;
	dst_frame->img_fmt = ch->ch_uinfo.dst_fmt;
	ret = cam_buf_ionbuf_get(&dst_frame->buf);
	if (ret)
		goto dst_fail;

	mid_frame = cam_queue_empty_frame_get();
	mid_frame->channel_id = ch->ch_id;
	pack_bits = ch->ch_uinfo.dcam_raw_fmt;

	pr_info("day raw_proc_post pack_bits %d", pack_bits);
	if (proc_info->fd_dst0 > 0) {
		mid_frame->buf.type = CAM_BUF_USER;
		mid_frame->buf.mfd[0] = proc_info->fd_dst0;
		mid_frame->buf.offset[0] = proc_info->dst0_offset;
		ret = cam_buf_ionbuf_get(&mid_frame->buf);
		if (ret)
			goto mid_fail;
	} else {
		pr_err("fail to get mid buf fd\n");
		goto mid_fail;
	}

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
		DCAM_PATH_CFG_OUTPUT_BUF, ch->dcam_path_id, mid_frame);
	if (ret) {
		pr_err("fail to cfg dcam out buffer.\n");
		goto dcam_out_fail;
	}

	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
		ISP_PATH_CFG_OUTPUT_BUF, ch->isp_ctx_id, ch->isp_path_id, dst_frame);
	if (ret)
		pr_err("fail to cfg isp out buffer.\n");

	pr_info("raw proc, src %p, mid %p, dst %p\n",
		src_frame, mid_frame, dst_frame);

	module->cap_status = CAM_CAPTURE_RAWPROC;
	module->dcam_cap_status = DCAM_CAPTURE_START;

	ret = camcore_frame_start_proc(module, src_frame);
	if (ret)
		pr_err("fail to start dcam/isp for raw proc\n");

	return ret;

dcam_out_fail:
	cam_buf_ionbuf_put(&mid_frame->buf);
mid_fail:
	cam_queue_empty_frame_put(mid_frame);
	cam_buf_ionbuf_put(&dst_frame->buf);
dst_fail:
	cam_queue_empty_frame_put(dst_frame);
	cam_buf_ionbuf_put(&src_frame->buf);
src_fail:
	cam_queue_empty_frame_put(src_frame);
	pr_err("fail to call post raw proc\n");
	return ret;
}

static int camcore_virtual_sensor_proc(
		struct camera_module *module,
		struct isp_raw_proc_info *proc_info)
{
	int ret = 0;
	struct channel_context *ch_pre = NULL, *ch_cap = NULL ;
	struct camera_frame *src_frame = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct dcam_path_cfg_param ch_desc = {0};
	struct camera_uchannel *ch_uinfo = NULL;
	timespec cur_ts = {0};

	sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	sw_ctx->fetch.fmt = DCAM_STORE_RAW_BASE;
	sw_ctx->pack_bits = DCAM_RAW_PACK_10;
	/*for preview path dcam size config*/
	camcore_compression_config(module);

	ch_pre = &module->channel[CAM_CH_PRE];
	if (ch_pre->enable) {
		camcore_pyr_info_config(module, ch_pre);
		ch_uinfo = &ch_pre->ch_uinfo;
		ch_desc.input_size.w = proc_info->src_size.width;
		ch_desc.input_size.h = proc_info->src_size.height;
		ch_desc.zoom_ratio_base = ch_uinfo->zoom_ratio_base;
		ch_desc.input_trim = ch_pre->trim_dcam;
		ch_desc.total_input_trim = ch_pre->total_trim_dcam;
		ch_desc.output_size = ch_pre->dst_dcam;
		ch_desc.priv_size_data = NULL;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CFG_SIZE, ch_pre->dcam_path_id, &ch_desc);
	}
	/*for capture path dcam size config*/
	ch_cap = &module->channel[CAM_CH_CAP];
	if (ch_cap->enable) {
		camcore_pyr_info_config(module, ch_cap);
		ch_uinfo = &ch_cap->ch_uinfo;
		ch_desc.input_size.w = proc_info->src_size.width;
		ch_desc.input_size.h = proc_info->src_size.height;
		ch_desc.zoom_ratio_base = ch_uinfo->zoom_ratio_base;

		if (camcore_capture_sizechoice(module, ch_cap)) {
			ch_desc.input_trim = ch_cap->trim_dcam;
			ch_desc.output_size.w = ch_cap->trim_dcam.size_x;
			ch_desc.output_size.h = ch_cap->trim_dcam.size_y;
		} else {
			ch_desc.output_size = ch_desc.input_size;
			ch_desc.input_trim.start_x = 0;
			ch_desc.input_trim.start_y = 0;
			ch_desc.input_trim.size_x = ch_desc.input_size.w;
			ch_desc.input_trim.size_y = ch_desc.input_size.h;
		}

		ch_desc.priv_size_data = NULL;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CFG_SIZE, ch_cap->dcam_path_id, &ch_desc);
	}

	memset(&cur_ts, 0, sizeof(timespec));

	atomic_set(&sw_ctx->state, STATE_RUNNING);

	src_frame = cam_queue_empty_frame_get();

	if (ch_cap->enable)
		src_frame->channel_id = ch_cap->ch_id;
	else if (ch_pre->enable)
		src_frame->channel_id = ch_pre->ch_id;
	else {
		pr_err("fail to get channel enable state\n");
		return -EFAULT;
	}
	src_frame->buf.type = CAM_BUF_USER;
	src_frame->buf.mfd[0] = proc_info->fd_src;
	src_frame->buf.offset[0] = proc_info->src_offset;
	src_frame->width = proc_info->src_size.width;
	src_frame->height = proc_info->src_size.height;
	src_frame->endian = proc_info->src_y_endian;
	src_frame->pattern = proc_info->src_pattern;
	ret = cam_buf_ionbuf_get(&src_frame->buf);
	if (ret)
		goto virtual_sensor_src_fail;

	src_frame->fid = module->simu_fid++;
	ktime_get_ts(&cur_ts);
	src_frame->sensor_time.tv_sec = cur_ts.tv_sec;
	src_frame->sensor_time.tv_usec = cur_ts.tv_nsec / NSEC_PER_USEC;
	src_frame->time = src_frame->sensor_time;
	src_frame->boot_time = ktime_get_boottime();
	src_frame->boot_sensor_time = src_frame->boot_time;

	ret = camcore_frame_start_proc(module, src_frame);
	if (ret)
		pr_err("fail to start dcam/isp for virtual sensor proc\n");

	return ret;
virtual_sensor_src_fail:
	cam_queue_empty_frame_put(src_frame);
	pr_err("fail to call post raw proc\n");
	return ret;
}

static int camcore_full_raw_switch(struct camera_module *module)
{
	int shutoff = 0;
	struct camera_frame *pframe;
	int total = DUMP_RAW_BUF_NUM, i = 0, ret = 0;
	uint32_t size = 0, pack_bits = 0, dcam_out_bits = 0, pitch = 0, is_pack = 0;
	struct dcam_path_cfg_param ch_desc;
	struct channel_context *ch_raw;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	struct cam_hw_info *hw = module->grp->hw_info;
	shutoff = 1;
	module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx, DCAM_PATH_CFG_SHUTOFF, DCAM_PATH_FULL, &shutoff);

	module->channel[CAM_CH_RAW].enable = 1;
	ch_raw = &module->channel[CAM_CH_RAW];
	ch_raw->ch_uinfo.sensor_raw_fmt = hw->ip_dcam[0]->sensor_raw_fmt;
	ch_raw->ch_uinfo.dcam_raw_fmt = hw->ip_dcam[0]->raw_fmt_support[0];
	camcore_channel_init(module, ch_raw);
	cam_queue_init(&ch_raw->share_buf_queue,
		CAM_SHARED_BUF_NUM, camcore_k_frame_put);
	ch_raw->swap_size.w = ch_raw->ch_uinfo.src_size.w;
	ch_raw->swap_size.h = ch_raw->ch_uinfo.src_size.h;

	memset(&ch_desc, 0, sizeof(ch_desc));
	ch_desc.input_size.w = ch_raw->ch_uinfo.src_size.w;
	ch_desc.input_size.h = ch_raw->ch_uinfo.src_size.h;
	ch_desc.output_size = ch_desc.input_size;
	ch_desc.input_trim.start_x = 0;
	ch_desc.input_trim.start_y = 0;
	ch_desc.input_trim.size_x = ch_desc.input_size.w;
	ch_desc.input_trim.size_y = ch_desc.input_size.h;
	ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_SIZE, ch_raw->dcam_path_id, &ch_desc);

	if ((g_dcam_raw_src >= ORI_RAW_SRC_SEL) && (g_dcam_raw_src < MAX_RAW_SRC_SEL))
		sw_ctx->path[DCAM_PATH_RAW].src_sel = g_dcam_raw_src;
	else
		sw_ctx->path[DCAM_PATH_RAW].src_sel = PROCESS_RAW_SRC_SEL;

	dcam_out_bits = module->cam_uinfo.sensor_if.if_spec.mipi.bits_per_pxl;
	if (ch_raw->aux_dcam_path_id >= 0)
		pack_bits = ch_raw->ch_uinfo.sensor_raw_fmt;
	else
		pack_bits = ch_raw->ch_uinfo.dcam_raw_fmt;
	is_pack = 0;
	if ((ch_raw->dcam_out_fmt & DCAM_STORE_RAW_BASE) && (pack_bits == DCAM_RAW_PACK_10))
		is_pack = 1;
	if ((ch_raw->dcam_out_fmt & DCAM_STORE_YUV_BASE) && (dcam_out_bits == DCAM_STORE_10_BIT))
		is_pack = 1;
	if (ch_raw->dcam_out_fmt & DCAM_STORE_RAW_BASE) {
		size = cal_sprd_raw_pitch(ch_raw->swap_size.w, pack_bits) * ch_raw->swap_size.h;
	} else if ((ch_raw->dcam_out_fmt == DCAM_STORE_YUV420) || (ch_raw->dcam_out_fmt == DCAM_STORE_YVU420)) {
		pitch = cal_sprd_yuv_pitch(ch_raw->swap_size.w, dcam_out_bits, is_pack);
		size = pitch * ch_raw->swap_size.h * 3 / 2;
		pr_debug("ch%d, dcam yuv size %d\n", ch_raw->ch_id, size);
	} else {
		size = ch_raw->swap_size.w * ch_raw->swap_size.h * 3;
	}

	pr_debug("cam%d, ch_id %d, buffer size: %u (%u x %u), num %d\n",
		module->idx, ch_raw->ch_id, size, ch_raw->swap_size.w, ch_raw->swap_size.h, total);

	for (i = 0 ; i < total; i++) {
		pframe = cam_queue_empty_frame_get();
		pframe->channel_id = ch_raw->ch_id;
		pframe->is_compressed = ch_raw->compress_input;
		pframe->compress_4bit_bypass = ch_raw->compress_4bit_bypass;
		pframe->width = ch_raw->swap_size.w;
		pframe->height = ch_raw->swap_size.h;
		pframe->endian = ENDIAN_LITTLE;

		ret = cam_buf_alloc(&pframe->buf, size, module->iommu_enable);
		if (ret) {
			pr_err("fail to alloc buf: %d ch %d\n", i, ch_raw->ch_id);
			cam_queue_empty_frame_put(pframe);
			atomic_inc(&ch_raw->err_status);
			return -ENOMEM;
		}
		pr_debug("dcam_path_id:%d", ch_raw->dcam_path_id);
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
			DCAM_PATH_CFG_OUTPUT_BUF, ch_raw->dcam_path_id, pframe);
		if (ret) {
			pr_err("fail to enqueue out buf: %d ch %d\n", i, ch_raw->ch_id);
			cam_buf_free(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
		} else {
			pr_debug("frame %p,idx %d,cnt %d,phy_addr %p\n",
				pframe, i, (void *)pframe->buf.addr_vir[0]);
		}
	}
	return ret;
}

static int camcore_csi_switch_disconnect(struct camera_module *module, uint32_t mode)
{
	int ret = 0;
	struct cam_hw_info *hw = module->grp->hw_info;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	struct dcam_hw_context *hw_ctx = sw_ctx->hw_ctx;
	struct dcam_switch_param csi_switch = {0};
	struct dcam_path_desc *path = NULL;
	struct camera_frame *frame = NULL;
	uint32_t i = 0,j = 0;
	struct isp_offline_param *in_param = NULL;
	struct camera_frame *pframe = NULL;

	if (sw_ctx->hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
		pr_warn("warning: sw_ctx has been disconnected and unbinded already. sw_ctx_id: %d\n", sw_ctx->sw_ctx_id);
		return 0;
	}
	/* switch disconnect */
	csi_switch.csi_id = module->dcam_idx;
	csi_switch.dcam_id = sw_ctx->hw_ctx_id;
	if (mode == CAM_CSI_RECOVERY_SWITCH)
		csi_switch.is_recovery = 1;

	if (atomic_read(&hw_ctx->user_cnt) > 0) {
		hw->dcam_ioctl(hw, DCAM_HW_DISCONECT_CSI, &csi_switch);
		/* reset */
		hw->dcam_ioctl(hw, DCAM_HW_CFG_STOP, sw_ctx);
		if (!csi_switch.is_recovery)
			hw->dcam_ioctl(hw, DCAM_HW_CFG_RESET, &sw_ctx->hw_ctx_id);
	} else {
		pr_err("fail to get DCAM%d valid user cnt %d\n", hw_ctx->hw_ctx_id, atomic_read(&hw_ctx->user_cnt));
		return -1;
	}
	pr_info("Disconnect csi_id = %d, dcam_id = %d, sw_ctx_id = %d module idx %d mode:%d\n",
		csi_switch.csi_id, csi_switch.dcam_id, sw_ctx->sw_ctx_id, module->idx, mode);

	atomic_set(&sw_ctx->state, STATE_IDLE);
	/* reset */
	dcam_int_tracker_dump(sw_ctx->hw_ctx_id);
	dcam_int_tracker_reset(sw_ctx->hw_ctx_id);

	/* reset result q */
	for (j = 0; j < DCAM_PATH_MAX; j++) {
		path = &sw_ctx->path[j];
		if (path == NULL)
			continue;
		frame = cam_queue_del_tail(&path->result_queue, struct camera_frame, list);
		while (frame) {
			pr_debug("DCAM%u path%d fid %u\n", sw_ctx->sw_ctx_id, j, frame->fid);

			in_param = (struct isp_offline_param *)frame->param_data;
			if (in_param) {
				if (path->path_id == DCAM_PATH_BIN) {
					struct channel_context *channel = &module->channel[frame->channel_id];
					if (!channel) {
						pr_err("fail to get channel,path_id:%d,channel_id:%d,frame->is_reserved:%d", path->path_id, frame->channel_id, frame->is_reserved);
					} else {
						in_param->prev = channel->isp_updata;
						channel->isp_updata = in_param;
						frame->param_data = NULL;
						pr_debug("store:  cur %p   prev %p\n", in_param, channel->isp_updata);
					}
				} else {
					struct isp_offline_param *prev = NULL;
					while (in_param) {
						prev = (struct isp_offline_param *)in_param->prev;
						kfree(in_param);
						in_param = prev;
					}
					frame->param_data = NULL;
				}
			}

			if (frame->is_reserved)
				cam_queue_enqueue(&path->reserved_buf_queue, &frame->list);
			else {
				cam_queue_enqueue_front(&path->out_buf_queue, &frame->list);
				if (path->path_id == DCAM_PATH_FULL)
					cam_buf_iommu_unmap(&frame->buf);
			}
			frame = cam_queue_del_tail(&path->result_queue, struct camera_frame, list);
		}
	}

	for(i = 0; i < CAM_CH_MAX; i++) {
		if (module->channel[i].enable) {
			ret = module->isp_dev_handle->isp_ops->clear_blk_param_q(module->isp_dev_handle, module->channel[i].isp_ctx_id);
			if (ret)
				pr_err("fail to recycle cam%d ch %d blk param node\n", module->idx, i);
		}
	}
	/* unbind */
	if (mode != CAM_CSI_RECOVERY_SWITCH)
		dcam_core_context_unbind(sw_ctx);
	if (module->channel[CAM_CH_CAP].enable) {
		if (module->cam_uinfo.need_share_buf) {
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CLR_OUTPUT_SHARE_BUF,
				DCAM_PATH_FULL, sw_ctx);
			do {
				pframe = cam_queue_dequeue(&module->channel[CAM_CH_CAP].share_buf_queue,
					struct camera_frame, list);
				if (pframe == NULL)
					break;
				camcore_share_buf_cfg(SHARE_BUF_SET_CB, pframe, module);
			} while (pframe);
		} else {
			path = &sw_ctx->path[module->channel[CAM_CH_CAP].dcam_path_id];
			do {
				pframe = cam_queue_dequeue(&module->channel[CAM_CH_CAP].share_buf_queue,
					struct camera_frame, list);
				if (pframe == NULL)
					break;
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_OUTPUT_BUF, path->path_id, pframe);
			} while (pframe);
		}
	}
	sw_ctx->dec_all_done = 0;
	sw_ctx->dec_layer0_done = 0;

	return ret;
}

static int camcore_csi_switch_connect(struct camera_module *module, uint32_t mode)
{
	int ret = 0;
	struct cam_hw_info *hw = module->grp->hw_info;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	struct dcam_switch_param csi_switch = {0};
	struct channel_context *ch_prev = NULL, *ch_cap = NULL;
	uint32_t loop = 0;

	/* bind */
	do {
		ret = dcam_core_context_bind(sw_ctx, hw->csi_connect_type, module->dcam_idx);
		if (!ret) {
			if (sw_ctx->hw_ctx_id >= DCAM_HW_CONTEXT_MAX)
				pr_err("fail to get hw_ctx_id\n");
			break;
		}
		pr_info_ratelimited("hw_ctx_id %d wait for hw. loop %d\n", sw_ctx->hw_ctx_id, loop);
		usleep_range(600, 800);
	} while (loop++ < 5000);

	if (sw_ctx->hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
		pr_err("fail to connect. csi %d dcam %d sw_ctx_id %d\n", module->dcam_idx, sw_ctx->hw_ctx_id, sw_ctx->sw_ctx_id);
		return -1;
	}
	if (module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR && !module->cam_uinfo.param_frame_sync) {
		uint32_t shutoff = 0;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx, DCAM_PATH_CFG_SHUTOFF,
			DCAM_PATH_RAW, &shutoff);
	}
	/* reconfig*/
	module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_RECFG_PARAM, NULL);

	/* start */
	ret = module->dcam_dev_handle->dcam_pipe_ops->start(sw_ctx, 1);
	if (ret < 0) {
		pr_err("fail to start dcam dev, ret %d\n", ret);
		return ret;
	}
	/* switch connect */
	csi_switch.csi_id = module->dcam_idx;
	csi_switch.dcam_id= sw_ctx->hw_ctx_id;
	hw->dcam_ioctl(hw, DCAM_HW_CONECT_CSI, &csi_switch);
	pr_info("Connect csi_id = %d, dcam_id = %d module idx %d\n", csi_switch.csi_id, csi_switch.dcam_id, module->idx);
	ch_prev = &module->channel[CAM_CH_PRE];
	if (ch_prev->enable)
		camcore_channel_size_config(module, ch_prev);

	ch_cap = &module->channel[CAM_CH_CAP];
	if (ch_cap->enable)
		camcore_channel_size_config(module, ch_cap);
	return ret;
}

static int camcore_zoom_proc(void *param)
{
	int update_pv = 0, update_c = 0;
	int update_always = 0;
	struct camera_module *module;
	struct channel_context *ch_prev, *ch_vid, *ch_cap;
	struct camera_frame *pre_zoom_coeff = NULL;
	struct camera_frame *vid_zoom_coeff = NULL;
	struct camera_frame *cap_zoom_coeff = NULL;

	module = (struct camera_module *)param;
	ch_prev = &module->channel[CAM_CH_PRE];
	ch_cap = &module->channel[CAM_CH_CAP];
	ch_vid = &module->channel[CAM_CH_VID];
next:
	pre_zoom_coeff = vid_zoom_coeff = cap_zoom_coeff = NULL;
	update_pv = update_c = update_always = 0;
	/* Get node from the preview/video/cap coef queue if exist */
	if (ch_prev->enable)
		pre_zoom_coeff = cam_queue_dequeue(&ch_prev->zoom_coeff_queue,
			struct camera_frame, list);
	if (pre_zoom_coeff) {
		ch_prev->ch_uinfo.src_crop = pre_zoom_coeff->zoom_crop;
		ch_prev->ch_uinfo.total_src_crop = pre_zoom_coeff->total_zoom_crop;
		cam_queue_empty_frame_put(pre_zoom_coeff);
		update_pv |= 1;
	}

	if (ch_vid->enable)
		vid_zoom_coeff = cam_queue_dequeue(&ch_vid->zoom_coeff_queue,
			struct camera_frame, list);
	if (vid_zoom_coeff) {
		ch_vid->ch_uinfo.src_crop = vid_zoom_coeff->zoom_crop;
		cam_queue_empty_frame_put(vid_zoom_coeff);
		update_pv |= 1;
	}

	if (ch_cap->enable)
		cap_zoom_coeff = cam_queue_dequeue(&ch_cap->zoom_coeff_queue,
			struct camera_frame, list);
	if (cap_zoom_coeff) {
		ch_cap->ch_uinfo.src_crop = cap_zoom_coeff->zoom_crop;
		cam_queue_empty_frame_put(cap_zoom_coeff);
		update_c |= 1;
	}

	if (update_pv || update_c) {
		if (ch_cap->enable && (ch_cap->mode_ltm == MODE_LTM_CAP) && (!module->cam_uinfo.is_dual))
			update_always = 1;

		if (module->zoom_solution == ZOOM_DEFAULT)
			camcore_channel_size_binning_cal(module, 1);
		else if (module->zoom_solution == ZOOM_BINNING2 ||
			module->zoom_solution == ZOOM_BINNING4)
			camcore_channel_size_binning_cal(module, 0);
		else if (module->zoom_solution == ZOOM_SCALER)
			camcore_channel_size_binning_cal(module, ZOOM_SCALER);
		else
			camcore_channel_size_rds_cal(module);

		if (ch_cap->enable && (update_c || update_always)) {
			mutex_lock(&module->zoom_lock);
			camcore_channel_size_config(module, ch_cap);
			mutex_unlock(&module->zoom_lock);
		}
		if (ch_prev->enable && (update_pv || update_always)) {
			mutex_lock(&module->zoom_lock);
			camcore_channel_size_config(module, ch_prev);
			mutex_unlock(&module->zoom_lock);
		}
		goto next;
	}
	return 0;
}

static int camcore_capture_proc(void *param)
{
	int ret = 0;
	struct camera_module *module;
	struct camera_frame *pframe;
	struct channel_context *channel;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;

	module = (struct camera_module *)param;

	mutex_lock(&module->fdr_lock);
	if ((module->fdr_done & (1 << CAMERA_IRQ_FDRL)) &&
		(module->fdr_done & (1 << CAMERA_IRQ_FDRH))) {
		pr_info("cam%d fdr done\n", module->idx);
		if (!module->cam_uinfo.raw_alg_type) {
			if (module->fdr_init)
				camcore_fdr_context_deinit(module, &module->channel[CAM_CH_CAP]);
			module->fdr_done = 0;
		}
		mutex_unlock(&module->fdr_lock);
		return 0;
	}
	mutex_unlock(&module->fdr_lock);

	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	channel = &module->channel[CAM_CH_CAP];
	do {
		pframe = cam_queue_dequeue(&channel->share_buf_queue,
				struct camera_frame, list);
		if (!pframe)
			return 0;
		if (module->dcam_cap_status != DCAM_CAPTURE_START_WITH_TIMESTAMP &&
			pframe->boot_sensor_time < module->capture_times) {
			pr_debug("cam%d cap skip frame type[%d] cap_time[%lld] sof_time[%lld]\n",
				module->idx, module->dcam_cap_status,
				module->capture_times, pframe->boot_sensor_time);
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(
				dcam_sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe);
		} else {
			break;
		}
	} while (pframe);

	ret = -1;
	if (module->cap_status != CAM_CAPTURE_STOP) {
		pr_debug("capture frame cam id %d, fid[%d], frame w %d, h %d c_time %lld, s_time %lld\n",
			module->idx, pframe->fid, pframe->width, pframe->height,
			module->capture_times, pframe->boot_sensor_time);
		ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe,
				channel->isp_ctx_id);
	}

	if (ret) {
		pr_warn("warning: capture stop or isp queue overflow\n");
		if (ret == -ESPIPE && module->dcam_cap_status == DCAM_CAPTURE_START_FROM_NEXT_SOF) {
			atomic_inc(&module->capture_frames_dcam);
			return 0;
		}
		if (module->cam_uinfo.dcam_slice_mode && pframe->dcam_idx == DCAM_ID_1)
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->aux_dcam_path_id, pframe);
		else
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, pframe);

		/* Bug 1103913. In Race condition when We have already received stop capture &
		 * stream_off request & then capture thread gets chance to execute. In that case
		 * module->dcam_dev_handle->dcam_pipe_ops->cfg_path will fail & return non-Zero value. This will  cause memory leak.
		 * So we need to free pframe buffer explicitely.
		 */
		if (ret)
			camcore_k_frame_put((void *)pframe);
	}
	return 0;
}

struct dcam_dev_param *camcore_aux_dcam_param_prepare(struct camera_frame *pframe, struct dcam_sw_context *pctx)
{
	uint32_t loop = 0;
	struct camera_frame *param_frm = NULL;

	do {
		param_frm = cam_queue_dequeue(&pctx->blk_param_queue, struct camera_frame, list);
		if (param_frm) {
			pr_debug("src fid:%d, dst fid:%d.\n", param_frm->fid, pframe->fid);
			if (param_frm->fid == pframe->fid) {
				param_frm->fid = 0xffff;
				cam_queue_enqueue(&pctx->blk_param_queue, &param_frm->list);
				return param_frm->pm;
			} else
				cam_queue_enqueue(&pctx->blk_param_queue, &param_frm->list);
		} else
			pr_warn("warning:no frame in param queue:%d.\n", pctx->blk_param_queue.cnt);
	} while (loop++ < pctx->blk_param_queue.cnt);

	pr_warn("Warning: not get param fm.\n");
	return NULL;
}

static int camcore_offline_proc(void *param)
{
	int ret = 0, loop = 0;
	struct camera_module *module = NULL;
	struct dcam_sw_context *pctx = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct camera_frame *pframe = NULL;
	struct cam_hw_info *hw = NULL;
	struct dcam_fmcu_ctx_desc *fmcu = NULL;
	struct dcam_dev_param *pm = NULL;
	struct dcam_pipe_context *pm_pctx = NULL;
	uint32_t need_slice = 0, lbuf_width = 0;
	uint32_t use_fmcu = 0;
	uint32_t dev_fid = 0;

	module = (struct camera_module *)param;
	dev = module->dcam_dev_handle;

	if (module->cam_uinfo.virtualsensor)
		pctx = &dev->sw_ctx[module->cur_sw_ctx_id];
	else
		pctx = &dev->sw_ctx[module->offline_cxt_id];

	hw = module->grp->hw_info;

	pr_debug("enter is raw alg:%d\n", pctx->is_raw_alg);
	ret = wait_for_completion_interruptible_timeout(&pctx->frm_done, DCAM_OFFLINE_TIMEOUT);
	if (ret <= 0) {
		pr_err("fail to wait dcam context %d.\n", pctx->sw_ctx_id);
		pframe = cam_queue_dequeue(&pctx->in_queue, struct camera_frame, list);
		if (!pframe) {
			pr_warn("warning: no frame from in_q. dcam%d\n", pctx->hw_ctx_id);
			return 0;
		}
		ret = -EFAULT;
		pctx->dcam_cb_func(DCAM_CB_RET_SRC_BUF, pframe, pctx->cb_priv_data);
		return ret;
	}

	if (!module->cam_uinfo.virtualsensor) {
		loop = 0;
		do {
			ret = dcam_core_context_bind(pctx, hw->csi_connect_type, module->aux_dcam_id);
			if (!ret)
				break;
			pr_info_ratelimited("ctx %d wait for hw. loop %d\n", pctx->hw_ctx_id, loop);
			usleep_range(600, 800);
		} while (loop++ < 5000);

		if ((loop == 5000) || (pctx->hw_ctx_id >= DCAM_HW_CONTEXT_MAX)) {
			pr_err("fail to get hw_ctx_id\n");
			return -1;
		}
	}
	pr_debug("bind hw context %d\n", pctx->hw_ctx_id);

	if (pctx->dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
		ret = dcam_core_offline_slices_sw_start(pctx);
		return ret;
	}

	/*in icap multi frame remove pctx->frame_index = DCAM_FRAME_INDEX_MAX for gtm map*/
	ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_RESET, &pctx->hw_ctx_id);
	pctx->frame_index = DCAM_FRAME_INDEX_MAX;

	if (ret)
		pr_err("fail to reset dcam%d\n", pctx->hw_ctx_id);

	pframe = dcam_offline_cycle_frame(pctx);
	if (!pframe)
		goto input_err;

	/* FDR solution: select context for frame type */
	pctx->cur_ctx_id = DCAM_CXT_0;
	if (pframe->irq_property != CAM_FRAME_COMMON)
		pctx->cur_ctx_id = DCAM_CXT_1;
	if (pframe->irq_property == CAM_FRAME_FDRH)
		pctx->cur_ctx_id = DCAM_CXT_2;
	if (pframe->irq_property == CAM_FRAME_PRE_FDR)
		pframe->irq_property = CAM_FRAME_COMMON;
	pm_pctx = &pctx->ctx[pctx->cur_ctx_id];
	pm = &pm_pctx->blk_pm;
	pm->non_zsl_cap = 1;
	pm->dev = pctx;
	if (pframe->irq_property == CAM_FRAME_FDRH && module->cam_uinfo.param_frame_sync) {
		pm = camcore_aux_dcam_param_prepare(pframe, pctx);
		if (pm) {
			pm->dev = pctx;
			pm->idx = pctx->hw_ctx_id;
		} else {
			pm = &pm_pctx->blk_pm;
			pm->dev = pctx;
		}
	}

	if ((pm->lsc.buf.mapping_state & CAM_BUF_MAPPING_DEV) == 0) {
		ret = cam_buf_iommu_map(&pm->lsc.buf, CAM_IOMMUDEV_DCAM);
		if (ret)
			pm->lsc.buf.iova[0] = 0L;
	}

	pctx->index_to_set = pframe->fid - pctx->base_fid;
	dev_fid = pctx->index_to_set;
	if (pframe->sensor_time.tv_sec || pframe->sensor_time.tv_usec) {
		pctx->frame_ts[tsid(dev_fid)].tv_sec = pframe->sensor_time.tv_sec;
		pctx->frame_ts[tsid(dev_fid)].tv_nsec = pframe->sensor_time.tv_usec * NSEC_PER_USEC;
		pctx->frame_ts_boot[tsid(dev_fid)] = pframe->boot_sensor_time;
	}

	ret = dcam_offline_param_get(hw, pctx, pframe);
	if (ret) {
		pr_err("fail to get dcam offline param\n");
		goto return_buf;
	}

	need_slice = dcam_offline_slice_needed(hw, pctx, &lbuf_width, pframe->width);
	if (need_slice) {
		use_fmcu = hw->ip_dcam[pctx->hw_ctx_id]->fmcu_support;
		if (hw->ip_dcam[pctx->hw_ctx_id]->offline_slice_support == 0) {
			pr_err("dcam%d failed, width %d > lbuf %d\n", pctx->hw_ctx_id, pframe->width, lbuf_width);
			goto return_buf;
		}
	}

	ret = dcam_offline_param_set(hw, pctx, pm);
	if (ret) {
		pr_err("fail to set dcam offline param\n");
		goto return_buf;
	}
	pctx->frame_index = pframe->fid;

	use_fmcu = 0;
	if (use_fmcu) {
		fmcu = dcam_fmcu_ctx_desc_get(hw, pctx->hw_ctx_id);
		if (fmcu && fmcu->ops) {
			fmcu->hw = hw;
			ret = fmcu->ops->ctx_init(fmcu);
			if (ret) {
				pr_err("fail to init fmcu ctx\n");
				dcam_fmcu_ctx_desc_put(fmcu);
				use_fmcu = 0;
			} else
				pctx->hw_ctx->fmcu = fmcu;
		} else {
			pr_debug("no more fmcu\n");
			use_fmcu = 0;
		}
	}

	if (need_slice) {
		dcam_offline_slice_info_cal(pctx, pframe, lbuf_width);/*prepare param for every slice*/
		if (!use_fmcu) {
			pr_info("use ap support slices for ctx %d hw %d\n", pm_pctx->ctx_id, pctx->hw_ctx_id);
		} else {
			pr_info("use fmcu support slices for ctx %d hw %d\n", pctx->sw_ctx_id, pctx->hw_ctx_id);
			ret = dcam_offline_slice_fmcu_cmds_set((void *)fmcu, pctx);
			if (ret) {
				pr_warn("warning: set fmcu cmd, slice by ap\n");
				use_fmcu = 0;
				dcam_fmcu_ctx_desc_put(fmcu);
			}
		}
	} else {
		pctx->slice_count = 1;
		pctx->slice_num = 1;
		pctx->dcam_slice_mode = CAM_SLICE_NONE;
	}

	if (use_fmcu) {
		pr_info("fmcu start.");
		ret = fmcu->ops->hw_start(fmcu);
	} else
		ret = dcam_offline_slices_proc(hw, pctx, pframe, pm);

	pr_debug("done\n");
	return ret;

return_buf:
	pframe = cam_queue_dequeue(&pctx->proc_queue, struct camera_frame, list);
	cam_buf_iommu_unmap(&pframe->buf);
	pctx->dcam_cb_func(DCAM_CB_RET_SRC_BUF, pframe, pctx->cb_priv_data);
input_err:
	complete(&pctx->slice_done);
	complete(&pctx->frm_done);
	dcam_core_context_unbind(pctx);
	return ret;
}

static int camcore_module_init(struct camera_module *module)
{
	int ch;
	struct channel_context *channel;

	pr_info("sprd_img: camera dev %d init start!\n", module->idx);

	atomic_set(&module->state, CAM_INIT);
	mutex_init(&module->lock);
	mutex_init(&module->zoom_lock);
	mutex_init(&module->fdr_lock);
	init_completion(&module->frm_com);
	init_completion(&module->streamoff_com);
	module->exit_flag = 0;

	module->cap_status = CAM_CAPTURE_STOP;
	module->dcam_cap_status = DCAM_CAPTURE_STOP;
	module->raw_cap_fetch_fmt = DCAM_RAW_MAX;

	for (ch = 0; ch < CAM_CH_MAX; ch++) {
		channel = &module->channel[ch];
		channel->ch_id = ch;
		channel->dcam_path_id = -1;
		channel->isp_ctx_id = -1;
		channel->isp_path_id = -1;
		mutex_init(&module->buf_lock[channel->ch_id]);
		init_completion(&channel->alloc_com);
	}

	module->flash_core_handle = get_cam_flash_handle(module->idx);

	camcore_timer_init(&module->cam_timer, (unsigned long)module);
	module->attach_sensor_id = SPRD_SENSOR_ID_MAX + 1;
	module->is_smooth_zoom = 1;
	cam_queue_init(&module->frm_queue,
		CAM_FRAME_Q_LEN, camcore_empty_frame_put);
	cam_queue_init(&module->irq_queue,
		CAM_IRQ_Q_LEN, camcore_empty_frame_put);
	cam_queue_init(&module->statis_queue,
		CAM_STATIS_Q_LEN, camcore_empty_frame_put);
	cam_queue_init(&module->alloc_queue,
		CAM_ALLOC_Q_LEN, camcore_empty_frame_put);
	pr_info("module[%d] init OK %px!\n", module->idx, module);
	return 0;
}

static int camcore_module_deinit(struct camera_module *module)
{
	int ch = 0;
	struct channel_context *channel = NULL;

	put_cam_flash_handle(module->flash_core_handle);
	cam_queue_clear(&module->frm_queue, struct camera_frame, list);
	cam_queue_clear(&module->irq_queue, struct camera_frame, list);
	cam_queue_clear(&module->statis_queue, struct camera_frame, list);
	cam_queue_clear(&module->alloc_queue, struct camera_frame, list);
	for (ch = 0; ch < CAM_CH_MAX; ch++) {
		channel = &module->channel[ch];
		mutex_destroy(&module->buf_lock[channel->ch_id]);
	}
	mutex_destroy(&module->fdr_lock);
	mutex_destroy(&module->zoom_lock);
	mutex_destroy(&module->lock);
	return 0;
}

static int camcore_faceid_secbuf(uint32_t sec, struct camera_buf *buf)
{
	int ret = 0;
	bool vaor_bp_en = 0;
	if (sec) {
		buf->buf_sec = 1;
		vaor_bp_en = true;
		ret = sprd_iommu_set_cam_bypass(vaor_bp_en);
		if (unlikely(ret)) {
			pr_err("fail to enable vaor bypass mode, ret %d\n", ret);
			ret = -EFAULT;
		}
	}

	return ret;
}

#define CAM_IOCTL_LAYER
#include "cam_ioctl.c"
#undef CAM_IOCTL_LAYER

static struct cam_ioctl_cmd ioctl_cmds_table[] = {
	[_IOC_NR(SPRD_IMG_IO_SET_MODE)]             = {SPRD_IMG_IO_SET_MODE,             camioctl_mode_set},
	[_IOC_NR(SPRD_IMG_IO_SET_CAP_SKIP_NUM)]     = {SPRD_IMG_IO_SET_CAP_SKIP_NUM,     camioctl_cap_skip_num_set},
	[_IOC_NR(SPRD_IMG_IO_SET_SENSOR_SIZE)]      = {SPRD_IMG_IO_SET_SENSOR_SIZE,      camioctl_sensor_size_set},
	[_IOC_NR(SPRD_IMG_IO_SET_SENSOR_TRIM)]      = {SPRD_IMG_IO_SET_SENSOR_TRIM,      camioctl_sensor_trim_set},
	[_IOC_NR(SPRD_IMG_IO_SET_FRM_ID_BASE)]      = {SPRD_IMG_IO_SET_FRM_ID_BASE,      camioctl_frame_id_base_set},
	[_IOC_NR(SPRD_IMG_IO_SET_CROP)]             = {SPRD_IMG_IO_SET_CROP,             camioctl_crop_set},
	[_IOC_NR(SPRD_IMG_IO_SET_FLASH)]            = {SPRD_IMG_IO_SET_FLASH,            camioctl_flash_set},
	[_IOC_NR(SPRD_IMG_IO_SET_OUTPUT_SIZE)]      = {SPRD_IMG_IO_SET_OUTPUT_SIZE,      camioctl_output_size_set},
	[_IOC_NR(SPRD_IMG_IO_SET_ZOOM_MODE)]        = {SPRD_IMG_IO_SET_ZOOM_MODE,        camioctl_zoom_mode_set},
	[_IOC_NR(SPRD_IMG_IO_SET_SENSOR_IF)]        = {SPRD_IMG_IO_SET_SENSOR_IF,        camioctl_sensor_if_set},
	[_IOC_NR(SPRD_IMG_IO_SET_FRAME_ADDR)]       = {SPRD_IMG_IO_SET_FRAME_ADDR,       camioctl_frame_addr_set},
	[_IOC_NR(SPRD_IMG_IO_PATH_FRM_DECI)]        = {SPRD_IMG_IO_PATH_FRM_DECI,        camioctl_frm_deci_set},
	[_IOC_NR(SPRD_IMG_IO_PATH_PAUSE)]           = {SPRD_IMG_IO_PATH_PAUSE,           camioctl_path_pause},
	[_IOC_NR(SPRD_IMG_IO_PATH_RESUME)]          = {SPRD_IMG_IO_PATH_RESUME,          camioctl_path_resume},
	[_IOC_NR(SPRD_IMG_IO_STREAM_ON)]            = {SPRD_IMG_IO_STREAM_ON,            camioctl_stream_on},
	[_IOC_NR(SPRD_IMG_IO_STREAM_OFF)]           = {SPRD_IMG_IO_STREAM_OFF,           camioctl_stream_off},
	[_IOC_NR(SPRD_IMG_IO_STREAM_PAUSE)]         = {SPRD_IMG_IO_STREAM_PAUSE,         camioctl_stream_pause},
	[_IOC_NR(SPRD_IMG_IO_STREAM_RESUME)]        = {SPRD_IMG_IO_STREAM_RESUME,        camioctl_stream_resume},
	[_IOC_NR(SPRD_IMG_IO_GET_FMT)]              = {SPRD_IMG_IO_GET_FMT,              camioctl_fmt_get},
	[_IOC_NR(SPRD_IMG_IO_GET_CH_ID)]            = {SPRD_IMG_IO_GET_CH_ID,            camioctl_ch_id_get},
	[_IOC_NR(SPRD_IMG_IO_GET_TIME)]             = {SPRD_IMG_IO_GET_TIME,             camioctl_time_get},
	[_IOC_NR(SPRD_IMG_IO_CHECK_FMT)]            = {SPRD_IMG_IO_CHECK_FMT,            camioctl_fmt_check},
	[_IOC_NR(SPRD_IMG_IO_SET_SHRINK)]           = {SPRD_IMG_IO_SET_SHRINK,           camioctl_shrink_set},
	[_IOC_NR(SPRD_IMG_IO_CFG_FLASH)]            = {SPRD_IMG_IO_CFG_FLASH,            camioctl_flash_cfg},
	[_IOC_NR(SPRD_IMG_IO_GET_IOMMU_STATUS)]     = {SPRD_IMG_IO_GET_IOMMU_STATUS,     camioctl_iommu_status_get},
	[_IOC_NR(SPRD_IMG_IO_START_CAPTURE)]        = {SPRD_IMG_IO_START_CAPTURE,        camioctl_capture_start},
	[_IOC_NR(SPRD_IMG_IO_STOP_CAPTURE)]         = {SPRD_IMG_IO_STOP_CAPTURE,         camioctl_capture_stop},
	[_IOC_NR(SPRD_IMG_IO_DCAM_PATH_SIZE)]       = {SPRD_IMG_IO_DCAM_PATH_SIZE,       camioctl_dcam_path_size},
	[_IOC_NR(SPRD_IMG_IO_SET_SENSOR_MAX_SIZE)]  = {SPRD_IMG_IO_SET_SENSOR_MAX_SIZE,  camioctl_sensor_max_size_set},
	[_IOC_NR(SPRD_ISP_IO_SET_STATIS_BUF)]       = {SPRD_ISP_IO_SET_STATIS_BUF,       camioctl_statis_buf_set},
	[_IOC_NR(SPRD_ISP_IO_CFG_PARAM)]            = {SPRD_ISP_IO_CFG_PARAM,            camioctl_param_cfg},
	[_IOC_NR(SPRD_ISP_IO_RAW_CAP)]              = {SPRD_ISP_IO_RAW_CAP,              camioctl_raw_proc},
	[_IOC_NR(SPRD_IMG_IO_GET_DCAM_RES)]         = {SPRD_IMG_IO_GET_DCAM_RES,         camioctl_cam_res_get},
	[_IOC_NR(SPRD_IMG_IO_PUT_DCAM_RES)]         = {SPRD_IMG_IO_PUT_DCAM_RES,         camioctl_cam_res_put},
	[_IOC_NR(SPRD_IMG_IO_SET_FUNCTION_MODE)]    = {SPRD_IMG_IO_SET_FUNCTION_MODE,    camioctl_function_mode_set},
	[_IOC_NR(SPRD_IMG_IO_GET_FLASH_INFO)]       = {SPRD_IMG_IO_GET_FLASH_INFO,       camioctl_flash_get},
	[_IOC_NR(SPRD_IMG_IO_EBD_CONTROL)]          = {SPRD_IMG_IO_EBD_CONTROL,          camioctl_ebd_control},
	[_IOC_NR(SPRD_IMG_IO_SET_4IN1_ADDR)]        = {SPRD_IMG_IO_SET_4IN1_ADDR,        camioctl_4in1_raw_addr_set},
	[_IOC_NR(SPRD_IMG_IO_4IN1_POST_PROC)]       = {SPRD_IMG_IO_4IN1_POST_PROC,       camioctl_4in1_post_proc},
	[_IOC_NR(SPRD_IMG_IO_SET_CAM_SECURITY)]     = {SPRD_IMG_IO_SET_CAM_SECURITY,     camioctl_cam_security_set},
	[_IOC_NR(SPRD_IMG_IO_GET_PATH_RECT)]        = {SPRD_IMG_IO_GET_PATH_RECT,        camioctl_path_rect_get},
	[_IOC_NR(SPRD_IMG_IO_SET_3DNR_MODE)]        = {SPRD_IMG_IO_SET_3DNR_MODE,        camioctl_3dnr_mode_set},
	[_IOC_NR(SPRD_IMG_IO_SET_AUTO_3DNR_MODE)]   = {SPRD_IMG_IO_SET_AUTO_3DNR_MODE,   camioctl_auto_3dnr_mode_set},
	[_IOC_NR(SPRD_IMG_IO_CAPABILITY)]           = {SPRD_IMG_IO_CAPABILITY,           camioctl_capability_get},
	[_IOC_NR(SPRD_IMG_IO_POST_FDR)]             = {SPRD_IMG_IO_POST_FDR,             camioctl_fdr_post},
	[_IOC_NR(SPRD_IMG_IO_CAM_TEST)]             = {SPRD_IMG_IO_CAM_TEST,             camioctl_cam_test},
	[_IOC_NR(SPRD_IMG_IO_DCAM_SWITCH)]          = {SPRD_IMG_IO_DCAM_SWITCH,          camioctl_csi_switch},
	[_IOC_NR(SPRD_IMG_IO_GET_SCALER_CAP)]       = {SPRD_IMG_IO_GET_SCALER_CAP,       camioctl_scaler_capability_get},
	[_IOC_NR(SPRD_IMG_IO_GET_DWARP_HW_CAP)]     = {SPRD_IMG_IO_GET_DWARP_HW_CAP,     camioctl_dewarp_hw_capability_get},
	[_IOC_NR(SPRD_IMG_IO_SET_DWARP_OTP)]        = {SPRD_IMG_IO_SET_DWARP_OTP,        camioctl_dewarp_otp_set},
	[_IOC_NR(SPRD_IMG_IO_SET_LONGEXP_CAP)]      = {SPRD_IMG_IO_SET_LONGEXP_CAP,      camioctl_longexp_mode_set},
	[_IOC_NR(SPRD_IMG_IO_SET_MUL_MAX_SN_SIZE)]  = {SPRD_IMG_IO_SET_MUL_MAX_SN_SIZE,  camioctl_mul_max_sensor_size_set},
	[_IOC_NR(SPRD_IMG_IO_SET_CAP_ZSL_INFO)]     = {SPRD_IMG_IO_SET_CAP_ZSL_INFO,     camioctl_cap_zsl_info_set},
	[_IOC_NR(SPRD_IMG_IO_SET_DCAM_RAW_FMT)]     = {SPRD_IMG_IO_SET_DCAM_RAW_FMT,     camioctl_dcam_raw_fmt_set},
	[_IOC_NR(SPRD_IMG_IO_SET_KEY)]              = {SPRD_IMG_IO_SET_KEY,              camioctl_key_set},
	[_IOC_NR(SPRD_IMG_IO_SET_960FPS_PARAM)]     = {SPRD_IMG_IO_SET_960FPS_PARAM,     camioctl_960fps_param_set},
	[_IOC_NR(SPRD_IMG_IO_CFG_PARAM_STATUS)]     = {SPRD_IMG_IO_CFG_PARAM_STATUS,     camioctl_cfg_param_start_end},
};

static long camcore_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	int locked = 0;
	struct camera_module *module = NULL;
	struct cam_ioctl_cmd *ioctl_cmd_p = NULL;
	int nr = _IOC_NR(cmd);

	pr_debug("cam ioctl, cmd:0x%x, cmdnum %d\n", cmd, nr);

	module = (struct camera_module *)file->private_data;
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (unlikely(!(nr >= 0 && nr < ARRAY_SIZE(ioctl_cmds_table)))) {
		pr_info("invalid cmd: 0x%xn", cmd);
		return -EINVAL;
	}

	ioctl_cmd_p = &ioctl_cmds_table[nr];
	if (unlikely((ioctl_cmd_p->cmd != cmd) ||
			(ioctl_cmd_p->cmd_proc == NULL))) {
		pr_debug("unsupported cmd_k: 0x%x, cmd_u: 0x%x, nr: %d\n",
			ioctl_cmd_p->cmd, cmd, nr);
		return 0;
	}

	/* There is race condition under several cases during stream/off
	 * Take care of lock use
	 */
	if (atomic_read(&module->state) != CAM_RUNNING
		|| cmd == SPRD_IMG_IO_STREAM_OFF
		|| cmd == SPRD_ISP_IO_CFG_PARAM
		|| cmd == SPRD_IMG_IO_DCAM_SWITCH
		|| cmd == SPRD_IMG_IO_CFG_PARAM_STATUS) {
		mutex_lock(&module->lock);
		locked = 1;
	}
	down_read(&module->grp->switch_recovery_lock);
	if (cmd == SPRD_IMG_IO_SET_KEY || module->private_key == 1) {
		ret = ioctl_cmd_p->cmd_proc(module, arg);
		if (ret) {
			pr_debug("fail to ioctl cmd:%x, nr:%d, func %ps\n",
				cmd, nr, ioctl_cmd_p->cmd_proc);
			goto exit;
		}
	} else
		pr_err("cam %d fail to get ioctl permission %d\n", module->idx, module->private_key);

	pr_debug("cam id:%d, %ps, done!\n",
		module->idx, ioctl_cmd_p->cmd_proc);
exit:
	up_read(&module->grp->switch_recovery_lock);
	if (locked)
		mutex_unlock(&module->lock);

	return ret;
}

#ifdef CONFIG_COMPAT
static long camcore_ioctl_compat(struct file *file,
	unsigned int cmd, unsigned long arg)
{

	long ret = 0L;
	struct camera_module *module = NULL;
	void __user *data32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	module = (struct camera_module *)file->private_data;
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	module->compat_flag = 1;
	pr_debug("cmd [0x%x][%d]\n", cmd, _IOC_NR(cmd));

	switch (cmd) {
	case COMPAT_SPRD_ISP_IO_CFG_PARAM:
		ret = file->f_op->unlocked_ioctl(file, SPRD_ISP_IO_CFG_PARAM,
			(unsigned long)data32);
		break;
	default:
		ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)data32);
		break;
	}
	return ret;
}
#endif

static int camcore_recovery_proc(void *param)
{
	int ret = 0, i = 0, recovery_id = 0, line_w = 0;
	struct camera_group *grp = NULL;
	struct camera_module *module = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct dcam_hw_context *hw_ctx = NULL;
	struct cam_hw_info *hw = NULL;
	struct dcam_switch_param csi_switch = {0};
	uint32_t switch_mode = CAM_CSI_RECOVERY_SWITCH;
	struct cam_hw_lbuf_share camarg = {0};

	grp = (struct camera_group *)param;
	hw = grp->hw_info;

	if (atomic_read(&grp->recovery_state) != CAM_RECOVERY_NONE) {
		pr_warn("warning: in recoverying\n");
		return 0;
	}

	down_write(&grp->switch_recovery_lock);
	atomic_set(&grp->recovery_state, CAM_RECOVERY_RUNNING);
	/* all avaiable dcam csi switch disconnect & stop */
	for (i = 0; i < CAM_COUNT; i++) {
		module = grp->module[i];
		if (module && (atomic_read(&module->state) == CAM_RUNNING)){
			sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
			if (sw_ctx->hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
				pr_warn("warning: sw_ctx %d has been disconnected and unbinded already\n", sw_ctx->sw_ctx_id);
				continue;
			}
			pr_debug("sw_ctx %d dcam %d recovery disconnect\n", sw_ctx->sw_ctx_id, sw_ctx->hw_ctx_id);
			module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, DCAM_RECOVERY);
			camcore_csi_switch_disconnect(module, switch_mode);
			hw_ctx = sw_ctx->hw_ctx;
			if (sw_ctx->slw_type == DCAM_SLW_FMCU)
				hw_ctx->fmcu->ops->buf_unmap(hw_ctx->fmcu);
		}
	}

	recovery_id = 0;
	hw->dcam_ioctl(hw, DCAM_HW_CFG_ALL_RESET, &recovery_id);
	cam_buf_iommu_restore(CAM_IOMMUDEV_DCAM);

	/* all avaiable dcam reconfig & start */
	for (i = 0; i < CAM_COUNT; i++) {
		module = grp->module[i];
		if (module && (atomic_read(&module->state) == CAM_RUNNING)){
			sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
			if (sw_ctx->hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
				pr_warn("warning: sw_ctx %d has been disconnected and unbinded already\n", sw_ctx->sw_ctx_id);
				continue;
			}

			line_w = module->cam_uinfo.sn_rect.w;
			if (module->cam_uinfo.is_4in1)
				line_w /= 2;
			camarg.idx = sw_ctx->hw_ctx_id;
			camarg.width = line_w;
			camarg.offline_flag = 0;
			if (hw->ip_dcam[sw_ctx->hw_ctx_id]->lbuf_share_support)
				ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_LBUF_SHARE_SET, &camarg);

			pr_debug("sw_ctx %d dcam %d recovery connect\n", sw_ctx->sw_ctx_id, sw_ctx->hw_ctx_id);
			csi_switch.csi_id = module->dcam_idx;
			csi_switch.dcam_id= sw_ctx->hw_ctx_id;
			hw->dcam_ioctl(hw, DCAM_HW_FORCE_EN_CSI, &csi_switch);

			pr_debug("sw_ctx %d dcam %d recovery start\n", sw_ctx->sw_ctx_id, sw_ctx->hw_ctx_id);
			module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_RECFG_PARAM, NULL);
			if (sw_ctx->slw_type == DCAM_SLW_FMCU)
				hw_ctx->fmcu->ops->buf_map(hw_ctx->fmcu);
			ret = module->dcam_dev_handle->dcam_pipe_ops->start(sw_ctx, 1);
		}
	}
	atomic_set(&grp->recovery_state, CAM_RECOVERY_DONE);
	up_write(&grp->switch_recovery_lock);
	pr_info("cam recovery is finish\n");

	return ret;
}

static ssize_t camcore_read(struct file *file, char __user *u_data,
		size_t cnt, loff_t *cnt_ret)
{
	int ret = 0;
	int i = 0;
	int superzoom_val = 0;
	struct sprd_img_read_op read_op;
	struct camera_module *module = NULL;
	struct camera_frame *pframe;
	struct channel_context *pchannel;
	struct sprd_img_path_capability *cap;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_reg_trace trace;

	module = (struct camera_module *)file->private_data;
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (cnt != sizeof(struct sprd_img_read_op)) {
		pr_err("fail to img read, cnt %zd read_op %d\n", cnt,
			(int32_t)sizeof(struct sprd_img_read_op));
		return -EIO;
	}

	if (copy_from_user(&read_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	pr_debug("cam %d read cmd %d\n", module->idx, read_op.cmd);

	switch (read_op.cmd) {
	case SPRD_IMG_GET_SCALE_CAP:
		hw = module->grp->hw_info;
		for (i = 0; i < DCAM_ID_MAX; i++) {
			if (hw->ip_dcam[i]->superzoom_support) {
				superzoom_val = 1;
				break;
			}
		}

		if (superzoom_val)
			read_op.parm.reserved[1] = 10;
		else
			read_op.parm.reserved[1] = 4;

		read_op.parm.reserved[0] = 4672;
		read_op.parm.reserved[2] = 4672;
		pr_debug("line threshold %d, sc factor %d, scaling %d.\n",
			read_op.parm.reserved[0],
			read_op.parm.reserved[1],
			read_op.parm.reserved[2]);
		break;
	case SPRD_IMG_GET_FRM_BUFFER:
rewait:
		memset(&read_op, 0, sizeof(struct sprd_img_read_op));
		while (1) {
			ret = wait_for_completion_interruptible(
				&module->frm_com);
			if (ret == 0) {
				break;
			} else if (ret == -ERESTARTSYS) {
				read_op.evt = IMG_SYS_BUSY;
				ret = 0;
				goto read_end;
			} else {
				pr_err("read frame buf, fail to down, %d\n",
					ret);
				return -EPERM;
			}
		}

		pchannel = NULL;
		pframe = cam_queue_dequeue(&module->frm_queue,
			struct camera_frame, list);
		if (!pframe) {
			/* any exception happens or user trigger exit. */
			pr_info("No valid frame buffer. tx stop.\n");
			read_op.evt = IMG_TX_STOP;
		} else if (pframe->evt == IMG_TX_DONE) {
			atomic_set(&module->timeout_flag, 0);
			if ((pframe->irq_type == CAMERA_IRQ_4IN1_DONE)
				|| (pframe->irq_type == CAMERA_IRQ_FDRL)
				|| (pframe->irq_type == CAMERA_IRQ_FDRH)
				|| (pframe->irq_type == CAMERA_IRQ_FDR_DRC)
				|| (pframe->irq_type == CAMERA_IRQ_IMG)
				|| (pframe->irq_type == CAMERA_IRQ_PRE_FDR)
				|| (pframe->irq_type == CAMERA_IRQ_RAW_IMG)
				|| (pframe->irq_type == CAMERA_IRQ_RAW_BPC_IMG)) {
				cam_buf_ionbuf_put(&pframe->buf);
				pchannel = &module->channel[pframe->channel_id];
				if (pframe->buf.mfd[0] == pchannel->reserved_buf_fd) {
					pr_info("get output buffer with reserved frame fd %d, ch %d\n",
						pchannel->reserved_buf_fd, pchannel->ch_id);
					cam_queue_empty_frame_put(pframe);
					goto rewait;
				}
				read_op.parm.frame.channel_id = pframe->channel_id;
				read_op.parm.frame.index = pchannel->frm_base_id;
				read_op.parm.frame.frm_base_id = pchannel->frm_base_id;
				read_op.parm.frame.img_fmt = pframe->img_fmt;
			}
			read_op.evt = pframe->evt;
			read_op.parm.frame.irq_type = pframe->irq_type;
			read_op.parm.frame.irq_property = pframe->irq_property;
			read_op.parm.frame.length = pframe->width;
			read_op.parm.frame.height = pframe->height;
			read_op.parm.frame.real_index = pframe->fid;
			read_op.parm.frame.frame_id = pframe->fid;
			read_op.parm.frame.is_flash_status = pframe->is_flash_status;
			/*
			 * read_op.parm.frame.sec = pframe->time.tv_sec;
			 * read_op.parm.frame.usec = pframe->time.tv_usec;
			 * read_op.parm.frame.monoboottime = pframe->boot_time;
			 */
			/* use SOF time instead of ISP time for better accuracy */
			read_op.parm.frame.sec = pframe->sensor_time.tv_sec;
			read_op.parm.frame.usec = pframe->sensor_time.tv_usec;
			read_op.parm.frame.monoboottime = pframe->boot_sensor_time;
			read_op.parm.frame.yaddr_vir = (uint32_t)pframe->buf.addr_vir[0];
			read_op.parm.frame.uaddr_vir = (uint32_t)pframe->buf.addr_vir[1];
			read_op.parm.frame.vaddr_vir = (uint32_t)pframe->buf.addr_vir[2];
			read_op.parm.frame.mfd = pframe->buf.mfd[0];
			read_op.parm.frame.yaddr = pframe->buf.offset[0];
			read_op.parm.frame.uaddr = pframe->buf.offset[1];
			read_op.parm.frame.vaddr = pframe->buf.offset[2];

			if ((pframe->irq_type == CAMERA_IRQ_FDRL) ||
				(pframe->irq_type == CAMERA_IRQ_FDRH) ||
				(pframe->irq_type == CAMERA_IRQ_RAW_IMG)) {
				pr_info("FDR %d ch %d, evt %d, fid %d, buf_fd %d,  time  %06d.%06d\n",
					pframe->irq_type,  read_op.parm.frame.channel_id, read_op.evt,
					read_op.parm.frame.real_index, read_op.parm.frame.mfd,
					read_op.parm.frame.sec, read_op.parm.frame.usec);
			}
			/* for statis buffer address below. */
			read_op.parm.frame.addr_offset = pframe->buf.offset[0];

			read_op.parm.frame.zoom_ratio = pframe->zoom_ratio;
			read_op.parm.frame.total_zoom = pframe->total_zoom;
		} else {
			struct cam_hw_info *hw = module->grp->hw_info;

			pr_err("fail to get correct event %d\n", pframe->evt);
			if (hw == NULL) {
				pr_err("fail to get hw ops.\n");
				return -EFAULT;
			}

			csi_api_reg_trace();

			trace.type = ABNORMAL_REG_TRACE;
			trace.idx = module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id;
			hw->isp_ioctl(hw, ISP_HW_CFG_REG_TRACE, &trace);
			read_op.evt = pframe->evt;
			read_op.parm.frame.irq_type = pframe->irq_type;
			read_op.parm.frame.irq_property = pframe->irq_property;
		}

		pr_debug("cam%d read frame, evt 0x%x irq %d, irq_property %d, ch 0x%x index 0x%x mfd 0x%x\n",
			module->idx, read_op.evt, read_op.parm.frame.irq_type, read_op.parm.frame.irq_property, read_op.parm.frame.channel_id,
			read_op.parm.frame.real_index, read_op.parm.frame.mfd);

		if (pframe) {
			if (pframe->irq_type != CAMERA_IRQ_4IN1_DONE) {
				cam_queue_empty_frame_put(pframe);
				break;
			}
			/* 4in1 report frame for remosaic
			 * save frame for 4in1_post IOCTL
			 */
			ret = cam_queue_enqueue(&module->remosaic_queue,
				&pframe->list);
			if (!ret)
				break;
			/* fail, give back */
			cam_queue_empty_frame_put(pframe);
			ret = 0;
		}

		break;

	case SPRD_IMG_GET_PATH_CAP:
		pr_debug("get path capbility\n");
		cap = &read_op.parm.capability;
		memset(cap, 0, sizeof(struct sprd_img_path_capability));
		cap->support_3dnr_mode = 1;
		cap->support_4in1 = 1;
		cap->count = 6;
		cap->path_info[CAM_CH_RAW].support_yuv = 0;
		cap->path_info[CAM_CH_RAW].support_raw = 1;
		cap->path_info[CAM_CH_RAW].support_jpeg = 0;
		cap->path_info[CAM_CH_RAW].support_scaling = 0;
		cap->path_info[CAM_CH_RAW].support_trim = 1;
		cap->path_info[CAM_CH_RAW].is_scaleing_path = 0;
		cap->path_info[CAM_CH_PRE].line_buf = ISP_WIDTH_MAX;
		cap->path_info[CAM_CH_PRE].support_yuv = 1;
		cap->path_info[CAM_CH_PRE].support_raw = 0;
		cap->path_info[CAM_CH_PRE].support_jpeg = 0;
		cap->path_info[CAM_CH_PRE].support_scaling = 1;
		cap->path_info[CAM_CH_PRE].support_trim = 1;
		cap->path_info[CAM_CH_PRE].is_scaleing_path = 0;
		cap->path_info[CAM_CH_CAP].line_buf = ISP_WIDTH_MAX;
		cap->path_info[CAM_CH_CAP].support_yuv = 1;
		cap->path_info[CAM_CH_CAP].support_raw = 0;
		cap->path_info[CAM_CH_CAP].support_jpeg = 0;
		cap->path_info[CAM_CH_CAP].support_scaling = 1;
		cap->path_info[CAM_CH_CAP].support_trim = 1;
		cap->path_info[CAM_CH_CAP].is_scaleing_path = 0;
		cap->path_info[CAM_CH_VID].line_buf = ISP_WIDTH_MAX;
		cap->path_info[CAM_CH_VID].support_yuv = 1;
		cap->path_info[CAM_CH_VID].support_raw = 0;
		cap->path_info[CAM_CH_VID].support_jpeg = 0;
		cap->path_info[CAM_CH_VID].support_scaling = 1;
		cap->path_info[CAM_CH_VID].support_trim = 1;
		cap->path_info[CAM_CH_VID].is_scaleing_path = 0;
		cap->path_info[CAM_CH_PRE_THM].line_buf = ISP_WIDTH_MAX;
		cap->path_info[CAM_CH_PRE_THM].support_yuv = 1;
		cap->path_info[CAM_CH_PRE_THM].support_raw = 0;
		cap->path_info[CAM_CH_PRE_THM].support_jpeg = 0;
		cap->path_info[CAM_CH_PRE_THM].support_scaling = 1;
		cap->path_info[CAM_CH_PRE_THM].support_trim = 1;
		cap->path_info[CAM_CH_PRE_THM].is_scaleing_path = 0;
		cap->path_info[CAM_CH_CAP_THM].line_buf = ISP_WIDTH_MAX;
		cap->path_info[CAM_CH_CAP_THM].support_yuv = 1;
		cap->path_info[CAM_CH_CAP_THM].support_raw = 0;
		cap->path_info[CAM_CH_CAP_THM].support_jpeg = 0;
		cap->path_info[CAM_CH_CAP_THM].support_scaling = 1;
		cap->path_info[CAM_CH_CAP_THM].support_trim = 1;
		cap->path_info[CAM_CH_CAP_THM].is_scaleing_path = 0;
		break;
	case SPRD_IMG_GET_DCAM_RAW_CAP:
		hw = module->grp->hw_info;
		for (i = 0; i < DCAM_RAW_MAX; i++)
			read_op.parm.reserved[i] = hw->ip_dcam[0]->raw_fmt_support[i];
		break;
	default:
		pr_err("fail to get valid cmd\n");
		return -EINVAL;
	}

read_end:
	if (copy_to_user((void __user *)u_data, &read_op, cnt))
		ret = -EFAULT;

	if (ret)
		cnt = ret;

	return cnt;
}

static ssize_t camcore_write(struct file *file, const char __user *u_data,
		size_t cnt, loff_t *cnt_ret)
{
	int ret = 0;
	struct sprd_img_write_op write_op;
	struct camera_module *module = NULL;

	module = (struct camera_module *)file->private_data;
	if (!module) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (cnt != sizeof(struct sprd_img_write_op)) {
		pr_err("fail to write, cnt %zd  write_op %d\n", cnt,
				(uint32_t)sizeof(struct sprd_img_write_op));
		return -EIO;
	}

	if (copy_from_user(&write_op, (void __user *)u_data, cnt)) {
		pr_err("fail to get user info\n");
		return -EFAULT;
	}

	switch (write_op.cmd) {
	case SPRD_IMG_STOP_DCAM:
		pr_info("user stop camera %d\n", module->idx);
		module->exit_flag = 1;
		complete(&module->frm_com);
		break;

	default:
		pr_err("fail to get write cmd %d\n", write_op.cmd);
		break;
	}

	ret =  copy_to_user((void __user *)u_data, &write_op, cnt);
	if (ret) {
		pr_err("fail to get user info\n");
		cnt = ret;
		return -EFAULT;
	}

	return cnt;
}

static int camcore_open(struct inode *node, struct file *file)
{
	int ret = 0;
	unsigned long flag;
	struct camera_module *module = NULL;
	struct camera_group *grp = NULL;
	struct miscdevice *md = file->private_data;
	uint32_t i, idx, count = 0;
	struct cam_thread_info *thrd = NULL;

	grp = md->this_device->platform_data;
	count = grp->dcam_count;

	if (count == 0 || count > CAM_COUNT) {
		pr_err("fail to get valid dts configured dcam count\n");
		return -ENODEV;
	}

	if (atomic_inc_return(&grp->camera_opened) > count) {
		pr_err("fail to open camera, all %d cameras opened already.", count);
		atomic_dec(&grp->camera_opened);
		return -EMFILE;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_get_sync(&grp->hw_info->pdev->dev);
#endif

	pr_info("sprd_img: the camera opened count %d\n",
		atomic_read(&grp->camera_opened));

	pr_info("camca: camsec_mode = %d\n", grp->camsec_cfg.camsec_mode);

	spin_lock_irqsave(&grp->module_lock, flag);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	__pm_stay_awake(grp->ws);
#endif
	for (i = 0, idx = count; i < count; i++) {
		if ((grp->module_used & (1 << i)) == 0) {
			if (grp->module[i] != NULL) {
				pr_err("fail to get null module, un-release camera module:  %p, idx %d\n",
					grp->module[i], i);
				spin_unlock_irqrestore(&grp->module_lock, flag);
				ret = -EMFILE;
				goto exit;
			}
			idx = i;
			grp->module_used |= (1 << i);
			break;
		}
	}
	spin_unlock_irqrestore(&grp->module_lock, flag);

	if (idx == count) {
		pr_err("fail to get available camera module.\n");
		ret = -EMFILE;
		goto exit;
	}

	pr_debug("kzalloc. size of module %x, group %x\n",
		(int)sizeof(struct camera_module),
		(int) sizeof(struct camera_group));

	module = vzalloc(sizeof(struct camera_module));
	if (!module) {
		pr_err("fail to alloc camera module %d\n", idx);
		ret = -ENOMEM;
		goto alloc_fail;
	}

	module->idx = idx;
	ret = camcore_module_init(module);
	if (ret) {
		pr_err("fail to init camera module %d\n", idx);
		ret = -ENOMEM;
		goto init_fail;
	}

	if (atomic_read(&grp->camera_opened) == 1) {
		/* should check all needed interface here. */
		spin_lock_irqsave(&grp->module_lock, flag);

		rwlock_init(&grp->hw_info->soc_dcam->cam_ahb_lock);
		g_empty_frm_q = &grp->empty_frm_q;
		cam_queue_init(g_empty_frm_q, CAM_EMP_Q_LEN_MAX,
			cam_queue_empty_frame_free);
		g_empty_state_q = &grp->empty_state_q;
		cam_queue_init(g_empty_state_q, CAM_EMP_STATE_LEN_MAX,
			cam_queue_empty_state_free);
		g_empty_interruption_q = &grp->empty_interruption_q;
		cam_queue_init(g_empty_interruption_q, CAM_INT_EMP_Q_LEN_MAX,
			cam_queue_empty_interrupt_free);
		cam_queue_init(&grp->mul_share_buf_q,
			CAM_SHARED_BUF_NUM, camcore_k_frame_put);
		g_empty_mv_state_q = &grp->empty_mv_state_q;
		cam_queue_init(g_empty_mv_state_q, CAM_EMP_STATE_LEN_MAX, cam_queue_empty_mv_state_free);

		spin_unlock_irqrestore(&grp->module_lock, flag);
		/* create recovery thread */
		if (grp->hw_info->ip_dcam[DCAM_ID_0]->recovery_support) {
			thrd = &grp->recovery_thrd;
			sprintf(thrd->thread_name, "cam_recovery");
			ret = camcore_thread_create(grp, thrd, camcore_recovery_proc);
			if (ret)
				pr_warn("warning: creat recovery thread fail\n");
		}
		pr_info("init frm_q %px state_q %px mv_state_q %px\n", g_empty_frm_q, g_empty_state_q, g_empty_mv_state_q);
	}

	module->idx = idx;
	module->grp = grp;
	grp->module[idx] = module;
	file->private_data = (void *)module;

	pr_info("sprd_img: open end! %d, %px, %px, grp %px\n",
		idx, module, grp->module[idx], grp);

	return 0;

init_fail:
	vfree(module);

alloc_fail:
	spin_lock_irqsave(&grp->module_lock, flag);
	grp->module_used &= ~(1 << idx);
	grp->module[idx] = NULL;
	spin_unlock_irqrestore(&grp->module_lock, flag);

exit:
	atomic_dec(&grp->camera_opened);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	pm_runtime_put_sync(&grp->hw_info->pdev->dev);
#endif

	pr_err("fail to open camera %d\n", ret);
	return ret;
}

/* sprd_img_release may be called for all state.
 * if release is called,
 * all other system calls in this file should be returned before.
 * state may be (RUNNING / IDLE / INIT).
 */
static int camcore_release(struct inode *node, struct file *file)
{
	int ret = 0;
	int idx = 0;
	unsigned long flag;
	struct camera_group *group = NULL;
	struct camera_module *module;

	pr_info("sprd_img: cam release start.\n");

	module = (struct camera_module *)file->private_data;
	if (!module || !module->grp) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	module->private_key = 0;
	group = module->grp;
	idx = module->idx;

	if (module->grp->camsec_cfg.camsec_mode  != SEC_UNABLE) {
		bool ret = 0;

		module->grp->camsec_cfg.camsec_mode = SEC_UNABLE;
		ret = cam_trusty_security_set(&module->grp->camsec_cfg,
			CAM_TRUSTY_EXIT);

		pr_info("camca :camsec_mode%d, ret %d\n",
			module->grp->camsec_cfg.camsec_mode, ret);
	}

	pr_info("cam %d, state %d\n", idx,
		atomic_read(&module->state));
	pr_info("used: %d, module %px, %px, grp %px\n",
		group->module_used, module, group->module[idx], group);

	spin_lock_irqsave(&group->module_lock, flag);
	if (((group->module_used & (1 << idx)) == 0) ||
		(group->module[idx] != module)) {
		pr_err("fail to release camera %d. used:%x, module:%px\n",
			idx, group->module_used, module);
		spin_unlock_irqrestore(&group->module_lock, flag);
		return -EFAULT;
	}
	spin_unlock_irqrestore(&group->module_lock, flag);

	down_read(&group->switch_recovery_lock);
	ret = camioctl_stream_off(module, 0L);
	up_read(&group->switch_recovery_lock);

	camcore_module_deinit(module);

	if (atomic_read(&module->state) == CAM_IDLE) {
	/* function "camioctl_cam_res_put" couldn't be called, when HAL sever is killed.
	* cause camera device did not close completely.
	* So. we need clear camera device resoures when HAL process exited abnormally.
	*/
		module->attach_sensor_id = -1;

		camcore_thread_stop(&module->dcam_offline_proc_thrd);
		camcore_thread_stop(&module->cap_thrd);
		camcore_thread_stop(&module->zoom_thrd);
		camcore_thread_stop(&module->buf_thrd);
		camcore_thread_stop(&module->dump_thrd);
		camcore_thread_stop(&module->mes_thrd);

		if (module->dcam_dev_handle) {
			pr_info("force close dcam %px\n", module->dcam_dev_handle);
			module->dcam_dev_handle->dcam_pipe_ops->close(module->dcam_dev_handle);
			dcam_core_pipe_dev_put(module->dcam_dev_handle);
			module->dcam_dev_handle = NULL;
		}

		if (module->isp_dev_handle) {
			pr_info("force close isp %px\n", module->isp_dev_handle);
			module->isp_dev_handle->isp_ops->close(module->isp_dev_handle);
			isp_core_pipe_dev_put(module->isp_dev_handle);
			module->isp_dev_handle = NULL;
		}
	}

	spin_lock_irqsave(&group->module_lock, flag);
	group->module_used &= ~(1 << idx);
	group->module[idx] = NULL;
	spin_unlock_irqrestore(&group->module_lock, flag);

	vfree(module);
	file->private_data = NULL;

	if (atomic_dec_return(&group->camera_opened) == 0) {
		spin_lock_irqsave(&group->module_lock, flag);
		if (group->mul_share_pyr_rec_buf) {
			camcore_k_frame_put(group->mul_share_pyr_rec_buf);
			group->mul_share_pyr_rec_buf = NULL;
		}

		if (group->mul_share_pyr_dec_buf) {
			camcore_k_frame_put(group->mul_share_pyr_dec_buf);
			group->mul_share_pyr_dec_buf = NULL;
		}
		pr_info("release %px\n", g_empty_frm_q);

		/* g_leak_debug_cnt should be 0 after clr, or else memory leak.
		 */
		cam_queue_clear(&group->mul_share_buf_q, struct camera_frame, list);
		cam_queue_clear(g_empty_frm_q, struct camera_frame, list);
		g_empty_frm_q = NULL;
		cam_queue_clear(g_empty_state_q, struct isp_stream_ctrl, list);
		g_empty_state_q = NULL;
		cam_queue_clear(g_empty_interruption_q, struct camera_interrupt, list);
		g_empty_interruption_q = NULL;
		cam_queue_clear(g_empty_mv_state_q, struct dcam_3dnrmv_ctrl, list);
		g_empty_mv_state_q = NULL;

		ret = cam_buf_mdbg_check();
		atomic_set(&group->runner_nr, 0);
		atomic_set(&group->mul_buf_alloced, 0);
		atomic_set(&group->mul_pyr_buf_alloced, 0);
		group->is_mul_buf_share = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		__pm_relax(group->ws);
#endif
		spin_unlock_irqrestore(&group->module_lock, flag);
		camcore_thread_stop(&group->recovery_thrd);
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = pm_runtime_put_sync(&group->hw_info->pdev->dev);
#endif

	pr_info("sprd_img: cam %d release end.\n", idx);

	return ret;
}

static const struct file_operations image_fops = {
	.open = camcore_open,
	.unlocked_ioctl = camcore_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = camcore_ioctl_compat,
#endif
	.release = camcore_release,
	.read = camcore_read,
	.write = camcore_write,
};

static struct miscdevice image_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = IMG_DEVICE_NAME,
	.fops = &image_fops,
};

static int camcore_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct camera_group *group = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	pr_info("Start camera img probe\n");
	group = kzalloc(sizeof(struct camera_group), GFP_KERNEL);
	if (group == NULL) {
		pr_err("fail to alloc memory\n");
		return -ENOMEM;
	}

	ret = misc_register(&image_dev);
	if (ret) {
		pr_err("fail to register misc devices, ret %d\n", ret);
		kfree(group);
		return -EACCES;
	}

	image_dev.this_device->of_node = pdev->dev.of_node;
	image_dev.this_device->platform_data = (void *)group;
	group->md = &image_dev;
	group->pdev = pdev;
	group->hw_info = (struct cam_hw_info *)
		of_device_get_match_data(&pdev->dev);
	if (!group->hw_info) {
		pr_err("fail to get hw_info\n");
		goto probe_pw_fail;
	}
	atomic_set(&group->camera_opened, 0);
	atomic_set(&group->runner_nr, 0);
	atomic_set(&group->recovery_state, 0);
	spin_lock_init(&group->module_lock);
	spin_lock_init(&group->rawproc_lock);
	spin_lock_init(&g_reg_wr_lock);

	mutex_init(&group->dual_deal_lock);
	mutex_init(&group->pyr_mulshare_lock);
	init_rwsem(&group->switch_recovery_lock);
	pr_info("sprd img probe pdev name %s\n", pdev->name);
	pr_info("sprd dcam dev name %s\n", pdev->dev.init_name);
	ret = dcam_drv_dt_parse(pdev, group->hw_info, &group->dcam_count);
	if (ret) {
		pr_err("fail to parse dcam dts\n");
		goto probe_pw_fail;
	}

	pr_info("sprd isp dev name %s\n", pdev->dev.init_name);
	ret = isp_drv_dt_parse(pdev->dev.of_node, group->hw_info,
		&group->isp_count);
	if (ret) {
		pr_err("fail to parse isp dts\n");
		goto probe_pw_fail;
	}

	if (group->hw_info && group->hw_info->soc_dcam->pdev)
		ret = cam_buf_iommudev_reg(
			&group->hw_info->soc_dcam->pdev->dev,
			CAM_IOMMUDEV_DCAM);

	if (group->hw_info && group->hw_info->soc_dcam_lite && group->hw_info->soc_dcam_lite->pdev)
		ret = cam_buf_iommudev_reg(
			&group->hw_info->soc_dcam_lite->pdev->dev,
			CAM_IOMMUDEV_DCAM_LITE);

	if (group->hw_info && group->hw_info->soc_isp->pdev)
		ret = cam_buf_iommudev_reg(
			&group->hw_info->soc_isp->pdev->dev,
			CAM_IOMMUDEV_ISP);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	group->ws = wakeup_source_create("Camdrv Wakeuplock");
	wakeup_source_add(group->ws);
#endif

	/* for get ta status
	 * group->ca_conn  = cam_trusty_connect();
	 */
	if (group->ca_conn)
		pr_info("cam ca-ta unconnect\n");

	group->debugger.hw = group->hw_info;
	ret = cam_debugger_init(&group->debugger);
	if (ret)
		pr_err("fail to init cam debugfs\n");

	return 0;

probe_pw_fail:
	misc_deregister(&image_dev);
	kfree(group);

	return ret;
}

static int camcore_remove(struct platform_device *pdev)
{
	struct camera_group *group = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	group = image_dev.this_device->platform_data;
	if (group) {
		cam_buf_iommudev_unreg(CAM_IOMMUDEV_DCAM);
		cam_buf_iommudev_unreg(CAM_IOMMUDEV_DCAM_LITE);
		cam_buf_iommudev_unreg(CAM_IOMMUDEV_ISP);
		if (group->ca_conn)
			cam_trusty_disconnect();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		wakeup_source_remove(group->ws);
		wakeup_source_destroy(group->ws);
#endif
		kfree(group);
		image_dev.this_device->platform_data = NULL;
	}
	misc_deregister(&image_dev);

	return 0;
}

static const struct of_device_id sprd_cam_of_match[] = {
	#if defined (PROJ_SHARKL3)
	{ .compatible = "sprd,sharkl3-cam", .data = &sharkl3_hw_info},
	#elif defined (PROJ_SHARKL5)
	{ .compatible = "sprd,sharkl5-cam", .data = &sharkl5_hw_info},
	#elif defined (PROJ_SHARKL5PRO)
	{ .compatible = "sprd,sharkl5pro-cam", .data = &sharkl5pro_hw_info},
	#elif defined (PROJ_QOGIRL6)
	{ .compatible = "sprd,qogirl6-cam", .data = &qogirl6_hw_info},
	#elif defined (PROJ_QOGIRN6PRO)
	{ .compatible = "sprd,qogirn6pro-cam", .data = &qogirn6pro_hw_info},
	#elif defined (PROJ_QOGIRN6L)
	{ .compatible = "sprd,qogirn6l-cam", .data = &qogirn6l_hw_info},
	#endif
	{ },
};

static struct platform_driver sprd_img_driver = {
	.probe = camcore_probe,
	.remove = camcore_remove,
	.driver = {
		.name = IMG_DEVICE_NAME,
		.of_match_table = of_match_ptr(sprd_cam_of_match),
	},
};

module_platform_driver(sprd_img_driver);

MODULE_DESCRIPTION("SPRD CAM Driver");
MODULE_AUTHOR("Multimedia_Camera@SPRD");
MODULE_LICENSE("GPL");
