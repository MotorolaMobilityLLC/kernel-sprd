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

#ifndef _DCAM_CORE_HEADER_
#define _DCAM_CORE_HEADER_

#include "sprd_img.h"
#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "flash_drv.h"
#include "dcam_block.h"
#include "dcam_drv.h"
#include "isp_drv.h"
#include "isp_statis_buf.h"

#define IMG_DEVICE_NAME                 "sprd_image"
#define CAMERA_INVALID_FOURCC           0xFFFFFFFF
#define CAMERA_QUEUE_LENGTH             96
#define CAMERA_TIMING_LEN               16

#define CAMERA_ZOOM_LEVEL_MAX           4
#define CAMERA_ZOOM_STEP(x, y)          (((x) - (y)) / CAMERA_ZOOM_LEVEL_MAX)
#define CAMERA_PIXEL_ALIGNED            4
#define CAMERA_WIDTH(w)                 ((w) & ~(CAMERA_PIXEL_ALIGNED - 1))
#define CAMERA_HEIGHT(h)                ((h) & ~(CAMERA_PIXEL_ALIGNED - 1))

#define DCAM_TIMEOUT                    1500

enum {
	PATH_IDLE = 0x00,
	PATH_RUN,
};

struct camera_format {
	char *name;
	uint32_t fourcc;
	int depth;
};

struct camera_node {
	uint32_t irq_flag;
	uint32_t irq_type;
	uint32_t f_type;
	uint32_t index;
	uint32_t height;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t phy_addr;
	uint32_t vir_addr;
	uint32_t addr_offset;
	uint32_t kaddr[2];
	uint32_t buf_size;
	uint32_t irq_property;
	uint32_t invalid_flag;
	uint32_t frame_id;
	uint32_t mfd[3];
	uint32_t reserved[2];
	struct timeval time;
	ktime_t boot_time;
	struct sprd_img_vcm_dac_info dac_info;
};

struct camera_queue {
	struct camera_node node[CAMERA_QUEUE_LENGTH];
	struct camera_node *write;
	struct camera_node *read;
	uint32_t wcnt;
	uint32_t rcnt;
	spinlock_t lock;
};

struct camera_img_buf_addr {
	struct camera_addr frm_addr;
	struct camera_addr frm_addr_vir;
};

struct camera_img_buf_queue {
	struct camera_img_buf_addr buf_addr[DCAM_FRM_CNT_MAX];
	struct camera_img_buf_addr *write;
	struct camera_img_buf_addr *read;
	uint32_t wcnt;
	uint32_t rcnt;
};

struct camera_path_spec {
	uint32_t is_work;
	uint32_t is_from_isp;
	uint32_t is_loose;
	uint32_t rot_mode;
	uint32_t status;
	enum isp_path_mode path_mode;
	struct camera_size in_size;
	struct camera_path_dec img_deci;
	struct camera_rect in_rect;
	struct camera_size out_size;
	struct camera_size isp_out_size;
	enum dcam_fmt out_fmt;
	struct camera_endian_sel end_sel;
	uint32_t fourcc;
	uint32_t frm_id_base;
	uint32_t frm_type;
	uint32_t index[DCAM_FRM_CNT_MAX];
	struct camera_img_buf_queue buf_queue;
	struct camera_img_buf_queue tmp_buf_queue;
	struct camera_addr frm_reserved_addr;
	struct camera_addr frm_reserved_addr_vir;
	uint32_t frm_cnt_act;
	uint32_t path_frm_deci;
	uint32_t path_skip_num;
	struct dcam_regular_desc regular_desc;
	struct sprd_pdaf_control pdaf_ctrl;
	struct sprd_ebd_control ebd_ctrl;
	uint32_t bin_ratio;
	uint32_t isp_fetch_fmt;
};

struct camera_pulse_queue {
	struct sprd_img_vcm_param node[CAMERA_QUEUE_LENGTH];
	struct sprd_img_vcm_param *write;
	struct sprd_img_vcm_param *read;
	uint32_t wcnt;
	uint32_t rcnt;
	spinlock_t lock;
};

struct camera_pulse_type {
	struct workqueue_struct *pulse_work_queue;
	struct work_struct pulse_work;
	uint32_t pulse_line;
	int32_t last_vcm_pos;
	uint32_t enable_debug_info;
	struct sprd_img_vcm_dac_info dac_info;
	struct camera_pulse_queue vcm_queue;
	struct mutex pulse_mutex;
};

