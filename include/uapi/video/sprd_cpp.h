/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _SPRD_CPP_H_
#define _SPRD_CPP_H_

/* Structure Definitions */

enum {
	ROT_YUV422 = 0,
	ROT_YUV420,
	ROT_YUV400,
	ROT_RGB888,
	ROT_RGB666,
	ROT_RGB565,
	ROT_RGB555,
	ROT_FMT_MAX
};

enum {
	ROT_90 = 0,
	ROT_270,
	ROT_180,
	ROT_MIRROR,
	ROT_ANGLE_MAX
};

enum {
	ROT_ENDIAN_BIG = 0,
	ROT_ENDIAN_LITTLE,
	ROT_ENDIAN_HALFBIG,
	ROT_ENDIAN_HALFLITTLE,
	ROT_ENDIAN_MAX
};

enum {
	SCALE_YUV420 = 0,
	SCALE_YUV420_3FRAME,
	SCALE_YUV422,
	SCALE_JPEG_LS,
	SCALE_YUV400,
	SCALE_RGB565,
	SCALE_RGB888,
	SCALE_FTM_MAX
};

enum {
	SCALE_ENDIAN_BIG = 0,
	SCALE_ENDIAN_LITTLE,
	SCALE_ENDIAN_HALFBIG,
	SCALE_ENDIAN_HALFLITTLE,
	SCALE_ENDIAN_MAX
};

enum {
	SCALE_MODE_NORMAL = 0,
	SCALE_MODE_SLICE,
	SCALE_MODE_SLICE_READDR,
	SCALE_MODE_MAX
};

enum {
	SCALE_REGULATE_MODE_NORMAL = 0,
	SCALE_REGULATE_MODE_SHRINK,
	SCALE_REGULATE_MODE_CUT,
	SCALE_REGULATE_MODE_SPECIAL_EFFECT,
	SCALE_REGULATE_MODE_MAX
};

enum {
	PATH2_ENDIAN_BIG = 0,
	PATH2_ENDIAN_LITTLE,
	PATH2_ENDIAN_HALFBIG,
	PATH2_ENDIAN_HALFLITTLE,
	PATH2_ENDIAN_MAX
};

enum {
	PATH3_ENDIAN_BIG = 0,
	PATH3_ENDIAN_LITTLE,
	PATH3_ENDIAN_HALFBIG,
	PATH3_ENDIAN_HALFLITTLE,
	PATH3_ENDIAN_MAX
};


struct sprd_cpp_size {
	unsigned int w;
	unsigned int h;
};

struct sprd_cpp_rect {
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
};

struct sprd_cpp_addr {
	unsigned int y;
	unsigned int u;
	unsigned int v;
	unsigned int mfd[3];
};

struct sprd_cpp_rot_cfg_parm {
	struct sprd_cpp_size size;
	unsigned int format;
	unsigned int angle;
	struct sprd_cpp_addr src_addr;
	struct sprd_cpp_addr dst_addr;
	unsigned int src_endian;
	unsigned int dst_endian;
};

/* scaling */
struct sprd_cpp_scale_jpegls_info {
	unsigned int y;
	unsigned int u;
	unsigned int v;
};

struct sprd_cpp_scale_deci {
	unsigned int hor;
	unsigned int ver;
};

struct sprd_cpp_scale_regulate_threshold {
	unsigned int effect_threshold_y;
	unsigned int effect_threshold_uv;
	unsigned int down_threshold_y;
	unsigned int down_threshold_uv;
	unsigned int up_threshold_y;
	unsigned int up_threshold_uv;
};

struct sprd_cpp_scale_endian_sel {
	unsigned char y_endian;
	unsigned char uv_endian;
	unsigned char reserved[2];
};

struct sprd_cpp_scale_slice_parm {
	unsigned int slice_height;
	struct sprd_cpp_rect input_rect;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_addr output_addr;
};

struct sprd_cpp_scale_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_addr input_addr_vir;
	struct sprd_cpp_scale_endian_sel input_endian;

	struct sprd_cpp_size output_size;
	unsigned int output_format;
	struct sprd_cpp_addr output_addr;
	struct sprd_cpp_addr output_addr_vir;
	struct sprd_cpp_scale_endian_sel output_endian;

	unsigned int scale_mode;
	unsigned int slice_height;

	struct sprd_cpp_scale_jpegls_info jpegls_info;
	struct sprd_cpp_scale_regulate_threshold  regulate_threshold;
	unsigned int regualte_mode;
	struct sprd_cpp_scale_deci scale_deci;
	struct sprd_cpp_addr input_r_block_addr;
	unsigned int split_left_block_w;
	unsigned int regualte_mode_src;
	unsigned int regualte_mode_des;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_dma_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_scale_endian_sel input_endian;

	unsigned int total_num;

	struct sprd_cpp_size output_size;
	unsigned int output_format;
	struct sprd_cpp_addr output_addr;
	struct sprd_cpp_scale_endian_sel output_endian;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_path2_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_scale_endian_sel input_endian;

	struct sprd_cpp_size output_size;
	unsigned int output_format;
	struct sprd_cpp_addr output_addr;
	struct sprd_cpp_scale_endian_sel output_endian;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_path3_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_scale_endian_sel input_endian;

	struct sprd_cpp_size output_size;
	unsigned int output_format;
	struct sprd_cpp_addr output_addr;
	struct sprd_cpp_scale_endian_sel output_endian;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_path0_path3_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	struct sprd_cpp_rect path3_input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_scale_endian_sel input_endian;

	struct sprd_cpp_size path0_output_size;
	unsigned int path0_output_format;
	struct sprd_cpp_addr path0_output_addr;
	struct sprd_cpp_scale_endian_sel path0_output_endian;

	struct sprd_cpp_size path3_output_size;
	unsigned int path3_output_format;
	struct sprd_cpp_addr path3_output_addr;
	struct sprd_cpp_scale_endian_sel path3_output_endian;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_path2_path3_cfg_parm {
	struct sprd_cpp_size input_size;
	struct sprd_cpp_rect input_rect;
	struct sprd_cpp_rect path3_input_rect;
	unsigned int input_format;
	struct sprd_cpp_addr input_addr;
	struct sprd_cpp_scale_endian_sel input_endian;

	struct sprd_cpp_size path2_output_size;
	unsigned int path2_output_format;
	struct sprd_cpp_addr path2_output_addr;
	struct sprd_cpp_scale_endian_sel path2_output_endian;

	struct sprd_cpp_size path3_output_size;
	unsigned int path3_output_format;
	struct sprd_cpp_addr path3_output_addr;
	struct sprd_cpp_scale_endian_sel path3_output_endian;
	unsigned int isDirectVirAddr;
};

