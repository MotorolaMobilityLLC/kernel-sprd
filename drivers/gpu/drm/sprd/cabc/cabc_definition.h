/*
 *Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef _CABC_DEFINITION_H_
#define _CABC_DEFINITION_H_

#include <linux/string.h>
#include <linux/kernel.h>

#define STEP2_INIT   2
#define HIST_BIN_CABC  14

#define PRE  0
#define PRE_PRE 1

struct bl_out_tag {
	u8 cur;
	u8 pre;
	u8 pre2;
	u8 pre3;
	int cur_fix_ui;
	u8 pre_fix_ui;
	int cur_fix_video;
	u8 pre_fix_video;
};

struct cabc_context_tag {
	int width;
	int height;
	int pixel_total_num;
};

struct max_hist_num_tag {
	int max_hist_num_cur;
	int  max_hist_num_pre;
	int  max_hist_num_pre_1;
	int  max_hist_num_pre_2;
};
struct cabc_para {
	u32 cabc_hist[32];
	u16 gain;
	u16 bl_fix;
	u16 cur_bl;
	bool is_VSP_working;
};

int cabc_trigger(struct cabc_para *para, int frame_no);
void init_cabc(int img_width, int img_height);
void step_set(int step0, int step1, int step2, int scene_change_thr);

#endif
