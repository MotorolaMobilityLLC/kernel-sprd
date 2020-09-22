// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/debugfs.h>
#include <asm/byteorder.h>
#include <linux/uaccess.h>
#include "dw_dptx.h"
#include "dp_api.h"

static u32 aux_addr;
static u32 dpcd_addr;
static u8 aux_type;
static u32 aux_size;
/**
 * DOC: DEBUGFS Interface
 *
 * Top level:
 *
 * max_lane_count [rw] - The maximum lane count supported. Write to
 * this to set the max lane count.
 *
 * max_rate [rw] - The maximum rate supported. Write to this to set
 * the maximum rate.
 *
 *
 * pixel_mode_sel [rw] - Pixel mode selection. Write to this to set pixel mode.
 *
 * regdump [r] - Read this to see the values of all the core
 * registers.
 *
 * Link attributes:
 *
 * link/lane_count [r] - The current lanes in use. This will be 1, 2,
 * or 4.
 *
 * link/rate [r] - The current rate. 0 - RBR, 1 - HBR, 2 - HBR2, 3 -
 * HBR3.
 *
 * link/retrain [w] - Write to this to retrain the link. The value to
 * write is the desired rate and lanes separated by a space. For
 * example to retrain the link at 4 lanes at RBR write "0 4" to this
 * file.
 *
 * link/status [r] - Shows the status of the link.
 *
 * link/trained [r] - True if the link training was successful.
 *
 * audio/inf_type [rw] - The audio interface type. 0 - I2S, 1 - SPDIF
 *
 * audio/num_ch [rw] - Number of audio channels. 1 -8
 *
 * audio/data_width [rw] - The audio input data width. 16 - 24
 *
 * video/pix_enc [rw] - The video pixel encoding
 * 0 - RGB, 1 - YCBCR420, 2 - YCBCR422, 3 - YCBCR444, 4 - YONLY, 5 - RAW
 *
 * video/bpc [rw] - The video bits per component.  6, 8, 10, 12, 16
 *
 * video/pattern [rw] - Video pattern.
 * 0 - TILE, 1 - RAMP, 2 - COLRAMP, 3 - CHESS
 *
 * video/dynamic_range [rw] - Video dynamic range. 0 - CEA, 1 - VESA
 *
 * video/colorimetry [rw] - Video colorimetry.
 * This parametr is necassary in case of pixel encoding
 * YCBCR4:2:2, YCBCR4:4:4. 1 - YCBCR4:2:2, 2 - ITU-R BT.709
 *                   *
 * video/refresh_rate [rw] - Video mode refresh rate.
 * This parametr is neccassary in case of
 * CEA video modes 8, 9, 12, 13, 23, 24, 27, 28
 *
 * video/video_format [rw] - Video format. 0 - CEA, 1 - CVT, 2 - DMT
 *
 * aux_type [rw] - Type of AUX transaction. 0 - Native AUX Native Read, 1 - I2C
 * over AUX read, 2 - AUX Native Write, 3 - I2C over AUx write.
 *
 * aux_addr [rw] - Address used for AUX transaction data.
 *
 * aux_size [rw] - Size of aux transaction in bytes.
 *
 * aux [rw] - Data for AUX transaction.
 *
 * mute [rw] - Mute the Audio. 1- mute, 0 unmute.
 *
 * sdp [rw] - SDP data payload to be transferred during blanking period.
 *
 */

static int dptx_link_status_show(struct seq_file *s, void *unused)
{
	int i;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);

	seq_printf(s, "trained = %d\n", dptx->link.trained);
	seq_printf(s, "rate = %d\n", dptx->link.rate);
	seq_printf(s, "lanes = %d\n", dptx->link.lanes);

	if (!dptx->link.trained)
		goto done;

	for (i = 0; i < dptx->link.lanes; i++) {
		seq_printf(s, "preemp and vswing level [%d] = %d, %d\n",
			   i, dptx->link.preemp_level[i],
			   dptx->link.vswing_level[i]);
	}

