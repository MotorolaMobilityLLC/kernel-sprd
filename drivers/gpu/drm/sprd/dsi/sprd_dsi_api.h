#ifndef _MIPI_DSI_API_H_
#define _MIPI_DSI_API_H_

#include "sprd_dsi.h"

int sprd_dsi_init(struct sprd_dsi *dsi);
int sprd_dsi_uninit(struct sprd_dsi *dsi);
int sprd_dsi_dpi_video(struct sprd_dsi *dsi,
			struct dpi_video_param *param);
int sprd_dsi_edpi_video(struct sprd_dsi *dsi,
			struct edpi_video_param *param);
int sprd_dsi_htime_update(struct sprd_dsi *dsi,
			struct dpi_video_param *param);
int sprd_dsi_wr_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			const u8 *param, u16 len);
int sprd_dsi_rd_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			u8 msb_byte, u8 lsb_byte,
			u8 *buffer, u8 bytes_to_read);
//int sprd_dsi_gen_write(struct sprd_dsi *dsi, u8 *param, u16 len);
//int sprd_dsi_gen_read(struct sprd_dsi *dsi, u8 *param, u16 len,
//						u8 *buf, u8 count);
//int sprd_dsi_dcs_write2(struct sprd_dsi *dsi, u8 *param, u16 len);
//int sprd_dsi_dcs_read2(struct sprd_dsi *dsi, u8 param, u8 *buf, u8 count);
//int sprd_dsi_force_write(struct sprd_dsi *dsi, u8 type, u8 *param, u16 len);
//int sprd_dsi_set_max_return_size(struct sprd_dsi *dsi, u16 size);
void sprd_dsi_set_work_mode(struct sprd_dsi *dsi, u8 mode);
int sprd_dsi_get_work_mode(struct sprd_dsi *dsi);
void sprd_dsi_lp_cmd_enable(struct sprd_dsi *dsi, int enable);
void sprd_dsi_state_reset(struct sprd_dsi *dsi);
u32 sprd_dsi_int_status(struct sprd_dsi *dsi, int index);
void sprd_dsi_int_mask(struct sprd_dsi *dsi, int index);

#endif /* _MIPI_DSI_API_H_ */
