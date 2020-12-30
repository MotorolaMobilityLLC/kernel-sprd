// SPDX-License-Identifier: GPL-2.0
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

#define pr_fmt(fmt) "[sprd-audio-usb:offload] "fmt

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>
#include <linux/usb/hcd.h>
#include <linux/module.h>

#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "usbaudio.h"
#include "card.h"

struct snd_usb_vendor_audio {
	int usb_aud_ofld_en[SNDRV_PCM_STREAM_LAST + 1];
	int usb_aud_should_suspend;
};

static struct snd_usb_vendor_audio vendor_audio;

static int sprd_usb_offload_enable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_usb_audio *chip = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int stream = mc->shift;

	if (!chip) {
		pr_err("%s chip is null\n", __func__);
		return 0;
	}
	if (stream > SNDRV_PCM_STREAM_LAST) {
		pr_err("%s invalid pcm stream %d", __func__, stream);
		return 0;
	}

	ucontrol->value.integer.value[0] = vendor_audio.usb_aud_ofld_en[stream];
	pr_info("%s audio usb offload %s %s\n", __func__,
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ? "playback" : "capture",
		vendor_audio.usb_aud_ofld_en[stream] ? "ON" : "OFF");

	return 0;
}

static int sprd_usb_offload_enable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_usb_audio *chip = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;
	int stream = mc->shift;
	int max = mc->max;
	int val;

	if (!chip) {
		pr_err("%s chip is null\n", __func__);
		return 0;
	}
	if (stream > SNDRV_PCM_STREAM_LAST) {
		pr_err("%s invalid pcm stream %d", __func__, stream);
		return 0;
	}
	val = ucontrol->value.integer.value[0];
	if (val > max) {
		pr_err("val is invalid\n");
		return -EINVAL;
	}
	vendor_audio.usb_aud_ofld_en[stream] = val;

	pr_info("%s audio usb offload %s %s, val=%#x\n", __func__,
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ? "playback" : "capture",
		vendor_audio.usb_aud_ofld_en[stream] ? "ON" : "OFF",
		vendor_audio.usb_aud_ofld_en[stream]);

	return 0;
}

static int sprd_usb_should_suspend_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_usb_audio *chip = snd_kcontrol_chip(kcontrol);

	if (!chip) {
		pr_err("%s chip is null\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = vendor_audio.usb_aud_should_suspend;
	return 0;
}

typedef void (*SET_OFFLOAD_MODE)(struct usb_hcd *hcd, bool is_offload);

static int sprd_usb_should_suspend_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_usb_audio *chip = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max, val;
	struct usb_hcd *hcd;
	SET_OFFLOAD_MODE set_offload_mode;

	if (!chip || !chip->dev || !chip->dev->bus) {
		pr_err("%s chip or dev or bus is null\n", __func__);
		return -EINVAL;
	}

	val = ucontrol->value.integer.value[0];
	if (val > max) {
		pr_err("val is invalid\n");
		return -EINVAL;
	}
	vendor_audio.usb_aud_should_suspend = val;
	pr_info("set to %s\n", val ? "suspend" : "not suspend");
	hcd = bus_to_hcd(chip->dev->bus);
	if (!hcd->driver || !hcd->driver->android_vendor_data1[1]) {
		pr_err("%s hcd driver or usb_aud_mode is null\n", __func__);
		return -EINVAL;
	}
	set_offload_mode = (SET_OFFLOAD_MODE)hcd->driver->android_vendor_data1[1];
	set_offload_mode(hcd, vendor_audio.usb_aud_should_suspend);

	return 0;
}

static struct snd_kcontrol_new sprd_controls[] = {
	SOC_SINGLE_EXT("USB_AUD_OFLD_P_EN", SND_SOC_NOPM,
		SNDRV_PCM_STREAM_PLAYBACK, 1, 0,
		sprd_usb_offload_enable_get,
		sprd_usb_offload_enable_put),
	SOC_SINGLE_EXT("USB_AUD_OFLD_C_EN", SND_SOC_NOPM,
		SNDRV_PCM_STREAM_CAPTURE, 1, 0,
		sprd_usb_offload_enable_get,
		sprd_usb_offload_enable_put),
	SOC_SINGLE_EXT("USB_AUD_SHOULD_SUSPEND", SND_SOC_NOPM, 0, 1, 0,
		       sprd_usb_should_suspend_get,
		       sprd_usb_should_suspend_put),
	{},
};

static int sprd_usb_control_add(struct snd_usb_audio *chip)
{
	int ret = 0;
	int i = 0;

	if (!chip) {
		pr_err("%s failed chip is null\n", __func__);
		return -EINVAL;
	}

	while (sprd_controls[i].name) {
		ret = snd_ctl_add(chip->card, snd_ctl_new1(&sprd_controls[i],
				chip));
		if (ret < 0) {
			dev_err(&chip->dev->dev, "cannot add control.\n");
			return ret;
		}
		i++;
	}

	return 0;
}


static int vendor_connect(struct usb_interface *intf)
{
	return 0;
}

static void vendor_disconnect(struct usb_interface *intf)
{

}

static int set_vendor_interface(struct usb_device *udev,
			     struct usb_host_interface *alts,
			     int iface, int alt)
{
	return 0;
}

static int set_vendor_rate(struct usb_interface *intf, int iface, int rate,
			int alt)
{
	return 1;
}

static int set_vendor_pcm_buf(struct usb_device *udev, int iface)
{
	return 0;
}

static int set_vendor_pcm_intf(struct usb_interface *intf, int iface, int alt,
			    int direction)
{
	return 0;
}

static int set_vendor_pcm_connection(struct usb_device *udev,
				  enum snd_vendor_pcm_open_close onoff,
				  int direction)
{
	return 0;
}

static int set_vendor_pcm_binterval(struct audioformat *fp,
				 struct audioformat *found,
				 int *cur_attr, int *attr)
{
	return 0;
}

static int usb_vendor_add_ctls(struct snd_usb_audio *chip)
{
	return sprd_usb_control_add(chip);
}

static struct snd_usb_audio_vendor_ops usb_audio_vendor_ops = {
	.connect = vendor_connect,
	.disconnect = vendor_disconnect,
	.set_interface = set_vendor_interface,
	.set_rate = set_vendor_rate,
	.set_pcm_buf = set_vendor_pcm_buf,
	.set_pcm_intf = set_vendor_pcm_intf,
	.set_pcm_connection = set_vendor_pcm_connection,
	.set_pcm_binterval = set_vendor_pcm_binterval,
	.usb_add_ctls = usb_vendor_add_ctls,
};

void snd_usb_vendor_set(void)
{
	vendor_audio.usb_aud_ofld_en[SNDRV_PCM_STREAM_PLAYBACK] = 0;
	vendor_audio.usb_aud_ofld_en[SNDRV_PCM_STREAM_CAPTURE] = 0;
	vendor_audio.usb_aud_should_suspend = 1;

	snd_vendor_set_ops(&usb_audio_vendor_ops);
}

int snd_vendor_audio_offload(int stream)
{
	return vendor_audio.usb_aud_ofld_en[stream];
}
