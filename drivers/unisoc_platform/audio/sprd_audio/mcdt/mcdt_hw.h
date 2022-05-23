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

#ifndef _mcdt_hw_
#define _mcdt_hw_

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include <linux/platform_device.h>
#include <linux/slab.h>

#define FIFO_LENGTH 512
/* #define INT_REQ_MCDT_AGCP    (48 + 32) */
#define INT_REQ_MCDT_AGCP    (48)

#if (defined(CONFIG_UNISOC_AUDIO_MCDT) || defined(CONFIG_UNISOC_AUDIO_MCDT_MODULE))
#define MCDT_USED_VERSION	1
#elif (defined(CONFIG_UNISOC_AUDIO_MCDT_R2P0) || defined(CONFIG_UNISOC_AUDIO_MCDT_R2P0_MODULE))
#define MCDT_USED_VERSION	2
#endif

enum MCDT_VERSION{
	MCDT_VERSION_R1 = 1,
	MCDT_VERSION_R2
};

struct irq_desc_mcdt {
	void *data;
	void (*irq_handler)(void *, unsigned int);
	unsigned int cpu_id;
};

struct channel_status {
	int int_enabled;
	int dma_enabled;
	int dma_channel;
	int int_count;
};

enum MCDT_CHAN_NUM {
	MCDT_CHAN0 = 0,
	MCDT_CHAN1,
	MCDT_CHAN2,
	MCDT_CHAN3,
	MCDT_CHAN4,
	MCDT_CHAN5,
	MCDT_CHAN6,
	MCDT_CHAN7,
	MCDT_CHAN8,
	MCDT_CHAN9,
	MCDT_CHAN10
};


enum MCDT_AP_DMA_CHAN {
	MCDT_AP_DMA_CH0 = 0,
	MCDT_AP_DMA_CH1,
	MCDT_AP_DMA_CH2,
	MCDT_AP_DMA_CH3,
	MCDT_AP_DMA_CH4,
	MCDT_AP_DMA_CH5
};

enum {
	dac_channel0 = 0,
	dac_channel1,
	dac_channel2,
	dac_channel3,
	dac_channel4,
	dac_channel5,
	dac_channel6,
	dac_channel7,
	dac_channel8,
	dac_channel9,
	dac_channel10,
	dac_channel_max,
};

enum {
	adc_channel0 = 0,
	adc_channel1,
	adc_channel2,
	adc_channel3,
	adc_channel4,
	adc_channel5,
	adc_channel6,
	adc_channel7,
	adc_channel8,
	adc_channel9,
	adc_channel_max,
};

int mcdt_dac_int_enable_r1(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int emptymark);
int mcdt_dac_int_enable_r2(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int emptymark);

int mcdt_adc_int_enable_r1(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int fullmark);
int mcdt_adc_int_enable_r2(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int fullmark);

/*space available,return bytes*/
unsigned int mcdt_dac_buffer_size_avail_r1(unsigned int channel);
unsigned int mcdt_dac_buffer_size_avail_r2(unsigned int channel);

/*data available,return bytes*/
unsigned int mcdt_adc_data_size_avail_r1(unsigned int channel);
unsigned int mcdt_adc_data_size_avail_r2(unsigned int channel);

int mcdt_write_r1(unsigned int channel, char *pTxBuf, unsigned int size);
int mcdt_write_r2(unsigned int channel, char *pTxBuf, unsigned int size);

int mcdt_read_r1(unsigned int channel, char *pRxBuf, unsigned int size);
int mcdt_read_r2(unsigned int channel, char *pRxBuf, unsigned int size);

int mcdt_dac_dma_enable_r1(unsigned int channel, unsigned int emptymark);
int mcdt_dac_dma_enable_r2(unsigned int channel, unsigned int emptymark);
int mcdt_dac_dma_enable(unsigned int channel, unsigned int emptymark)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		return mcdt_dac_dma_enable_r1(channel, emptymark);
	else
		return mcdt_dac_dma_enable_r2(channel, emptymark);

}