struct camera_dbg_dump {
	bool is_inited; /* to check if already inited */
	uint32_t bin_cnt; /* dump cnt */
	uint32_t full_cnt; /* dump cnt */
	struct camera_frame *bin_frame;
	struct camera_frame *full_frame;
	struct workqueue_struct *dump_work_queue;
	struct work_struct dump_bin_work;
	struct work_struct dump_full_work;
	struct mutex bin_lock;
	struct mutex full_lock;
};

struct camera_info {
	uint32_t if_mode;
	uint32_t sn_mode;
	uint32_t img_ptn;
	uint32_t data_bits;
	uint32_t is_loose;
	struct dcam_cap_sync_pol sync_pol;
	uint32_t frm_deci;
	struct dcam_cap_dec img_deci;
	struct camera_size cap_in_size;
	struct camera_rect cap_in_rect;
	struct camera_size cap_out_size;
	struct camera_size small_size;
	struct camera_size dst_size;
	struct camera_size sn_max_size;
	uint32_t pxl_fmt;
	uint32_t need_isp_tool;
	uint32_t need_isp;
	uint32_t rt_refocus;
	uint32_t is_raw_rt;
	struct camera_rect path_input_rect;
	struct camera_path_spec dcam_path[CAMERA_MAX_PATH];
	uint32_t capture_mode;
	uint32_t skip_number;
	struct sprd_img_set_flash set_flash;
	uint32_t flash_last_status;
	uint32_t flash_skip_fid;
	uint32_t frame_index;
	uint32_t after_af;
	uint32_t is_smooth_zoom;
	struct timeval timestamp;
	uint32_t scene_mode;
	uint32_t is_slow_motion;
	struct camera_pulse_type pulse_info;
	uint32_t is_3dnr;
	uint32_t is_hdr;
	struct sprd_flash_capacity capacity;
	uint32_t uframe_sync;/* frame sync for video and callback channel */
	struct camera_dbg_dump dump_info;
	uint32_t raw_callback;
};

struct camera_group;
struct camera_dev {
	struct mutex dcam_mutex;
	struct mutex cap_mutex;
	struct completion irq_com;
	atomic_t users;
	struct camera_info dcam_cxt;
	atomic_t stream_on;
	struct camera_queue queue;
	struct timer_list dcam_timer;
	atomic_t run_flag;
	struct completion flash_thread_com;
	struct task_struct *flash_thread;
	uint32_t is_flash_thread_stop;
	uint32_t frame_skipped;
	uint32_t channel_id;
	uint32_t use_path;
	enum dcam_id idx;
	struct isp_statis_buf_input init_inptr;
	void *isp_dev_handle;
	struct camera_group *cam_grp;
	uint32_t cap_flag;
	struct dcam_statis_module statis_module_info;
	struct camera_frame *frame;
	uint32_t is_simulation_mode;
	uint32_t frame_id;
	uint32_t bin_frame_id;
	uint32_t afm_delta_frame;
};

struct camera_group {
	uint32_t dev_inited;
	uint32_t mode_inited;
	uint32_t dcam_res_used;
	uint32_t dcam_count;
	uint32_t isp_count;
	atomic_t camera_opened;
	atomic_t dcam_run_count;
	struct camera_dev *dev[DCAM_ID_MAX];
	//struct ion_client *cam_ion_client[DCAM_ID_MAX];
	struct platform_device *pdev;
	struct wakeup_source ws;
	struct mutex camera_dualcam_mutex;
	struct completion dualcam_recovery_com;
	void *p_dbg_info; /* used to debug camera using sysfs */
};

struct camera_file {
	int idx;
	struct camera_group *grp;
	uint32_t private_key;
};

int img_get_timestamp(struct timeval *tv);
void gen_frm_timestamp(struct frm_timestamp *pts);
int sprd_img_start_flash(struct camera_frame *frame, void *param);
int sprd_camera_stream_off(struct camera_group *group, enum dcam_id idx);
int sprd_img_get_dcam_dev(struct camera_file *pcamerafile,
				 struct camera_dev **ppdev,
				 struct camera_info **ppinfo);
#endif /* _DCAM_CORE_H_ */