done:
	mutex_unlock(&dptx->mutex);
	return 0;
}

static int dptx_link_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_link_status_show, inode->i_private);
}

static const struct file_operations dptx_link_status_fops = {
	.open = dptx_link_status_open,
	.write = NULL,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_link_retrain_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	seq_printf(s, "trained = %d\n", dptx->link.trained);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_link_retrain_write(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	int retval = 0;
	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;
	u8 buf[32];
	u8 rate;
	u8 lanes;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	rate = buf[0] - '0';
	lanes = buf[1] - '0';

	retval = dptx_link_retrain(dptx, rate, lanes);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_link_retrain_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_link_retrain_show, inode->i_private);
}

static const struct file_operations dptx_link_retrain_fops = {
	.open = dptx_link_retrain_open,
	.write = dptx_link_retrain_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t dptx_aux_write(struct file *file,
			      const char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	int retval = 0;
	u8 *buf;
	struct drm_dp_aux_msg *aux_msg = NULL;
	int i = 0;
	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	buf = kzalloc(sizeof(count), GFP_KERNEL);
	if (!buf) {
		retval = -ENOMEM;
		goto err3;
	}
	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto err2;
	}

	for (i = 0; i < count; i++)
		buf[i] = buf[i] - '0';

	aux_msg = kzalloc(sizeof(*aux_msg), GFP_KERNEL);
	if (!aux_msg) {
		retval = -ENOMEM;
		goto err2;
	}

	switch (aux_type) {
	case 2:
		aux_msg->request = DP_AUX_NATIVE_WRITE;
		break;
	case 3:
		aux_msg->request = DP_AUX_I2C_WRITE;
		break;
	}
	aux_msg->address = aux_addr;
	aux_msg->buffer = buf;
	aux_msg->size = count;

	retval = dptx_aux_transfer(dptx, aux_msg);
	if (retval <= 0)
		goto err1;

	retval = count;
err1:
	kfree(aux_msg);
err2:
	kfree(buf);
err3:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static ssize_t dptx_aux_read(struct file *file,
			     char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int retval = 0;
	u8 *aux_buf = NULL;
	struct drm_dp_aux_msg *aux_msg = NULL;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	aux_buf = kmalloc(aux_size, GFP_KERNEL);

	if (!aux_buf) {
		retval = -EFAULT;
		goto err3;
	}
	aux_msg = kmalloc(sizeof(*aux_msg), GFP_KERNEL);
	if (!aux_msg) {
		retval = -EFAULT;
		goto err2;
	}

	memset(aux_buf, 0, sizeof(*aux_buf));
	memset(aux_msg, 0, sizeof(*aux_msg));

	switch (aux_type) {
	case 0:
		aux_msg->request = DP_AUX_NATIVE_READ;
		break;
	case 1:
		aux_msg->request = DP_AUX_I2C_READ;
		break;
	}
	aux_msg->address = aux_addr;
	aux_msg->buffer = aux_buf;
	aux_msg->size = aux_size;

	retval = dptx_aux_transfer(dptx, aux_msg);
	if (retval <= 0)
		goto err1;

	if (copy_to_user(ubuf, aux_msg->buffer, aux_size) != 0) {
		retval = -EFAULT;
		goto err1;
	}
	retval = count;
err1:
	kfree(aux_msg);
err2:
	kfree(aux_buf);
err3:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_aux_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int dptx_aux_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_aux_show, inode->i_private);
}

static const struct file_operations dptx_aux_fops = {
	.open = dptx_aux_open,
	.write = dptx_aux_write,
	.read = dptx_aux_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_rx_caps_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	seq_printf(s, "%x\n", dptx->rx_caps[0x21]);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static int dptx_rx_caps_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_rx_caps_show, inode->i_private);
}

