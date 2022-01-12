#include "ufshcd.h"
#include "ufs-ioctl.h"

/**
 * ufs_sprd_ffu_send_cmd - sends WRITE BUFFER command to do FFU
 * @hba: per adapter instance
 * @idata: ioctl data for ffu
 *
 * Returns 0 if ffu operation is sccessfull
 * Returns non-zero if failed to do ffu
 */
static int sprd_ufs_ffu_send_cmd(struct scsi_device *dev,
	struct ufs_ioctl_ffu_data *idata)
{
	struct ufs_hba *hba;
	unsigned char cmd[10] = {0};
	struct scsi_sense_hdr sshdr;
	unsigned long flags;
	unsigned int fw_size = idata->buf_byte;
	unsigned int chunk_size = idata->chunk_byte;
	unsigned int write_buf_count = 0;
	unsigned int buf_offset = 0;
	int ret = 0;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	spin_lock_irqsave(hba->host->host_lock, flags);

	ret = scsi_device_get(dev);
	if (!ret && !scsi_device_online(dev)) {
		ret = -ENODEV;
		scsi_device_put(dev);
	}


	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;

	while (fw_size > 0) {
		if (fw_size > chunk_size)
			write_buf_count = chunk_size;
		else
			write_buf_count = fw_size;

		cmd[0] = WRITE_BUFFER;                   /* Opcode */
		cmd[1] = 0xE;                            /* 0xE: Download firmware */
		cmd[2] = 0;                              /* Buffer ID = 0 */
		cmd[3] = (buf_offset >> 16) & 0xff;      /* Buffer Offset[23:16]*/
		cmd[4] = (buf_offset >> 8) & 0xff;       /* Buffer Offset[15:08]*/
		cmd[5] = (buf_offset) & 0xff;            /* Buffer Offset[07:00]*/
		cmd[6] = (write_buf_count >> 16) & 0xff; /* Length[23:16] */
		cmd[7] = (write_buf_count >> 8) & 0xff;  /* Length[15:08] */
		cmd[8] = (write_buf_count) & 0xff;       /* Length[07:00] */
		cmd[9] = 0x0;                            /* Control = 0 */

		/*
		 * Current function would be generally called from the power management
		 * callbacks hence set the RQF_PM flag so that it doesn't resume the
		 * already suspended children.
		 */

		ret = scsi_execute(dev, cmd, DMA_TO_DEVICE,
				   idata->buf_ptr + buf_offset, write_buf_count, NULL, &sshdr,
				   msecs_to_jiffies(1000), 0, 0, RQF_PM, NULL);
		if (ret) {
			sdev_printk(KERN_ERR, dev,
				"WRITE BUFFER failed for firmware upgrade\n");

			goto out;
		}
		buf_offset = buf_offset + write_buf_count;
		fw_size = fw_size - write_buf_count;
	}

out:
	scsi_device_put(dev);
	hba->host->eh_noresume = 0;
	return ret;
}

