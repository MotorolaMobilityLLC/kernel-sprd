#include <linux/kernel.h>
#include <linux/io.h>
#include "getyuvinput.h"

#define floor(x, y) (x/y)
#define ASSERT(e) do {\
	if (!(e))\
		pr_err("error:%s [%s]\n", (#e), __func__);\
	} while (0)

static void calc_scaler_phase(int phase,
			      unsigned short factor,
			      short *phase_int,
			      unsigned short *phase_rmd)
{
	phase_int[0] = (char)floor(phase, factor);
	phase_rmd[0] = (unsigned short)(phase - factor*phase_int[0]);
}

void InitDcamInfo(struct DCAMINFO *pDCAMInfo)
{
	struct Crop_tag *pCapCropInfo = NULL;
	struct Decimation_tag *pCapDeciInfo = NULL;
	struct YUV_PATH_INFO_tag *pYuvPathInfo = NULL;
	struct Scaler_tag *pScalerInfo = NULL;
	int path_id;
	unsigned short cur_width = 0, cur_height = 0;
	int PIX_ALIGN = 8;
	unsigned short scl_factor_in_hor, scl_factor_out_hor;
	unsigned short scl_factor_in_ver, scl_factor_out_ver;
	unsigned char  tap_luma_ver = 8, tap_chrome_ver = 8;
	unsigned short i_w, o_w, i_h, o_h;

	if (!pDCAMInfo) {
		pr_err("%s, invalid para", __func__);
		return;
	}

	pCapCropInfo = &pDCAMInfo->cap_crop_info;
	if (pCapCropInfo->crop_en) {
		cur_width =
			pCapCropInfo->crop_end_x - pCapCropInfo->crop_start_x+1;
		cur_height =
			pCapCropInfo->crop_end_y - pCapCropInfo->crop_start_y+1;
	} else {
		cur_width = pDCAMInfo->dcam_in_width;
		cur_height = pDCAMInfo->dcam_in_height;
	}

	pCapDeciInfo = &pDCAMInfo->cap_deci_info;
	if (pCapDeciInfo->deci_x_en)
		cur_width = cur_width/pCapDeciInfo->deci_x;
	else
		cur_width = cur_width;

	if (pCapDeciInfo->deci_y_en)
		cur_height = cur_height/pCapDeciInfo->deci_y;
	else
		cur_height = cur_height;

	for (path_id = 0; path_id < YUV_PATH_NUM; path_id++) {
		pYuvPathInfo = &pDCAMInfo->dcam_yuv_path[path_id];

		pYuvPathInfo->src_size_x = cur_width;
		pYuvPathInfo->src_size_y = cur_height;
	}

	for (path_id = 0; path_id < YUV_PATH_NUM; path_id++) {
		pYuvPathInfo = &pDCAMInfo->dcam_yuv_path[path_id];

		if (pYuvPathInfo->path_en) {
			ASSERT(pYuvPathInfo->trim0_info.trim_start_x%2 == 0);
			ASSERT(pYuvPathInfo->trim0_info.trim_size_x%2 == 0);

			ASSERT((pYuvPathInfo->trim0_info.trim_start_x/
				(pYuvPathInfo->deci_info.deci_x*2))*
			       (pYuvPathInfo->deci_info.deci_x*2)
			       == pYuvPathInfo->trim0_info.trim_start_x);
			ASSERT((pYuvPathInfo->trim0_info.trim_start_y/
				(pYuvPathInfo->deci_info.deci_y))*
			       (pYuvPathInfo->deci_info.deci_y)
			       == pYuvPathInfo->trim0_info.trim_start_y);

			if (pYuvPathInfo->deci_info.deci_x_en == 0)
				pYuvPathInfo->deci_info.deci_x = 1;
			if (pYuvPathInfo->deci_info.deci_y_en == 0)
				pYuvPathInfo->deci_info.deci_y = 1;

			if (pYuvPathInfo->deci_info.deci_x_en)
				cur_width =
					pYuvPathInfo->trim0_info.trim_size_x/
					pYuvPathInfo->deci_info.deci_x;
			else
				cur_width =
					pYuvPathInfo->trim0_info.trim_size_x;

			if (pYuvPathInfo->deci_info.deci_y_en)
				cur_height =
					pYuvPathInfo->trim0_info.trim_size_y/
					pYuvPathInfo->deci_info.deci_y;
			else
				cur_height =
					pYuvPathInfo->trim0_info.trim_size_y;

			pScalerInfo = &pYuvPathInfo->scaler_info;
			if (pScalerInfo->scaler_en) {
				int  scl_initial_phase;

				if (pYuvPathInfo->outdata_mode == YUV420)
					pScalerInfo->scaling2yuv420 = 1;
				else
					pScalerInfo->scaling2yuv420 = 0;

				pScalerInfo->scaler_in_width = cur_width;
				pScalerInfo->scaler_in_height = cur_height;

				i_w = pScalerInfo->scaler_in_width;
				o_w = pScalerInfo->scaler_out_width;
				i_h = pScalerInfo->scaler_in_height;
				o_h = pScalerInfo->scaler_out_height;

				ASSERT(i_w%2 == 0 && o_w%2 == 0 &&
				       i_w <= o_w*4 && o_w <= i_w*4);
				ASSERT(i_h%2 == 0 && o_h%2 == 0 &&
				       i_h <= o_h*4 && o_h <= i_h*4);

				scl_factor_in_hor = (unsigned short)(i_w);
				scl_factor_out_hor = (unsigned short)(o_w);
				scl_factor_in_ver = (unsigned short)(i_h);
				scl_factor_out_ver = (unsigned short)(o_h);

				ASSERT(scl_factor_in_hor >= 8 &&
				       scl_factor_out_hor >= 8);
				if (pScalerInfo->scaling2yuv420) {
					ASSERT(scl_factor_in_ver >= 8 &&
					       scl_factor_out_ver/2 >= 8);
				} else {
					ASSERT(scl_factor_in_ver >= 8 &&
					       scl_factor_out_ver >= 8);
				}

				pScalerInfo->scaler_factor_in_hor =
					scl_factor_in_hor;
				pScalerInfo->scaler_factor_out_hor =
					scl_factor_out_hor;
				pScalerInfo->scaler_factor_in_ver =
					scl_factor_in_ver;
				pScalerInfo->scaler_factor_out_ver =
					scl_factor_out_ver;

				scl_initial_phase = 0;
				pScalerInfo->scaler_initial_phase =
					scl_initial_phase;
				calc_scaler_phase(scl_initial_phase,
				  scl_factor_out_hor,
				  &pScalerInfo->scaler_init_phase_int,
				  &pScalerInfo->scaler_init_phase_rmd);
				calc_scaler_phase(scl_initial_phase/2,
				  scl_factor_out_hor/2,
				  &pScalerInfo->scaler_chroma_init_phase_int,
				  &pScalerInfo->scaler_chroma_init_phase_rmd);

				pScalerInfo->scaler_tap = tap_luma_ver;
				pScalerInfo->scaler_y_ver_tap = tap_luma_ver;
				pScalerInfo->scaler_uv_ver_tap = tap_chrome_ver;

				cur_width = pScalerInfo->scaler_out_width;
				cur_height = pScalerInfo->scaler_out_height;
			}
			pYuvPathInfo->trim1_info.trim_start_x = 0;
			pYuvPathInfo->trim1_info.trim_start_y = 0;
			pYuvPathInfo->trim1_info.trim_size_x = cur_width;
			pYuvPathInfo->trim1_info.trim_size_y = cur_height;

			pDCAMInfo->DualInfo[path_id].offset[0] =
				pDCAMInfo->DualInfo[path_id].offset[1] = 0;
			pDCAMInfo->DualInfo[path_id].stride[0] =
				pDCAMInfo->DualInfo[path_id].stride[1] =
				pYuvPathInfo->trim1_info.trim_size_x;

			if (pDCAMInfo->cowork_mode == 0) {
				memcpy(&pDCAMInfo->InputInfo[0][path_id],
				       pYuvPathInfo,
				       sizeof(struct YUV_PATH_INFO_tag));
			} else {
				short s[2];
				unsigned short overlap[2], x0[2], x1[2];
				unsigned short trim0_start_x, trim0_end_x;
				unsigned short trim0_size_x, trim1_size_x;
				unsigned short overlap_uv, src_size_x_uv;
				unsigned short scl_factor_in_uv;
				unsigned short scl_factor_out_uv;
				int initial_phase_uv, last_phase_uv;

				ASSERT(pYuvPathInfo->src_size_x%4 == 0);

				pYuvPathInfo =
					&pDCAMInfo->dcam_yuv_path[path_id];
				memcpy(&pDCAMInfo->InputInfo[0][path_id],
				       pYuvPathInfo,
				       sizeof(struct YUV_PATH_INFO_tag));
				memcpy(&pDCAMInfo->InputInfo[1][path_id],
				       pYuvPathInfo,
				       sizeof(struct YUV_PATH_INFO_tag));

				overlap_uv = pDCAMInfo->cowork_overlap/2;
				src_size_x_uv = pYuvPathInfo->src_size_x/2;

				scl_factor_in_uv =
					pYuvPathInfo->
					scaler_info.scaler_factor_in_hor/2;
				scl_factor_out_uv =
					pYuvPathInfo->
					scaler_info.scaler_factor_out_hor/2;
				initial_phase_uv =
					pYuvPathInfo->
					scaler_info.scaler_initial_phase/2;
				last_phase_uv =
					initial_phase_uv +
					scl_factor_in_uv*
					(pYuvPathInfo->
					 scaler_info.scaler_out_width/2 - 1);

				overlap[0] = overlap_uv;
				overlap[1] = overlap_uv;

				trim0_size_x =
					pYuvPathInfo->trim0_info.trim_size_x/2;
				trim0_start_x =
					pYuvPathInfo->trim0_info.trim_start_x/2;
				trim0_end_x =
					trim0_start_x + trim0_size_x;
				trim1_size_x =
					pYuvPathInfo->trim1_info.trim_size_x/2;

				x0[0] = trim0_start_x;
				x1[0] = src_size_x_uv/2 + overlap[0];

				x0[1] = src_size_x_uv/2 - overlap[1];
				x1[1] = trim0_end_x;

				if (x1[0] >= trim0_end_x) {
					x1[0] = trim0_end_x;
					pDCAMInfo->
					InputInfo[1][path_id].path_en = 0;
				} else if (x0[1] <= trim0_start_x) {
					x0[1] = trim0_start_x;
					pDCAMInfo->
					InputInfo[0][path_id].path_en = 0;
				}

				if (pDCAMInfo->InputInfo[0][path_id].path_en) {
					s[0] = (x1[0] - x0[0])/
						pYuvPathInfo->deci_info.deci_x*
						pYuvPathInfo->deci_info.deci_x;
					x1[0] = x0[0] + s[0];

					pDCAMInfo->
					InputInfo[0][path_id].src_size_x =
						pYuvPathInfo->src_size_x/2 +
						overlap[0]*2;
					pDCAMInfo->
					InputInfo[0][path_id].
					trim0_info.trim_start_x = x0[0]*2;
					pDCAMInfo->
					InputInfo[0][path_id].
					trim0_info.trim_size_x = s[0]*2;

					ASSERT(pDCAMInfo->
					       InputInfo[0][path_id].
					       trim0_info.trim_start_x/
					       (pDCAMInfo->
						InputInfo[0][path_id].
						deci_info.deci_x*2)*
					       (pDCAMInfo->
						InputInfo[0][path_id].
						deci_info.deci_x*2) ==
					       pDCAMInfo->
					       InputInfo[0][path_id].
					       trim0_info.trim_start_x);
					ASSERT(pDCAMInfo->
					       InputInfo[0][path_id].
					       trim0_info.trim_start_y/
					       (pDCAMInfo->
						InputInfo[0][path_id].
						deci_info.deci_y)*
					       (pDCAMInfo->
						InputInfo[0][path_id].
						deci_info.deci_y) ==
					       pDCAMInfo->
					       InputInfo[0][path_id].
					       trim0_info.trim_start_y);

					s[0] /= pYuvPathInfo->deci_info.deci_x;
					if (pYuvPathInfo->
					    scaler_info.scaler_en) {
						int initial_phase, last_phase;

						pDCAMInfo->
						InputInfo[0][path_id].
						scaler_info.scaler_in_width =
						s[0]*2;

						initial_phase =
							initial_phase_uv;
						if (pDCAMInfo->
						InputInfo[1][path_id].
							path_en) {
							last_phase =
							MIN((s[0]-2)*
							scl_factor_out_uv - 1,
							last_phase_uv);
						} else {
							last_phase =
								last_phase_uv;
						}
						s[0] = (unsigned short)(
							(last_phase -
							initial_phase)/
							scl_factor_in_uv + 1);

						pDCAMInfo->
						InputInfo[0][path_id].
						scaler_info.scaler_out_width =
						s[0]*2;
					}
				} else {
					s[0] = 0;
					pDCAMInfo->
					InputInfo[0][path_id].src_size_x = 0;
					pDCAMInfo->
					InputInfo[0][path_id].src_size_y = 0;
					memset(&pDCAMInfo->
					       InputInfo[0][path_id].trim0_info,
					       0,
					       sizeof(struct Trim_tag));
					memset(&pDCAMInfo->
					       InputInfo[0][path_id].trim1_info,
					       0,
					       sizeof(struct Trim_tag));
				}

				if (pDCAMInfo->InputInfo[1][path_id].path_en) {
					s[1] = (x1[1] - x0[1])/
						pYuvPathInfo->
						deci_info.deci_x*
						pYuvPathInfo->deci_info.deci_x;
					x0[1] = x1[1] - s[1];

					pDCAMInfo->
					InputInfo[1][path_id].src_size_x =
					pYuvPathInfo->src_size_x/2 +
					overlap[1]*2;
					pDCAMInfo->
					InputInfo[1][path_id].
					trim0_info.trim_start_x =
					x0[1]*2 -
					(pYuvPathInfo->src_size_x/2 -
					 overlap[1]*2);
					pDCAMInfo->
					InputInfo[1][path_id].
					trim0_info.trim_size_x = s[1]*2;

					ASSERT(pDCAMInfo->
					       InputInfo[1][path_id].
					       trim0_info.trim_start_x/
					       (pDCAMInfo->
						InputInfo[1][path_id].
						deci_info.deci_x*2)*
					       (pDCAMInfo->
						InputInfo[1][path_id].
						deci_info.deci_x*2) ==
					       pDCAMInfo->
					       InputInfo[1][path_id].
					       trim0_info.trim_start_x);
					ASSERT(pDCAMInfo->
					       InputInfo[1][path_id].
					       trim0_info.trim_start_y/
					       (pDCAMInfo->
						InputInfo[1][path_id].
						deci_info.deci_y)*
					       (pDCAMInfo->
						InputInfo[1][path_id].
						deci_info.deci_y) ==
					       pDCAMInfo->
					       InputInfo[1][path_id].
					       trim0_info.trim_start_y);

					s[1] /= pYuvPathInfo->deci_info.deci_x;
					if (pYuvPathInfo->
					    scaler_info.scaler_en) {
						int initial_phase, last_phase;

						pDCAMInfo->
						InputInfo[1][path_id].
						scaler_info.scaler_in_width =
						s[1]*2;

					last_phase = last_phase_uv -
						(trim0_size_x/
						pYuvPathInfo->deci_info.deci_x -
						s[1])*scl_factor_out_uv;
					ASSERT(last_phase <
					       scl_factor_out_uv*
					       pYuvPathInfo->
					       scaler_info.scaler_in_width/2);
					if (pDCAMInfo->
					    InputInfo[0][path_id].path_en)
						initial_phase =
						MAX(4*scl_factor_out_uv,
						initial_phase_uv);
					else
						initial_phase =
							initial_phase_uv;
					s[1] = (unsigned short)(
						(last_phase - initial_phase)
						/scl_factor_in_uv +
						1);

					pDCAMInfo->
						InputInfo[1][path_id].
						scaler_info.scaler_out_width =
						s[1]*2;

					initial_phase =
						last_phase -
						(s[1] - 1)*scl_factor_in_uv;
					pDCAMInfo->
					InputInfo[1][path_id].
					scaler_info.scaler_initial_phase =
					initial_phase*4;
					calc_scaler_phase(initial_phase*4,
					scl_factor_out_uv*2,
					&pDCAMInfo->InputInfo[1][path_id].
					scaler_info.scaler_init_phase_int,
					&pDCAMInfo->
					InputInfo[1][path_id].
					scaler_info.scaler_init_phase_rmd);
					calc_scaler_phase(initial_phase,
					scl_factor_out_uv,
					&pDCAMInfo->
					InputInfo[1][path_id].
					scaler_info.
					scaler_chroma_init_phase_int,
					&pDCAMInfo->InputInfo[1][path_id].
					scaler_info.
					scaler_chroma_init_phase_rmd);
					}
				} else {
					s[1] = 0;
					pDCAMInfo->
						InputInfo[1][path_id].
						src_size_x = 0;
					pDCAMInfo->
						InputInfo[1][path_id].
						src_size_y = 0;
					memset(&pDCAMInfo->
					       InputInfo[1][path_id].trim0_info,
					       0,
					       sizeof(struct Trim_tag));
					memset(&pDCAMInfo->
					       InputInfo[1][path_id].trim1_info,
					       0,
					       sizeof(struct Trim_tag));
				}
				ASSERT(s[0] + s[1] >= trim1_size_x);

				if (pYuvPathInfo->outdata_format == PLANAR)
					PIX_ALIGN = 16;
				else
					PIX_ALIGN = 8;

				if (pDCAMInfo->
				    InputInfo[0][path_id].path_en == 1 &&
				    pDCAMInfo->
				    InputInfo[1][path_id].path_en == 1) {
					switch (pYuvPathInfo->
						rotation_info.rot_dir) {
						case ROTATE0:
						case FLIP:
							pDCAMInfo->
							InputInfo[0][path_id].
							trim1_info.trim_size_x =
							(s[0]*2/PIX_ALIGN)*
							PIX_ALIGN;
							pDCAMInfo->
							InputInfo[1][path_id].
							trim1_info.trim_size_x =
							pYuvPathInfo->
							trim1_info.trim_size_x -
							pDCAMInfo->
							InputInfo[0][path_id].
							trim1_info.trim_size_x;
							pDCAMInfo->
							DualInfo[path_id].
							offset[0] = 0;
							pDCAMInfo->
							DualInfo[path_id].
							offset[1] =
							pDCAMInfo->
							InputInfo[0][path_id].
							trim1_info.trim_size_x;
							break;
						case ROTATE180:
						case MIRROR:
							pDCAMInfo->
							InputInfo[1][path_id].
							trim1_info.trim_size_x =
							(s[1]*2/PIX_ALIGN)*
							PIX_ALIGN;
							pDCAMInfo->
							InputInfo[0][path_id].
							trim1_info.trim_size_x =
							pYuvPathInfo->
							trim1_info.trim_size_x -
							pDCAMInfo->
							InputInfo[1][path_id].
							trim1_info.trim_size_x;
							pDCAMInfo->
							DualInfo[path_id].
							offset[0] =
							pDCAMInfo->
							InputInfo[1][path_id].
							trim1_info.trim_size_x;
							break;
						default:
							break;
					}
					pDCAMInfo->
						InputInfo[0][path_id].
						trim1_info.trim_start_x = 0;
					pDCAMInfo->
						InputInfo[1][path_id].
						trim1_info.trim_start_x =
						s[1]*2 -
						pDCAMInfo->
						InputInfo[1][path_id].
						trim1_info.trim_size_x;
					ASSERT(pDCAMInfo->
					       InputInfo[0][path_id].
					       scaler_info.scaler_out_width >=
					       pDCAMInfo->
					       InputInfo[0][path_id].
					       trim1_info.trim_size_x);
					ASSERT(pDCAMInfo->
					       InputInfo[1][path_id].
					       scaler_info.scaler_out_width >=
					       pDCAMInfo->
					       InputInfo[1][path_id].
					       trim1_info.trim_size_x);
				}
			}
		}
	}
}