static const struct file_operations dptx_rx_caps_fops = {
	.open = dptx_rx_caps_open,
	.write = NULL,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_dpcd_read_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 byte;

	mutex_lock(&dptx->mutex);
	drm_dp_dpcd_readb(&dptx->aux_dev, dpcd_addr, &byte);
	seq_printf(s, "0x%02x\n", byte);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static int dptx_dpcd_read_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_dpcd_read_show, inode->i_private);
}

static const struct file_operations dptx_dpcd_read_fops = {
	.open = dptx_dpcd_read_open,
	.write = NULL,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_video_col_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 video_col;

	mutex_lock(&dptx->mutex);
	video_col = dptx_get_video_colorimetry(dptx);
	seq_printf(s, "%d\n", video_col);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_video_col_write(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	int retval = 0;
	char buf[3];
	u8 video_col;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	if (kstrtou8(buf, 10, &video_col) < 0) {
		retval = -EINVAL;
		goto done;
	}
	retval = dptx_set_video_colorimetry(dptx, video_col);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_video_col_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_video_col_show, inode->i_private);
}

static const struct file_operations dptx_video_col_fops = {
	.open = dptx_video_col_open,
	.write = dptx_video_col_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_video_range_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 dynamic_range;

	mutex_lock(&dptx->mutex);
	dynamic_range = dptx_get_video_dynamic_range(dptx);
	seq_printf(s, "%d\n", dynamic_range);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_video_range_write(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	int retval = 0;
	char buf[3];
	u8 dynamic_range;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	if (kstrtou8(buf, 10, &dynamic_range) < 0) {
		retval = -EINVAL;
		goto done;
	}
	retval = dptx_set_video_dynamic_range(dptx, dynamic_range);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_video_range_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_video_range_show, inode->i_private);
}

static const struct file_operations dptx_video_range_fops = {
	.open = dptx_video_range_open,
	.write = dptx_video_range_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_video_format_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 video_format;

	mutex_lock(&dptx->mutex);
	video_format = dptx_get_video_format(dptx);
	seq_printf(s, "%d\n", video_format);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_video_format_write(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	int retval = 0;
	char buf[3];
	u8 video_format;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	if (kstrtou8(buf, 10, &video_format) < 0) {
		retval = -EINVAL;
		goto done;
	}
	retval = dptx_set_video_format(dptx, video_format);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_video_format_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_video_format_show, inode->i_private);
}

static const struct file_operations dptx_video_format_fops = {
	.open = dptx_video_format_open,
	.write = dptx_video_format_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_pixel_enc_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 pixel_enc;

	mutex_lock(&dptx->mutex);
	pixel_enc = dptx_get_pixel_enc(dptx);
	seq_printf(s, "%d\n", pixel_enc);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_pixel_enc_write(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	int retval = 0;
	char buf[3];
	u8 pixel_enc;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	if (kstrtou8(buf, 10, &pixel_enc) < 0) {
		retval = -EINVAL;
		goto done;
	}
	retval = dptx_set_pixel_enc(dptx, pixel_enc);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_pixel_enc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_pixel_enc_show, inode->i_private);
}

static const struct file_operations dptx_pix_enc_fops = {
	.open = dptx_pixel_enc_open,
	.write = dptx_pixel_enc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dptx_bpc_show(struct seq_file *s, void *unused)
{
	struct dptx *dptx = s->private;
	u8 bpc;

	mutex_lock(&dptx->mutex);
	bpc = dptx_get_bpc(dptx);
	seq_printf(s, "%d\n", bpc);
	mutex_unlock(&dptx->mutex);

	return 0;
}

static ssize_t dptx_bpc_write(struct file *file,
			      const char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	int retval = 0;
	char buf[3];
	u8 bpc;

	struct seq_file *s = file->private_data;
	struct dptx *dptx = s->private;

	mutex_lock(&dptx->mutex);
	memset(buf, 0, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		retval = -EFAULT;
		goto done;
	}
	if (kstrtou8(buf, 10, &bpc) < 0) {
		retval = -EINVAL;
		goto done;
	}
	retval = dptx_set_bpc(dptx, bpc);
	if (retval)
		goto done;
	retval = count;
done:
	mutex_unlock(&dptx->mutex);
	return retval;
}

static int dptx_bpc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dptx_bpc_show, inode->i_private);
}

