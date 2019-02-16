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

#include"scene_detect.h"
#include"cabc_global.h"

void scene_detection(int *hist, int *hist_pre, int *hist_diff,
	int *hist_pre_diff, int sum_num, int num, u8 *scene_flag,
	int scene_detect_thr, int max_hist_num)
{
	int i, flag = 1;
	int hist_diff_diff_max_no = 1;
	int hist_diff_diff, hist_diff_diff_max = 0, sum_hist_diff = 0;

	for (i = 0; i < HIST_BIN_CABC; i++) {
		hist_diff_diff = abs(hist_diff[i] - hist_pre_diff[i]);
		sum_hist_diff += hist_diff_diff;
	}
	if ((sum_hist_diff * 100) >= (sum_num * g_scene_change_thr)) {
		*scene_flag = 0;
		for (i = 0; i < HIST_BIN_CABC; i++) {
			hist_diff_diff = abs(hist_diff[i] - hist_pre_diff[i]);
			if (hist_diff_diff_max < hist_diff_diff)
				hist_diff_diff_max = hist_diff_diff;
		}
		flag = 1;
		for (i = 0; i < HIST_BIN_CABC; i++) {
			if (hist_diff_diff_max == abs(hist_diff[i] -
					hist_pre_diff[i])) {
				hist_diff_diff_max_no = i;
				if ((hist_pre_diff[i] * 100 >=
					scene_detect_thr * num) ||
				(hist_diff[i] * 100 >= scene_detect_thr * num))
					flag = 0;
			}
		}
		if ((hist_diff_diff_max != 0) && (sum_hist_diff /
			hist_diff_diff_max <= 2) && flag) {
			if ((hist_diff_diff_max_no <= 10) ||
				(max_hist_num > 10))
				*scene_flag = 1;
		}
	} else {
		*scene_flag = 1;
	}
}
