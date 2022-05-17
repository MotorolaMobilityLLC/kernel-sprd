// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc audio usb offload driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 */

#define pr_fmt(fmt) "[sprd-audio-usb:offload] "fmt

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
#include <linux/soc/sprd/sprd_musb_offload.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <trace/hooks/audio_usboffload.h>
#include "usbaudio.h"
#include "card.h"

struct snd_usb_vendor_audio {
	int usb_aud_ofld_en[SNDRV_PCM_STREAM_LAST + 1];
	int usb_aud_should_suspend;
};

enum {
	USB_AUD_IIS_WIDTH_16,
	USB_AUD_IIS_WIDTH_24,
};

static struct snd_usb_vendor_audio vendor_audio;

int snd_vendor_audio_offload(int stream)
{
	return vendor_audio.usb_aud_ofld_en[stream];
}

/*
 * @return: 0 not offload mod, 1 offload mod
 */
static int sprd_usb_audio_offload_check(struct snd_usb_audio *chip,
	int stream)
{
	int is_offload_mod = 0;

	if (stream != SNDRV_PCM_STREAM_PLAYBACK &&
		stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s invalid stream %d\n", __func__, stream);
		return is_offload_mod;
	}

	if (!chip) {
		pr_err("%s chip is null stream =%d\n", __func__, stream);
		return is_offload_mod;
	}
	if (snd_vendor_audio_offload(stream))
		is_offload_mod = 1;
	else
		is_offload_mod = 0;

	pr_info("%s usb stream=%s, enter offload mode %d\n", __func__,
		stream ? "capture" : "playback", snd_vendor_audio_offload(stream));

	return is_offload_mod;
}

/* action: false: ep_stop; true: ep_start */
static void usb_offload_ep_action(void *data, void *arg, bool action)
{
	struct snd_usb_endpoint *ep = (struct snd_usb_endpoint *)arg;
	struct usb_hcd *hcd;
	int is_mono = 0;
	int is_pcm_24bit = 0;
	int is_offload_mod;
	int iis_width;
	int ofld_rate;
	int stream;

	if (ep) {
		stream = usb_pipein(ep->pipe) ? SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;
		iis_width = USB_AUD_IIS_WIDTH_24;
		ofld_rate = 48;

		switch (ep->cur_channels) {
		case 2:
			is_mono = false;
			break;
		case 1:
			is_mono = true;
			break;
		default:
			is_mono = false;
			pr_err("%s invalid channel %d\n", __func__, ep->cur_channels);
			break;
		}

		switch (ep->cur_format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			is_pcm_24bit = 0;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			is_pcm_24bit = 1;
			break;
		default:
			is_pcm_24bit = 0;
			pr_err("%s unknown pcm format %d\n", __func__, ep->cur_format);
			break;
		}

		pr_info("%s channels = %d, %s, %s, ep action = %s",
			__func__, ep->cur_channels, is_mono ? "is mono" : "is stereo",
			is_pcm_24bit ? "pcm 24" : "pcm 16bit", action ? "start" : "stop");

		is_offload_mod = sprd_usb_audio_offload_check(ep->chip, stream);

		if (is_offload_mod) {

			hcd = bus_to_hcd(ep->chip->dev->bus);

			if (!hcd->driver) {
				pr_err("%s hcd driver or usb_aud_config is null\n", __func__);
				return;
			}
			if (action) {
				pr_info("%s usb enter offload mode config usb i2s %s, %s, %s, %s\n",
					__func__, stream ? "capture" : "playback",
					is_mono ? "mono" : "stereo",
					is_pcm_24bit ? "data 24bit" : "data 16bit",
					iis_width == USB_AUD_IIS_WIDTH_24 ?
					"iis width 24bit" : "iis width 16bit");
				musb_offload_config(hcd, ep->ep_num, is_mono,
					is_pcm_24bit, iis_width, 48, is_offload_mod);
			} else {
				pr_info("%s close offload mode\n", __func__);
				musb_offload_config(hcd, ep->ep_num, is_mono,
					is_pcm_24bit, iis_width, 48, 0);
			}
		}
	}
}

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

static int sprd_usb_should_suspend_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_usb_audio *chip = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max, val;
	struct usb_hcd *hcd;

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
	if (!hcd->driver) {
		pr_err("%s hcd driver or usb_aud_mode is null\n", __func__);
		return -EINVAL;
	}
	musb_set_offload_mode(hcd, vendor_audio.usb_aud_should_suspend);

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

static void usb_offload_add_ctrl(void *data, struct usb_interface *intf, struct snd_usb_audio *chip)
{

	vendor_audio.usb_aud_ofld_en[SNDRV_PCM_STREAM_PLAYBACK] = 0;
	vendor_audio.usb_aud_ofld_en[SNDRV_PCM_STREAM_CAPTURE] = 0;
	vendor_audio.usb_aud_should_suspend = 1;

	sprd_usb_control_add(chip);
	pr_info("%s\n", __func__);
}

static int sprd_usb_aud_ofld_en(struct snd_usb_audio *chip, int stream)
{
	if (stream != SNDRV_PCM_STREAM_PLAYBACK &&
		stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s invalid stream %d\n", __func__, stream);
		return 0;
	}

	if (!chip) {
		pr_err("%s chip is null stream =%d\n", __func__, stream);
		return 0;
	}

	return snd_vendor_audio_offload(stream);
}

static int sprd_ofld_synctype_ignore(struct snd_usb_audio *chip,
	int stream, int attr)
{
	if (!sprd_usb_aud_ofld_en(chip, stream)) {
		pr_info("not enable usb audio offload, can't ignore\n");
		return 0;
	}
	if (attr != USB_ENDPOINT_SYNC_SYNC) {
		pr_info("offload ignore synctype stream %s sync_type %d\n",
			stream ? "capture" : "playback", attr);
		return 1;
	}
	pr_debug("stream %s sync_type is %d\n",
		stream ? "capture" : "playback", attr);

	return 0;
}

static void usb_offload_synctype_ignore(void *data, void *arg, int attr, bool *need_ignore)
{
	struct snd_usb_substream *subs = (struct snd_usb_substream *)arg;
	struct snd_usb_audio *chip;
	int ignore;

	chip = subs->stream ? subs->stream->chip : NULL;
	ignore = sprd_ofld_synctype_ignore(chip, subs->direction, attr);
	if (ignore) {
		pr_info("ofld_en %d, stream %d, sync_type %#x, ignore this audiofmt\n",
			sprd_usb_aud_ofld_en(chip, subs->direction), subs->direction, attr);
		*need_ignore = true;
	} else {
		*need_ignore = false;
	}

	pr_info("%s, need_ignore = %s\n", __func__, (*need_ignore) ? "true" : "false");
}
static int usb_offload_init(void)
{
	pr_info("%s\n", __func__);
	register_trace_android_vh_audio_usb_offload_connect(usb_offload_add_ctrl, NULL);
	register_trace_android_vh_audio_usb_offload_ep_action(usb_offload_ep_action, NULL);
	register_trace_android_vh_audio_usb_offload_synctype(usb_offload_synctype_ignore, NULL);
	return 0;
}

static void usb_offload_exit(void)
{
	pr_info("%s\n", __func__);
}

postcore_initcall(usb_offload_init);
module_exit(usb_offload_exit);

MODULE_DESCRIPTION("sprd audio usb offload support");
MODULE_LICENSE("GPL");