static const struct file_operations dptx_bpc_fops = {
	.open = dptx_bpc_open,
	.write = dptx_bpc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void dptx_debugfs_init(struct dptx *dptx)
{
	struct dentry *root;
	struct dentry *video;
	struct dentry *link;
	struct dentry *file;

	root = debugfs_create_dir(dev_name(dptx->dev), NULL);
	if (IS_ERR_OR_NULL(root)) {
		DRM_ERROR("Can't create debugfs root\n");
		return;
	}

	link = debugfs_create_dir("link", root);
	if (IS_ERR_OR_NULL(link)) {
		DRM_ERROR("Can't create debugfs link\n");
		debugfs_remove_recursive(root);
		return;
	}

	video = debugfs_create_dir("video", root);
	if (IS_ERR_OR_NULL(video)) {
		DRM_ERROR("Can't create debugfs video\n");
		debugfs_remove_recursive(root);
		return;
	}

	/* Core driver */
	debugfs_create_u8("max_rate", 0644, root,
			  &dptx->max_rate);
	debugfs_create_u8("max_lane_count", 0644, root,
			  &dptx->max_lanes);
	debugfs_create_u8("pixel_mode_sel", 0644, root,
			  &dptx->multipixel);

	/* DPCD */
	file = debugfs_create_file("rx_caps", 0644,
				   root, dptx, &dptx_rx_caps_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video rx_caps\n");

	debugfs_create_u32("dpcd_addr", 0644, root,
			   &dpcd_addr);

	file = debugfs_create_file("dpcd_read", 0644,
				   root, dptx, &dptx_dpcd_read_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video dpcd_read\n");

	/* Link */
	debugfs_create_u8("rate", 0444, link,
			  &dptx->link.rate);
	debugfs_create_u8("lane_count", 0444, link,
			  &dptx->link.lanes);
	debugfs_create_u8("aux_type", 0644, link,
			  &aux_type);
	debugfs_create_u32("aux_addr", 0644, link,
			   &aux_addr);
	debugfs_create_u32("aux_size", 0644, link,
			   &aux_size);

	debugfs_create_bool("trained", 0444, link,
			    &dptx->link.trained);

	file = debugfs_create_file("status", 0444,
				   link, dptx, &dptx_link_status_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs link status\n");

	file = debugfs_create_file("retrain", 0644,
				   link, dptx, &dptx_link_retrain_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs link retrain\n");

	file = debugfs_create_file("aux", 0644,
				   link, dptx, &dptx_aux_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs aux\n");

	/* Video */

	debugfs_create_u32("refresh_rate", 0644, video,
			   &dptx->vparams.refresh_rate);

	file = debugfs_create_file("colorimetry", 0644,
				   video, dptx, &dptx_video_col_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video colorimetry\n");

	file = debugfs_create_file("dynamic_range", 0644,
				   video, dptx, &dptx_video_range_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs dynamic_range\n");

	file = debugfs_create_file("bpc", 0644, video, dptx,
				   &dptx_bpc_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video bpc\n");

	file = debugfs_create_file("pix_enc", 0644, video, dptx,
				   &dptx_pix_enc_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video pix_enc\n");

	file = debugfs_create_file("video_format", 0644, video,
				   dptx, &dptx_video_format_fops);
	if (!file)
		DRM_DEBUG("Can't create debugfs video video_format\n");

	dptx->root = root;
}

void dptx_debugfs_exit(struct dptx *dptx)
{
	debugfs_remove_recursive(dptx->root);
}