int mcdt_adc_dma_enable_r1(unsigned int channel, unsigned int fullmark);
int mcdt_adc_dma_enable_r2(unsigned int channel, unsigned int fullmark);
int mcdt_adc_dma_enable(unsigned int channel, unsigned int fullmark)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		return mcdt_adc_dma_enable_r1(channel, fullmark);
	else
		return mcdt_adc_dma_enable_r2(channel, fullmark);
}

int get_mcdt_adc_dma_uid(enum MCDT_CHAN_NUM mcdt_chan,
			 enum MCDT_AP_DMA_CHAN mcdt_ap_dma_chan,
			 unsigned int fullmark);
int get_mcdt_dac_dma_uid(enum MCDT_CHAN_NUM mcdt_chan,
			 enum MCDT_AP_DMA_CHAN mcdt_ap_dma_chan,
			 unsigned int emptymark);

void mcdt_dac_dma_disable_r1(unsigned int channel);
void mcdt_dac_dma_disable_r2(unsigned int channel);
void mcdt_dac_dma_disable(unsigned int channel)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		mcdt_dac_dma_disable_r1(channel);
	else
		mcdt_dac_dma_disable_r2(channel);
}

void mcdt_adc_dma_disable_r1(unsigned int channel);
void mcdt_adc_dma_disable_r2(unsigned int channel);
void mcdt_adc_dma_disable(unsigned int channel)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		mcdt_adc_dma_disable_r1(channel);
	else
		mcdt_adc_dma_disable_r2(channel);
}

unsigned long mcdt_dac_phy_addr_r1(unsigned int channel);
unsigned long mcdt_dac_phy_addr_r2(unsigned int channel);

unsigned long mcdt_adc_phy_addr_r1(unsigned int channel);
unsigned long mcdt_adc_phy_addr_r2(unsigned int channel);

unsigned long mcdt_adc_dma_phy_addr_r1(unsigned int channel);
unsigned long mcdt_adc_dma_phy_addr_r2(unsigned int channel);
unsigned long mcdt_adc_dma_phy_addr(unsigned int channel)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		return mcdt_adc_dma_phy_addr_r1(channel);
	else
		return mcdt_adc_dma_phy_addr_r2(channel);
}

unsigned long mcdt_dac_dma_phy_addr_r1(unsigned int channel);
unsigned long mcdt_dac_dma_phy_addr_r2(unsigned int channel);
unsigned long mcdt_dac_dma_phy_addr(unsigned int channel)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		return mcdt_dac_dma_phy_addr_r1(channel);
	else
		return mcdt_dac_dma_phy_addr_r2(channel);
}

int mcdt_dac_disable_r1(unsigned int channel);
int mcdt_dac_disable_r2(unsigned int channel);

int mcdt_adc_disable_r1(unsigned int channel);
int mcdt_adc_disable_r2(unsigned int channel);

unsigned int mcdt_is_dac_full_r1(unsigned int channel);
unsigned int mcdt_is_dac_full_r2(unsigned int channel);

unsigned int mcdt_is_dac_empty_r1(unsigned int channel);
unsigned int mcdt_is_dac_empty_r2(unsigned int channel);

unsigned int mcdt_is_adc_full_r1(unsigned int channel);
unsigned int mcdt_is_adc_full_r2(unsigned int channel);

unsigned int mcdt_is_adc_empty_r1(unsigned int channel);
unsigned int mcdt_is_adc_empty_r2(unsigned int channel);

void mcdt_da_fifo_clr_r1(unsigned int chan_num);
void mcdt_da_fifo_clr_r2(unsigned int chan_num);
void mcdt_da_fifo_clr(unsigned int chan_num)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		mcdt_da_fifo_clr_r1(chan_num);
	else
		mcdt_da_fifo_clr_r2(chan_num);
}

void mcdt_ad_fifo_clr_r1(unsigned int chan_num);
void mcdt_ad_fifo_clr_r2(unsigned int chan_num);
void mcdt_ad_fifo_clr(unsigned int chan_num)
{
	if (MCDT_USED_VERSION == MCDT_VERSION_R1)
		mcdt_ad_fifo_clr_r1(chan_num);
	else
		mcdt_ad_fifo_clr_r2(chan_num);
}
#endif
