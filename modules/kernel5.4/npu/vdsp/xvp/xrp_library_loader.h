/*
* SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef XRP_LIBRARY_LOADER_H
#define XRP_LIBRARY_LOADER_H
#include <linux/mutex.h>
#include <linux/fs.h>

#define LIST_NO_MEM  -2
#define LIST_ERROR   -1
#define LIST_SUCCESS  0

#define LIB_RESULT_OK         0
#define LIB_RESULT_LOADED     1
#define LIB_RESULT_ERROR      2

enum load_unload_flag {
	XRP_NOT_LOAD_UNLOAD = 0,
	XRP_LOAD_LIB_FLAG,
	XRP_UNLOAD_LIB_FLAG,
	XRP_LOAD_LIB_FLAG_MAX,
};
enum library_state {
	XRP_LIBRARY_LOADING,
	XRP_LIBRARY_UNLOADING,
	XRP_LIBRARY_LOADED,
	XRP_LIBRARY_PROCESSING_CMD,
	XRP_LIBRARY_IDLE,
	XRP_LIBRARY_MISSED_STATE,
};

struct xrp_unload_cmdinfo {
	struct xrp_request *rq;
	int input_mem_fd;
	void *input_kaddr;
};

struct loadlib_info {
	enum library_state lib_state;	/*valid_flag 0 is invalid may because vdsp side error , 1 is valid */
	uint32_t lib_processing_count;
	char libname[32];	/*libname , load_flag ==1 is valid, load_flag ==0 is invalid */
	uint32_t length;	/*library length bytes */
	int handle;		/*mem fd */
	int pil_handle;
	phys_addr_t ion_phy;
	void *ion_kaddr;
	uint32_t pil_info;
	struct mutex mutex;	/*cmd mutex */
	uint32_t load_count;	/*load count when 0 it may unload */
	struct list_head node_libinfo;
	uint32_t original_flag;	/*original flag , 1 is first allocated info ,need release, 0 is duplictate one */
};

struct xrp_load_lib_info {
	struct mutex libload_mutex;
};

struct xrp_request;
struct xvp;

enum load_unload_flag xrp_check_load_unload(struct xvp *xvp, struct xrp_request *rq, uint32_t krqflag);
int32_t xrp_pre_process_request(struct file *filp, struct xrp_request *rq,
	enum load_unload_flag loadflag, char *libname, uint32_t krqflag);
int post_process_request(struct file *filp, struct xrp_request *rq,
	const char *libname, enum load_unload_flag load_flag, int32_t resultflag);
int32_t xrp_library_release_all(struct xvp *xvp);
int32_t xrp_library_decrease(struct file *filp, const char *libname);
//int32_t xrp_library_setall_missstate(struct xvp *xvp); no use
int32_t xrp_create_unload_cmd(struct file *filp, struct loadlib_info *libinfo,
	struct xrp_unload_cmdinfo *info);
int32_t xrp_free_unload_cmd(struct file *filp, struct xrp_unload_cmdinfo *info);

#endif
