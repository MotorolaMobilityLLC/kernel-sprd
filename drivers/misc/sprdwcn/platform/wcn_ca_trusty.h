/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef _WCN_CA_TRUSTY_H
#define _WCN_CA_TRUSTY_H

struct sys_img_header {
	uint32_t  mMagicNum;        // "BTHD"="0x42544844"="boothead"
	uint32_t  mVersion;         // 1
	uint8_t   mPayloadHash[32]; // sha256 hash value
	uint64_t  mImgAddr;         // image loaded address
	uint32_t  mImgSize;         // image size
	uint32_t  is_packed;        // packed image, 0:false 1:true
	uint32_t  mFirmwareSize;    // runtime firmware size
	uint32_t  ImgRealSize;      //image real size
	uint8_t   reserved[448];    // 448 + 16*4 = 512
};

#define SEC_IMAGE_MAGIC 0x42544844
#define SEC_IMAGE_HDR_SIZE sizeof(struct sys_img_header)
#define SEC_IMAGE_TAIL_MAX_SIZE 2048

enum secureboot_command {
	KERNELBOOTCP_BIT                = 1,
	KERNELBOOTCP_REQ_SHIFT          = 1,

	KERNEL_BOOTCP_VERIFY_ALL        = (0 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_UNLOCK_DDR        = (1 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_VDSP       = (2 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_WCN       = (3 << KERNELBOOTCP_REQ_SHIFT),
	KERNEL_BOOTCP_VERIFY_GPS       = (4 << KERNELBOOTCP_REQ_SHIFT),
};

/* Size of the footer.                 */
/* original definition in avb_footer.h */
#define AVB_FOOTER_SIZE    64

/* Size of  partition name .          */
#define PART_NAME_SIZE      32

struct KBC_IMAGE_S {
	uint64_t img_addr;  // the base address of image to verify
	uint32_t img_len;   // length of image
	uint32_t map_len;   // mapping length
	#ifdef CONFIG_VBOOT_V2
	uint8_t  footer[AVB_FOOTER_SIZE];
	uint8_t  partition[PART_NAME_SIZE];
	#endif
};

struct KBC_LOAD_TABLE_W {
	struct KBC_IMAGE_S wcn_fw;
	uint16_t    flag;      // not use
	uint16_t    is_packed; // is packed image
};

struct KBC_LOAD_TABLE_G {
	struct KBC_IMAGE_S gps_fw;
	uint16_t    flag;      // not use
	uint16_t    is_packed; // is packed image
};

/**
 * kernelbootcp_message - Serial header for communicating with KBC server
 * @cmd: the command, one of kernelbootcp_command.
 * @payload: start of the serialized command specific payload
 */
struct kernelbootcp_message {
	uint32_t cmd;
	uint8_t  payload[0];
};

struct wcn_ca_tipc_ctx {
	int state;
	struct mutex lock;
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct list_head rx_msg_queue;
};

struct firmware_verify_ctrl {
	uint32_t wcn_or_gnss_bin; // 1 for wcn, 2 for gnss.
	const char *tipc_chan_name;
	phys_addr_t bin_base_addr;
	uint32_t bin_length;
	struct wcn_ca_tipc_ctx *ca_tipc_ctx;
};

int wcn_firmware_sec_verify(uint32_t wcn_or_gnss_bin,
		phys_addr_t bin_base_addr, uint32_t bin_length);

#endif
