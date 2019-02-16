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

#include "sprd_dpu.h"
#include "bl_decision.h"
#include "cabc_global.h"

int sign_diff(u8 a, u8 b)
{
	if (a > b)
		return 1;
	else if (a < b)
		return -1;
	else
		return 0;
}

u8 smooth_step2(u8 diff, u8 div_result, u8 step2, u8 step)
{
	u8 step_result;

	if (diff <= 20)
		step_result = step2 + div_result * step;
	else if (diff <= 40)
		step_result = step2 + div_result * (2 + step);
	else if (diff <= 60)
		step_result = step2 + div_result * (4 + step);
	else
		step_result = step2 + 100;
	return step_result;
}

u8 gen_step2(int cur_pre, int pre_pre2, int pre2_pre3, int scene_flag,
	u8 div_result, u8 *step2, u8 step0, u8 step02, u8 abs_diff_bl_cur_pre)
{
	u8 temp_step2;

	if ((cur_pre < 0) && (pre_pre2 <= 0) && (pre2_pre3 <= 0)) {
		/* darker gradually with smaller step*/
		if ((scene_flag == 1) && (div_result > 4))
			*step2 = step02 + 2;
		else
			*step2 = smooth_step2(abs_diff_bl_cur_pre,
			div_result, step02, 0);
		temp_step2 = *step2;
	} else if ((cur_pre == 0) && (pre_pre2 > 0) && (pre2_pre3 == 0)) {
		/* brighter gradually with bigger step ,second frame*/
		temp_step2 = *step2;
	} else if ((cur_pre == 0) && (pre_pre2 == 0) && (pre2_pre3 > 0)) {
		/* brighter gradually with bigger step ,third frame*/
		temp_step2 = *step2;
	} else if ((cur_pre > 0) && (pre_pre2 == 0) && (pre2_pre3 == 0)) {
		/* brighter gradually with bigger step ,first frame*/
		if ((div_result > 4) && (scene_flag == 0))
			*step2 = smooth_step2(abs_diff_bl_cur_pre, div_result,
			step02, 3);
		else if ((div_result > 4) && (scene_flag == 1))
			*step2 = step02 + 2;
		else
			*step2 = smooth_step2(abs_diff_bl_cur_pre,
			div_result, step02, 1);
		temp_step2 = *step2;
	} else if ((cur_pre > 0) && ((pre_pre2 != 0) || (pre2_pre3 != 0))) {
		if (scene_flag == 1)
			*step2 = step02 + 2;
		else
			*step2 = smooth_step2(abs_diff_bl_cur_pre,
			div_result, step02, 3);
		temp_step2 = *step2;
	} else {
		temp_step2 = step0;
		*step2 = step0;
	}
	return temp_step2;
}

void backlight_decision(int *hist_cabc, struct bl_out_tag *bl, int max_hist_num)
{
	int i;

	if (max_hist_num >= 10) {
		bl->cur = g_min_backlight;
	} else {
		bl->cur = g_min_backlight + g_brightness_step[14];
		for (i = 0; i < 10; i++) {
			if ((hist_cabc[i] * 100) >= (hist_cabc[13] *
				g_cabc_percent_th)) {
				bl->cur = g_min_backlight +
					g_brightness_step[i];
				break;
			}
		}
	}
}

