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

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include "gsp_debug.h"
#include "gsp_layer.h"
#include "gsp_sync.h"

#define GSP_FENCE_WAIT_TIMEOUT 3000/* ms */

void gsp_sync_fence_signal(struct gsp_fence_data *data)
{

}

#if 0
static int gsp_sync_sig_fence_create(
				struct dma_fence **sig_fen,
				unsigned int record)
{
	int err;
	struct sync_pt *pt;
	struct dma_fence *fence;

	pt = gsp_sync_pt_create(obj, record);
	if (pt == NULL) {
		GSP_ERR("pt create failed\n");
		err = -ENOMEM;
		goto err;
	}

	fence = dma_fence_create("gsp", pt);

	if (fence == NULL) {
		GSP_ERR("fence create failed\n");
		sync_pt_free(pt);
		err = -ENOMEM;
		goto err;
	}

	/*store the new fence with the sig_fen pointer */
	*sig_fen = fence;

	return 0;

err:
	return err;
}
#endif

int gsp_sync_sig_fd_copy_to_user(struct dma_fence *fence,
				 int32_t __user *ufd)
{
	return 0;
#if 0
	int fd = get_unused_fd_flags(0);

	if (fd < 0) {
		GSP_ERR("fd overflow, fd: %d\n", fd);
		return fd;
	}

	if (put_user(fd, ufd)) {
		GSP_ERR("signal fence fd copy to user failed\n");
		dma_fence_put(fence);
		goto err;
	}
	dma_fence_install(fence, fd);

	GSP_DEBUG("copy signal fd: %d to ufd: %p success\n", fd, ufd);
	return 0;

err:
	put_unused_fd(fd);
	return -1;
#endif
}

int gsp_sync_wait_fence_collect(struct dma_fence **wait_fen_arr,
				uint32_t *count, int fd)
{
		return 0;
#if 0
	struct dma_fence *fence = NULL;
	int ret = -1;

	if (fd < 0) {
		GSP_DEBUG("wait fd < 0: indicate no need to wait\n");
		return 0;
	}

	if (*count >= GSP_WAIT_FENCE_MAX) {
		GSP_ERR("wait fence overflow, cnt: %d\n", *count);
		return ret;
	}

	fence = dma_fence_fdget(fd);
	if (fence != NULL) {
		/*store the wait fence at the wait_fen array */
		wait_fen_arr[*count] = fence;
		/*update the count of sig_fen */
		(*count)++;
		ret = 0;
	} else
		GSP_ERR("wait fence get from fd: %d error\n", fd);

	return ret;
#endif
}

int gsp_sync_fence_process(struct gsp_layer *layer,
			   struct gsp_fence_data *data,
			   bool last)
{
	return 0;
#if 0
	int ret = 0;
	int wait_fd = -1;
	int share_fd = -1;
	enum gsp_layer_type type = GSP_INVAL_LAYER;

	if (IS_ERR_OR_NULL(layer)
	    || IS_ERR_OR_NULL(data)) {
		GSP_ERR("layer[%d] fence process params error\n",
			gsp_layer_to_type(layer));
		return -1;
	}

	wait_fd = gsp_layer_to_wait_fd(layer);
	type = gsp_layer_to_type(layer);
	share_fd = gsp_layer_to_share_fd(layer);

	/* first collect wait fence */
	if (layer->enable == 0 || wait_fd < 0) {
		GSP_DEBUG("layer[%d] no need to collect wait fence\n",
			  gsp_layer_to_type(layer));
	} else {
		ret = gsp_sync_wait_fence_collect(data->wait_fen_arr,
						  &data->wait_cnt, wait_fd);
		if (ret < 0) {
			GSP_ERR("collect layer[%d] wait fence failed\n",
				gsp_layer_to_type(layer));
			return ret;
		}
	}


	if (type != GSP_DES_LAYER) {
		GSP_DEBUG("no need to create sig fd for img and osd layer\n");
		return ret;
	}

	ret = gsp_sync_sig_fence_create(&data->sig_fen,
					data->record);
	if (ret < 0) {
		GSP_ERR("create signal fence failed\n");
		return ret;
	}

	if (last != true) {
		GSP_DEBUG("no need to copy signal fd to user\n");
		return ret;
	}

	ret = gsp_sync_sig_fd_copy_to_user(data->sig_fen, data->ufd);
	if (ret < 0) {
		GSP_ERR("copy signal fd to user failed\n");
		return ret;
	}

	return ret;
#endif
}

void gsp_sync_fence_data_setup(struct gsp_fence_data *data,
			       int __user *ufd)
{
#if 0
	if (IS_ERR_OR_NULL(data)) {
		GSP_ERR("sync fence data set up params error\n");
		return;
	}

	data->ufd = ufd;
#endif
}

void gsp_sync_fence_free(struct gsp_fence_data *data)
{
#if 0
	int i = 0;

	/* free acuqire fence array */
	for (i = 0; i < data->wait_cnt; i++) {
		if (!data->wait_fen_arr[i]) {
			GSP_WARN("free null acquire fen\n");
			continue;
		}

		dma_fence_put(data->wait_fen_arr[i]);
		data->wait_fen_arr[i] = NULL;
	}
	data->wait_cnt = 0;

	/* signal release fence */
	if (data->sig_fen)
		gsp_dma_fence_signal(data->tl);
#endif
}

int gsp_sync_fence_wait(struct gsp_fence_data *data)
{
#if 0
	int ret = 0;
	int i = 0;

	/* wait acuqire fence array */
	for (i = 0; i < data->wait_cnt; i++) {
		if (!data->wait_fen_arr[i]) {
			GSP_WARN("wait null acquire fen\n");
			continue;
		}

		ret = dma_fence_wait(data->wait_fen_arr[i],
				      GSP_FENCE_WAIT_TIMEOUT);
		if (ret) {
			GSP_ERR("wait %d/%d fence failed\n",
				i + 1, data->wait_cnt);
			return ret;
		}
		dma_fence_put(data->wait_fen_arr[i]);
		data->wait_fen_arr[i] = NULL;
	}
	data->wait_cnt = 0;

	return ret;
#endif
	return 0;
}