/**
 * sprd_ufs_ioctl_ffu - update device firmware through ioctl
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int sprd_ufs_ioctl_ffu(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	struct ufs_ioctl_ffu_data *idata = NULL;
	struct ufs_ioctl_ffu_data *idata_user = NULL;
	int err = 0;
	u32 attr = 0;

	idata = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	idata_user = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata_user) {
		kfree(idata);
		err = -ENOMEM;
		goto out;
	}

	err = copy_from_user(idata_user, buf_user,
		sizeof(struct ufs_ioctl_ffu_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		kfree(idata);
		kfree(idata_user);
		goto out;
	}

	memcpy(idata, idata_user, sizeof(struct ufs_ioctl_ffu_data));

	idata->buf_ptr = NULL;
	idata->buf_ptr = kzalloc(idata->buf_byte, GFP_KERNEL);

	if (!idata->buf_ptr) {
		err = -ENOMEM;
		goto out_release_mem;
	}

	if (copy_from_user(idata->buf_ptr,
		(void __user *)idata_user->buf_ptr, idata->buf_byte)) {
		err = -EFAULT;
		goto out_release_mem;
	}

	sprd_ufs_device_quiesce(hba);

	err = sprd_ufs_ffu_send_cmd(dev, idata);
	if (err) {
		dev_err(hba->dev, "%s: ffu failed, err 0x%x\n", __func__, err);
		sprd_ufs_device_resume(hba);
	} else {
		dev_info(hba->dev, "%s: ffu ok\n", __func__);
	}

	/*
	 * Check bDeviceFFUStatus attribute
	 *
	 * For reference only since UFS spec. said the status is valid after
	 * device power cycle.
	 */
	err = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
		QUERY_ATTR_IDN_DEVICE_FFU_STATUS, 0, 0, &attr);

	if (err) {
		dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	if (attr > UFS_FFU_STATUS_SUCCESSFUL_UPDATE)
		dev_err(hba->dev, "%s: bDeviceFFUStatus shows fail %d (ref only)\n",
			__func__, attr);

out_release_mem:
	if (idata->buf_ptr != NULL)
		kfree(idata->buf_ptr);
	if (idata != NULL)
		kfree(idata);
	if (idata_user != NULL)
		kfree(idata_user);
out:
	/*
	 * UFS might not be used normally after FFU.
	 * Just reboot system (including device) to avoid following
	 * false alarm. For example, I/O errors.
	 */
	emergency_restart();

	return err;
}

/**
 * sprd_ufs_ioctl_get_dev_info - perform user request: query fw ver
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int sprd_ufs_ioctl_get_dev_info(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	struct ufs_ioctl_query_device_info *idata = NULL;
	int err;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}

	idata = kzalloc(sizeof(struct ufs_ioctl_query_device_info), GFP_KERNEL);

	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user,
			sizeof(struct ufs_ioctl_query_device_info));

	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}
	memcpy(idata->vendor, dev->vendor, UFS_IOCTL_FFU_MAX_VENDOR_BYTES);
	memcpy(idata->model, dev->model, UFS_IOCTL_FFU_MAX_MODEL_BYTES);
	memcpy(idata->fw_rev, dev->rev, UFS_IOCTL_FFU_MAX_FW_VER_BYTES);
	idata->manid = hba->manufacturer_id;
	idata->max_hw_sectors_size = (dev->request_queue->limits.max_hw_sectors << 9);

	err = copy_to_user(buf_user, idata, sizeof(struct ufs_ioctl_query_device_info));
	if (err) {
		dev_err(hba->dev, "%s: err %d copying to user.\n",
				__func__, err);
		goto out_release_mem;
	}

out_release_mem:
	kfree(idata);
out:

	return 0;
}


int sprd_ufs_ioctl_get_pwr_info(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	unsigned int *idata = NULL;
	int err;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}

	idata = kzalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user, sizeof(unsigned int));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	if (hba->ioctl_cmd == UFS_IOCTL_ENTER_MODE) {
		if ((((hba->pwr_info.pwr_tx) << 4)|
		      (hba->pwr_info.pwr_rx)) == 0x22)
			*idata = 1;
		else
			*idata = 0;
	}

	if (hba->ioctl_cmd == UFS_IOCTL_AFC_EXIT) {
		if ((((hba->pwr_info.pwr_tx) << 4)|
		      (hba->pwr_info.pwr_rx)) == 0x11)
			*idata = 1;
		else
			*idata = 0;
	}

	dev_err(hba->dev,
		"%s:gear[0x%x:0x%x],lane[0x%x:0x%x],pwr[0x%x:0x%x],hs_rate=0x%x,idata=0x%x!\n",
		__func__, hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		hba->pwr_info.pwr_rx, hba->pwr_info.pwr_tx,
		hba->pwr_info.hs_rate, *idata);

	err = copy_to_user(buf_user, idata, sizeof(unsigned int));
	if (err) {
		dev_err(hba->dev, "%s: err %d copying to user.\n",
				__func__, err);
		goto out_release_mem;
	}

out_release_mem:
	kfree(idata);
out:
	return 0;
}