void backlight_fix_ui(struct bl_out_tag *bl, struct max_hist_num_tag max_hist,
	u8 *s_step2, u8 step0, u8 step1, u8 step2, u8 scene_flag,
	int hist_num, int change_num_diff, u8 *change_cnt,
	int sign_num_diff)
{
	u8 div_result, temp_step2;
	int change_ratio, flag = 1;
	int diff_bl_cur_pre = bl->cur - bl->pre;
	int diff_bl_pre_pre2 = sign_diff(bl->pre, bl->pre2);
	int diff_bl_pre2_pre3 = sign_diff(bl->pre2, bl->pre3);
	u8 abs_diff_bl_cur_pre = abs(diff_bl_cur_pre);
	u8  cur_max_hist = max_hist.max_hist_num_cur;
	u8  pre_max_hist = max_hist.max_hist_num_pre;
	u8  pre_1_max_hist = max_hist.max_hist_num_pre_1;
	u8  pre_2_max_hist = max_hist.max_hist_num_pre_2;
	int sign_diff_max_hist_num_1 = sign_diff(cur_max_hist, pre_max_hist);
	int sign_diff_max_hist_num_2 = sign_diff(pre_max_hist, pre_1_max_hist);
	int sign_diff_max_hist_num_3 = sign_diff(pre_1_max_hist,
		pre_2_max_hist);
	int sign_diff_pre_fix_cur = sign_diff(bl->pre_fix_ui, bl->cur);

	if (change_num_diff != 0)
		change_ratio = hist_num / change_num_diff;
	else
		change_ratio = hist_num;
	div_result = abs_diff_bl_cur_pre / 10;
	if ((sign_diff_max_hist_num_1 == 0) && (sign_diff_max_hist_num_2 == 0)
		&& (sign_diff_max_hist_num_3 == 0)) {
		/* coming in a stable status*/
		*change_cnt = 0;
		if ((diff_bl_cur_pre == 0) && (diff_bl_pre_pre2) == 0) {
			if (change_ratio >= 80) {
				bl->cur_fix_ui = bl->pre_fix_ui - step0 *
					sign_diff_pre_fix_cur;
			} else {
				if ((cur_max_hist == 11) &&
					(sign_num_diff > 0)) {
					bl->cur_fix_ui = bl->pre_fix_ui +
						step0 * abs(60 - change_ratio);
					flag = 0;
				} else {
					if (sign_diff_pre_fix_cur >= 0)
						bl->cur_fix_ui = bl->pre_fix_ui;
					else
						bl->cur_fix_ui =
						bl->pre_fix_ui -
						step1 * abs(80 - change_ratio);
				}
			}
		} else {
			bl->cur_fix_ui = bl->pre_fix_ui - step0 *
					sign_diff_pre_fix_cur;
		}
	} else {
		/* ui menu coming  or exiting ,first frame*/
		if ((sign_diff_max_hist_num_2 == 0) &&
			(sign_diff_max_hist_num_3 == 0)) {
			*change_cnt = 1;
			/* to avoid ui menu flicking  configure menu*/
			bl->cur_fix_ui = bl->pre_fix_ui + step0;
		}  else if ((sign_diff_max_hist_num_1 == 0) &&
			(sign_diff_max_hist_num_3 == 0) && (*change_cnt == 1)) {
			/* ui menu coming  or exiting ,second frame*/
			*change_cnt = *change_cnt + 1;
			/* to avoid ui menu flicking   configure menu*/
			bl->cur_fix_ui = bl->pre_fix_ui + step0;
		} else if ((sign_diff_max_hist_num_1 == 0) &&
			(sign_diff_max_hist_num_2 == 0) && (*change_cnt == 2)) {
			*change_cnt = 0;
			/* to avoid ui menu flicking   configure menu*/
			bl->cur_fix_ui = bl->pre_fix_ui + step0;
		} else {
			*change_cnt = 0;
			temp_step2 = gen_step2(diff_bl_cur_pre,
				diff_bl_pre_pre2, diff_bl_pre2_pre3,
				scene_flag, div_result, s_step2,
			step0, step2, abs_diff_bl_cur_pre);
			bl->cur_fix_ui = bl->pre_fix_ui + temp_step2;
		}
	}
	if (flag) {
		if (((sign_diff_pre_fix_cur > 0) &&
			(bl->cur_fix_ui < bl->cur)) ||
			((sign_diff_pre_fix_cur < 0) &&
			(bl->cur_fix_ui > bl->cur))) {
			/* to avoid shaking*/
			bl->cur_fix_ui = bl->cur;
		}
	}
}

void backlight_fix_video(struct max_hist_num_tag max_hist, u8 s_scene_change,
	u8 step0, struct bl_out_tag *bl)
{
	int sign_diff_1, sign_diff_2, sign_diff_3;
	int backlight_diff_abs = abs(bl->cur - bl->pre);
	int b1_cur_pre_fix_diff = bl->cur - bl->pre_fix_video;
	int sign_diff_cur_pre = sign_diff(bl->cur, bl->pre);
	int sign_diff_cur_pre_fix = sign_diff(bl->cur, bl->pre_fix_video);

	if ((s_scene_change == 0) &&
		((max_hist.max_hist_num_cur < 13) ||
		(max_hist.max_hist_num_pre < 13))) {
		sign_diff_1 = sign_diff(bl->pre2, bl->pre3);
		sign_diff_2 = sign_diff(bl->pre, bl->pre2);
		sign_diff_3 = sign_diff(bl->cur, bl->pre);
		if (((sign_diff_1 > 0) && (sign_diff_2 > 0) &&
			(sign_diff_3 > 0)) ||
			((sign_diff_1 < 0) && (sign_diff_2 < 0) &&
			(sign_diff_3 < 0))) {
			bl->cur_fix_video = bl->pre_fix_video +
				step0 * sign_diff_3;
		} else if ((sign_diff_1 == 0) &&
				(sign_diff_2 * sign_diff_3 > 0)) {
			if ((bl->pre2 == g_min_backlight) ||
				(bl->pre2 == g_max_backlight)) {
				bl->cur_fix_video = bl->pre_fix_video +
					step0 * sign_diff_3;
			}
		} else {
			if (backlight_diff_abs <= 64) {
				if (b1_cur_pre_fix_diff != 0) {
					bl->cur_fix_video = bl->pre_fix_video +
						step0 * sign_diff_cur_pre_fix;
				} else {
					bl->cur_fix_video = bl->pre_fix_video;
				}
			} else {
				bl->cur_fix_video = bl->cur;
			}
		}
	} else {
		if (sign_diff_cur_pre * sign_diff_cur_pre_fix < 0) {
			bl->cur_fix_video = bl->pre_fix_video;
		} else if (sign_diff_cur_pre * sign_diff_cur_pre_fix > 0) {
			bl->cur_fix_video = bl->pre_fix_video +
					step0 * sign_diff_cur_pre;
		} else if (sign_diff_cur_pre == 0 &&
				sign_diff_cur_pre_fix != 0) {
			if (sign_diff_cur_pre_fix > 0) {
				bl->cur_fix_video = bl->pre_fix_video + step0;
				if (bl->cur_fix_video > bl->cur)
					bl->cur_fix_video = bl->cur;
			} else {
				bl->cur_fix_video = bl->pre_fix_video - step0;
				if (bl->cur_fix_video < bl->cur)
					bl->cur_fix_video = bl->cur;
			}
		} else if (sign_diff_cur_pre_fix == 0) {
			bl->cur_fix_video = bl->pre_fix_video;
		}
	}
}
