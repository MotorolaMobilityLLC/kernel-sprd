#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include "getyuvinput.h"

void show_info(struct YUV_PATH_INFO_tag *path_info)
{
	pr_err("src: %d %d, ",
		path_info->src_size_x,
		path_info->src_size_y);
	pr_err("des: %d %d, ",
		path_info->scaler_info.scaler_out_width,
		path_info->scaler_info.scaler_out_height);
	pr_err("trim0: %d %d %d %d, ",
		path_info->trim0_info.trim_start_x,
		path_info->trim0_info.trim_start_y,
		path_info->trim0_info.trim_size_x,
		path_info->trim0_info.trim_size_y);
	pr_err("trim1: %d %d %d %d\n",
		path_info->trim1_info.trim_start_x,
		path_info->trim1_info.trim_start_y,
		path_info->trim1_info.trim_size_x,
		path_info->trim1_info.trim_size_y);
	pr_err("phase: %d %d %d %d, ",
		path_info->scaler_info.scaler_init_phase_int,
		path_info->scaler_info.scaler_init_phase_rmd,
		path_info->scaler_info.scaler_chroma_init_phase_int,
		path_info->scaler_info.scaler_chroma_init_phase_rmd);
	pr_err("factor: %d %d\n",
		path_info->scaler_info.scaler_factor_in_hor,
		path_info->scaler_info.scaler_factor_out_hor);
}

void init_dcam_info(struct DCAMINFO *dcam_info)
{
	memset(dcam_info, 0x00, sizeof(*dcam_info));
	dcam_info->cowork_mode = 1;
	dcam_info->cowork_overlap = 32;
	dcam_info->dcam_in_width = 4632;
	dcam_info->dcam_in_height = 3480;

	dcam_info->dcam_yuv_path[0].path_en = 1;
	dcam_info->dcam_yuv_path[0].src_size_x = 4632;
	dcam_info->dcam_yuv_path[0].src_size_y = 3480;
	dcam_info->dcam_yuv_path[0].trim0_info.trim_start_x = 0;
	dcam_info->dcam_yuv_path[0].trim0_info.trim_start_y = 0;
	dcam_info->dcam_yuv_path[0].trim0_info.trim_size_x = 4632;
	dcam_info->dcam_yuv_path[0].trim0_info.trim_size_y = 3480;
	dcam_info->dcam_yuv_path[0].scaler_info.scaler_out_width = 2400;
	dcam_info->dcam_yuv_path[0].scaler_info.scaler_out_height = 1800;
	dcam_info->dcam_yuv_path[0].deci_info.deci_x = 1;
	dcam_info->dcam_yuv_path[0].deci_info.deci_y = 1;
	dcam_info->dcam_yuv_path[0].outdata_format = 1;
	dcam_info->dcam_yuv_path[0].outdata_mode = 1;
	dcam_info->dcam_yuv_path[0].scaler_info.scaler_en = 1;

	dcam_info->dcam_yuv_path[1].path_en = 1;
	dcam_info->dcam_yuv_path[1].src_size_x = 4632;
	dcam_info->dcam_yuv_path[1].src_size_y = 3480;
	dcam_info->dcam_yuv_path[1].trim0_info.trim_start_x = 0;
	dcam_info->dcam_yuv_path[1].trim0_info.trim_start_y = 0;
	dcam_info->dcam_yuv_path[1].trim0_info.trim_size_x = 4632;
	dcam_info->dcam_yuv_path[1].trim0_info.trim_size_y = 3480;
	dcam_info->dcam_yuv_path[1].scaler_info.scaler_out_width = 2316;
	dcam_info->dcam_yuv_path[1].scaler_info.scaler_out_height = 1740;
	dcam_info->dcam_yuv_path[1].deci_info.deci_x = 1;
	dcam_info->dcam_yuv_path[1].deci_info.deci_y = 1;
	dcam_info->dcam_yuv_path[1].outdata_format = 1;
	dcam_info->dcam_yuv_path[1].outdata_mode = 1;
	dcam_info->dcam_yuv_path[1].scaler_info.scaler_en = 1;

	dcam_info->dcam_yuv_path[2].path_en = 1;
	dcam_info->dcam_yuv_path[2].src_size_x = 4632;
	dcam_info->dcam_yuv_path[2].src_size_y = 3480;
	dcam_info->dcam_yuv_path[2].trim0_info.trim_start_x = 0;
	dcam_info->dcam_yuv_path[2].trim0_info.trim_start_y = 0;
	dcam_info->dcam_yuv_path[2].trim0_info.trim_size_x = 4632;
	dcam_info->dcam_yuv_path[2].trim0_info.trim_size_y = 3480;
	dcam_info->dcam_yuv_path[2].scaler_info.scaler_out_width = 1280;
	dcam_info->dcam_yuv_path[2].scaler_info.scaler_out_height = 960;
	dcam_info->dcam_yuv_path[2].deci_info.deci_x = 1;
	dcam_info->dcam_yuv_path[2].deci_info.deci_y = 1;
	dcam_info->dcam_yuv_path[2].outdata_format = 1;
	dcam_info->dcam_yuv_path[2].outdata_mode = 1;
	dcam_info->dcam_yuv_path[2].scaler_info.scaler_en = 1;
}

int dcam_path_init(void)
{
	struct DCAMINFO *dcamInfo = NULL;

	dcamInfo = vzalloc(sizeof(struct DCAMINFO));
	init_dcam_info(dcamInfo);

	if (dcamInfo->dcam_yuv_path[0].path_en ||
	   dcamInfo->dcam_yuv_path[1].path_en ||
	   dcamInfo->dcam_yuv_path[2].path_en) {
		InitDcamInfo(dcamInfo);
		pr_err("begin to print result:\n");
		pr_err("dcam0_path1:\n");
		show_info(&(dcamInfo->InputInfo[0][0]));
		pr_err("dcam0_path2:\n");
		show_info(&(dcamInfo->InputInfo[0][1]));
		pr_err("dcam0_path3:\n");
		show_info(&(dcamInfo->InputInfo[0][2]));
		pr_err("dcam1_path1:\n");
		show_info(&(dcamInfo->InputInfo[1][0]));
		pr_err("dcam1_path2:\n");
		show_info(&(dcamInfo->InputInfo[1][1]));
		pr_err("dcam1_path3:\n");
		show_info(&(dcamInfo->InputInfo[1][2]));
		pr_err("print result end!\n");
	}
	return 0;
}

void dcam_path_exit(void)
{
	pr_err("dcam_path exit!\n");
}

module_init(dcam_path_init);
module_exit(dcam_path_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ming.tang <ming.tang@spreadtrum.com>");
MODULE_DESCRIPTION("dcam path test");