struct sprd_cpp_scale_capability  {
	struct sprd_cpp_size src_size;
	int src_format;
	struct sprd_cpp_size dst_size;
	int dst_format;
	int is_supported;
};

enum sprd_cpp_current_senario {
	CPP_SENARIO_SCALE = (1<<0),
	CPP_SENARIO_ROTATE = (1<<1),
	CPP_SENARIO_PATH2 = (1<<2),
	CPP_SENARIO_PATH3 = (1<<3),
	CPP_SENARIO_PATH0_PATH3 = (1<<4),
	CPP_SENARIO_PATH2_PATH3 = (1<<5)
};

/*struct sprd_cpp_wait_buffer_parm {*/
    /*enum sprd_cpp_current_senario cur_senario;*/
    /*bool nonblocking;*/
/*};*/

/* structure shared between cpp and dcam for*/
/*kernel call from dcam to cpp.*/
struct sprd_dcam_img_frm {
	unsigned int                                fmt;
	unsigned int                                buf_size;
	struct sprd_cpp_rect                 rect;
	struct sprd_cpp_rect                 rect2;
	struct sprd_cpp_size                 size;
	struct sprd_cpp_addr                addr_phy;
	struct sprd_cpp_addr                addr_vir;
	unsigned int                                fd;
	struct sprd_cpp_scale_endian_sel    data_end;
	unsigned int                                format_pattern;
	void                                             *reserved;
};


#define SPRD_CPP_IOCTL_MAGIC           'm'
#define SPRD_CPP_IO_OPEN_ROT \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 1, unsigned int)
#define SPRD_CPP_IO_CLOSE_ROT \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 2, unsigned int)
#define SPRD_CPP_IO_START_ROT \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 3, struct sprd_cpp_rot_cfg_parm)

#define SPRD_CPP_IO_OPEN_SCALE \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 4, unsigned int)
#define SPRD_CPP_IO_CLOSE_SCALE \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 5, unsigned int)
#define SPRD_CPP_IO_START_SCALE \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 6, struct sprd_cpp_scale_cfg_parm)
#define SPRD_CPP_IO_CONTINUE_SCALE \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 7, struct sprd_cpp_scale_slice_parm)
#define SPRD_CPP_IO_STOP_SCALE \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 8, unsigned int)

/*#define SPRD_CPP_IO_OPEN_PREVIEW \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 9, unsigned int)*/
/*#define SPRD_CPP_IO_CLOSE_PREVIEW \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 10, unsigned int)*/
#define SPRD_CPP_IO_START_PATH2 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 11, struct sprd_cpp_path2_cfg_parm)
#define SPRD_CPP_IO_STOP_PATH2 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 12, unsigned int)

/*#define SPRD_CPP_IO_OPEN_PLAYBACK \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 13, unsigned int)*/
/*#define SPRD_CPP_IO_CLOSE_PLAYBACK \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 14, unsigned int)*/
#define SPRD_CPP_IO_START_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 15, struct sprd_cpp_path3_cfg_parm)
#define SPRD_CPP_IO_STOP_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 16, unsigned int)

/*#define SPRD_CPP_IO_OPEN_PATH0_PATH3 \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 17, unsigned int)*/
/*#define SPRD_CPP_IO_CLOSE_PATH0_PATH3 \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 18, unsigned int)*/
#define SPRD_CPP_IO_START_PATH0_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 19, struct sprd_cpp_path0_path3_cfg_parm)
#define SPRD_CPP_IO_STOP_PATH0_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 20, unsigned int)

/*#define SPRD_CPP_IO_OPEN_PATH2_PATH3 \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 21, unsigned int)*/
/*#define SPRD_CPP_IO_CLOSE_PATH2_PATH3 \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 22, unsigned int)*/
#define SPRD_CPP_IO_START_PATH2_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 23, struct sprd_cpp_path2_path3_cfg_parm)
#define SPRD_CPP_IO_STOP_PATH2_PATH3 \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 24, unsigned int)

/*#define SPRD_CPP_IO_WAIT_BUFFER_DONE \*/
	/*_IOW(SPRD_CPP_IOCTL_MAGIC, 25, struct sprd_cpp_wait_buffer_parm)*/

#define SPRD_CPP_IO_OPEN_PATH \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 26, unsigned int)

#define SPRD_CPP_IO_CLOSE_PATH \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 27, unsigned int)

#define SPRD_CPP_IO_OPEN_DMA \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 28, unsigned int)
#define SPRD_CPP_IO_START_DMA \
	_IOW(SPRD_CPP_IOCTL_MAGIC, 29, struct sprd_cpp_dma_cfg_parm)

#define SPRD_CPP_IO_SCALE_CAPABILITY \
	_IOWR(SPRD_CPP_IOCTL_MAGIC, 30, struct sprd_cpp_scale_capability)

#endif
