// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/hwspinlock.h>
#include <linux/iio/iio.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

/* PMIC global registers definition */
#define SC2731_MODULE_EN		0xc08
#define SC2730_MODULE_EN		0x1808
#define UMP9620_MODULE_EN		0x2008
#define SC27XX_MODULE_ADC_EN		BIT(5)
#define SC2721_ARM_CLK_EN		0xc0c
#define SC2731_ARM_CLK_EN		0xc10
#define SC2730_ARM_CLK_EN		0x180c
#define UMP9620_ARM_CLK_EN		0x200c
#define SC27XX_CLK_ADC_EN		BIT(5)
#define SC27XX_CLK_ADC_CLK_EN		BIT(6)

/* ADC controller registers definition */
#define SC27XX_ADC_CTL			0x0
#define SC27XX_ADC_CH_CFG		0x4
#define SC27XX_ADC_DATA			0x4c
#define SC27XX_ADC_INT_EN		0x50
#define SC27XX_ADC_INT_CLR		0x54
#define SC27XX_ADC_INT_STS		0x58
#define SC27XX_ADC_INT_RAW		0x5c

/* Bits and mask definition for SC27XX_ADC_CTL register */
#define SC27XX_ADC_EN			BIT(0)
#define SC27XX_ADC_CHN_RUN		BIT(1)
#define SC27XX_ADC_12BIT_MODE		BIT(2)
#define SC27XX_ADC_RUN_NUM_MASK		GENMASK(7, 4)
#define SC27XX_ADC_RUN_NUM_SHIFT	4

/* Bits and mask definition for SC27XX_ADC_CH_CFG register */
#define SC27XX_ADC_CHN_ID_MASK		GENMASK(4, 0)
#define SC27XX_ADC_SCALE_MASK		GENMASK(10, 9)
#define SC2721_ADC_SCALE_MASK		BIT(5)
#define SC27XX_ADC_SCALE_SHIFT		9
#define SC2721_ADC_SCALE_SHIFT		5

/* Bits definitions for SC27XX_ADC_INT_EN registers */
#define SC27XX_ADC_IRQ_EN		BIT(0)

/* Bits definitions for SC27XX_ADC_INT_CLR registers */
#define SC27XX_ADC_IRQ_CLR		BIT(0)

/* Bits definitions for SC27XX_ADC_INT_RAW registers */
#define SC27XX_ADC_IRQ_RAW		BIT(0)

/* Mask definition for SC27XX_ADC_DATA register */
#define SC27XX_ADC_DATA_MASK		GENMASK(11, 0)

/* Timeout (ms) for the trylock of hardware spinlocks */
#define SC27XX_ADC_HWLOCK_TIMEOUT	5000

/* Maximum ADC channel number */
#define SC27XX_ADC_CHANNEL_MAX		32

/* Timeout (us) for ADC data conversion according to ADC datasheet */
#define SC27XX_ADC_RDY_TIMEOUT		1000000
#define SC27XX_ADC_POLL_RAW_STATUS	500

/* ADC voltage ratio definition */
#define SC27XX_VOLT_RATIO(n, d)		\
	(((n) << SC27XX_RATIO_NUMERATOR_OFFSET) | (d))
#define SC27XX_RATIO_NUMERATOR_OFFSET	16
#define SC27XX_RATIO_DENOMINATOR_MASK	GENMASK(15, 0)

/* ADC specific channel reference voltage 3.5V */
#define SC27XX_ADC_REFVOL_VDD35		3500000

/* ADC default channel reference voltage is 2.8V */
#define SC27XX_ADC_REFVOL_VDD28		2800000

#define SPRD_ADC_CELL_MAX		(2)
#define SPRD_ADC_INVALID_DATA		(0XFFFFFFFF)
#define SPRD_ADC_SCALE_MAX		(4)

enum SPRD_ADC_LOG_LEVEL {
	SPRD_ADC_LOG_LEVEL_ERR,
	SPRD_ADC_LOG_LEVEL_INFO,
	SPRD_ADC_LOG_LEVEL_DBG,
};

static int sprd_adc_log_level = SPRD_ADC_LOG_LEVEL_ERR;
module_param(sprd_adc_log_level, int, 0644);
MODULE_PARM_DESC(sprd_adc_log_level, "sprd adc dbg log enable (default: 0)");
#define SPRD_ADC_DBG(fmt, ...)						\
do {									\
	if (sprd_adc_log_level >= SPRD_ADC_LOG_LEVEL_DBG)		\
		pr_err("[%s] "pr_fmt(fmt), "SPRD_ADC", ##__VA_ARGS__);	\
} while (0)

#define SPRD_ADC_INFO(fmt, ...)							\
	do {									\
		if (sprd_adc_log_level >= SPRD_ADC_LOG_LEVEL_INFO)		\
			pr_err("[%s] "pr_fmt(fmt), "SPRD_ADC", ##__VA_ARGS__);	\
	} while (0)

#define SPRD_ADC_ERR(fmt, ...)						\
do {									\
	if (sprd_adc_log_level >= SPRD_ADC_LOG_LEVEL_ERR)		\
		pr_err("[%s] "pr_fmt(fmt), "SPRD_ADC", ##__VA_ARGS__);	\
} while (0)


#define SC27XX_ADC_CHANNEL(index, mask) {			\
	.type = IIO_VOLTAGE,					\
	.channel = index,					\
	.info_mask_separate = mask | BIT(IIO_CHAN_INFO_SCALE),	\
	.datasheet_name = "CH##index",				\
	.indexed = 1,						\
}

enum sc27xx_pmic_type {
	SC27XX_ADC,
	SC2721_ADC,
	UMP9620_ADC,
};

enum SPRD_ADC_GRAPH_TYPE {
	COMMON_BIG_SCALE_GRAPH,
	COMMON_SMALL_SCALE_GRAPH,
	SC2721_BIG_SCALE_GRAPH,
	SC2721_SMALL_SCALE_GRAPH,
	SC2730_BIG_SCALE_GRAPH,
	SC2730_SMALL_SCALE_GRAPH,
	UMP96XX_BIG_SCALE_GRAPH,
	UMP96XX_SMALL_SCALE_GRAPH,
	UMP96XX_VBAT_DET_GRAPH,
	SPRD_ADC_GRAPH_TYPE_MAX
};

struct sprd_adc_pm_data {
	struct regmap *pm_regmap;
	u32 clk26m_vote_reg;/* adc clk26 votre reg */
	u32 clk26m_vote_reg_mask;/* adc clk26 votre reg mask */
	bool pm_ctl_support;
	bool dev_suspended;
};

struct sc27xx_adc_data {
	struct iio_dev *indio_dev;
	struct device *dev;
	struct regulator *volref;
	struct regmap *regmap;
	/*
	 * One hardware spinlock to synchronize between the multiple
	 * subsystems which will access the unique ADC controller.
	 */
	struct hwspinlock *hwlock;
	u32 base;
	int irq;
	u32 graphs_index[SC27XX_ADC_CHANNEL_MAX];
	int channel_scale[SC27XX_ADC_CHANNEL_MAX];
	const struct sc27xx_adc_variant_data *var_data;
	struct sprd_adc_pm_data pm_data;
};

/*
 * Since different PMICs of SC27xx series can have different
 * address and ratio, we should save ratio config and base
 * in the device data structure.
 */
struct sc27xx_adc_variant_data {
	enum sc27xx_pmic_type pmic_type;
	u32 module_en;
	u32 clk_en;
	u32 scale_shift;
	u32 scale_mask;
	u32 calib_graphs_index[SPRD_ADC_GRAPH_TYPE_MAX];
	void (*init_scale)(struct sc27xx_adc_data *data);
	int (*get_ratio)(int channel, int scale);
	u32 (*get_graph)(int channel, int scale);
};

struct sc27xx_adc_linear_graph {
	const char *cell_names[SPRD_ADC_CELL_MAX+1];/* must end with NULL point */
	int cell_value[SPRD_ADC_CELL_MAX+1];
	void (*calibrate)(struct sc27xx_adc_linear_graph *graph);
	int volt0;
	int adc0;
	int volt1;
	int adc1;
};

static void sprd_adc_calib_with_one_cell(struct sc27xx_adc_linear_graph *graph);
static void sprd_adc_calib_with_two_cell(struct sc27xx_adc_linear_graph *graph);
/*
 * According to the datasheet, we can convert one ADC value to one voltage value
 * through 2 points in the linear graph. If the voltage is less than 1.2v, we
 * should use the small-scale graph, and if more than 1.2v, we should use the
 * big-scale graph.
 */
static struct sc27xx_adc_linear_graph sprd_adc_linear_graphs[] = {
	[COMMON_BIG_SCALE_GRAPH] = {
		.cell_names = {"big_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  4200,
		.adc0  =  3310,
		.volt1 =  3600,
		.adc1  =  2832,
	},
	[COMMON_SMALL_SCALE_GRAPH] = {
		.cell_names = {"small_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  1000,
		.adc0  =  3413,
		.volt1 =  100,
		.adc1  =  341,
	},
	[SC2721_BIG_SCALE_GRAPH] = {
		.cell_names = {"big_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  4200,
		.adc0  =  850,
		.volt1 =  3600,
		.adc1  =  728,
	},
	[SC2721_SMALL_SCALE_GRAPH] = {
		.cell_names = {"small_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  1000,
		.adc0  =  838,
		.volt1 =  100,
		.adc1  =  84,
	},
	[SC2730_BIG_SCALE_GRAPH] = {
		.cell_names = {"big_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  4200,
		.adc0  =  856,
		.volt1 =  3600,
		.adc1  =  733,
	},
	[SC2730_SMALL_SCALE_GRAPH] = {
		.cell_names = {"small_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  1000,
		.adc0  =  833,
		.volt1 =  100,
		.adc1  =  80,
	},
	[UMP96XX_BIG_SCALE_GRAPH] = {
		.cell_names = {"big_scale_calib1", "big_scale_calib2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  4200,
		.adc0  =  3310,
		.volt1 =  3600,
		.adc1  =  2832,
	},
	[UMP96XX_SMALL_SCALE_GRAPH] = {
		.cell_names = {"small_scale_calib1", "small_scale_calib2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  1000,
		.adc0  =  3413,
		.volt1 =  100,
		.adc1  =  341,
	},
	[UMP96XX_VBAT_DET_GRAPH] = {
		.cell_names = {"vbat_det_cal1", "vbat_det_cal2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  1400,
		.adc0  =  3482,
		.volt1 =  200,
		.adc1  =  476,
	},
};

static const struct iio_chan_spec sc27xx_channels[] = {
	SC27XX_ADC_CHANNEL(0, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(1, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(2, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(3, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(4, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(5, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(6, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(7, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(8, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(9, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(10, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(11, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(12, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(13, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(14, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(15, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(16, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(17, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(18, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(19, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(20, BIT(IIO_CHAN_INFO_RAW)),
	SC27XX_ADC_CHANNEL(21, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(22, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(23, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(24, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(25, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(26, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(27, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(28, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(29, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(30, BIT(IIO_CHAN_INFO_PROCESSED)),
	SC27XX_ADC_CHANNEL(31, BIT(IIO_CHAN_INFO_PROCESSED)),
};

static void sprd_adc_calib_with_one_cell(struct sc27xx_adc_linear_graph *graph)
{
	int calib_data = graph->cell_value[0];

	SPRD_ADC_DBG("calib before: adc0: %d:, adc1:%d, calib_data: %d\n",
		     graph->adc0, graph->adc1, calib_data);

	graph->adc0 = (((calib_data & 0xff) + graph->adc0 - 128) * 4);
	graph->adc1 = ((((calib_data >> 8) & 0xff) + graph->adc1 - 128) * 4);

	SPRD_ADC_DBG("calib aft: adc0: %d:, adc1:%d, calib_data: %d\n",
		     graph->adc0, graph->adc1, calib_data);
}

static void sprd_adc_calib_with_two_cell(struct sc27xx_adc_linear_graph *graph)
{
	int adc_calib_data0 = graph->cell_value[0];
	int adc_calib_data1 = graph->cell_value[1];

	SPRD_ADC_DBG("calib before: adc0: %d:, adc1:%d, calib_data0: %d, calib_data1: %d\n",
		     graph->adc0, graph->adc1, adc_calib_data0, adc_calib_data1);

	graph->adc0 = (adc_calib_data0 & 0xfff0) >> 4;
	graph->adc1 = (adc_calib_data1 & 0xfff0) >> 4;

	SPRD_ADC_DBG("calib aft: adc0: %d:, adc1:%d, calib_data0: %d, calib_data1: %d\n",
		     graph->adc0, graph->adc1, adc_calib_data0, adc_calib_data1);
}

static int adc_nvmem_cell_calib_data(struct sc27xx_adc_data *data, const char *cell_name)
{
	struct nvmem_cell *cell;
	void *buf;
	u32 calib_data = 0;
	size_t len = 0;

	if (!data)
		return -EINVAL;

	cell = nvmem_cell_get(data->dev, cell_name);
	if (IS_ERR_OR_NULL(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR_OR_NULL(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	SPRD_ADC_DBG("cell_name: %s:, calib_data:%d\n", cell_name, calib_data);

	kfree(buf);
	nvmem_cell_put(cell);
	return calib_data;
}

static int sprd_adc_graphs_calibrate(struct sc27xx_adc_data *data)
{
	int i, j, index = 0;
	struct sc27xx_adc_linear_graph *graphs = sprd_adc_linear_graphs;

	for (i = 0; data->var_data->calib_graphs_index[i] != SPRD_ADC_INVALID_DATA; i++) {
		index = data->var_data->calib_graphs_index[i];
		for (j = 0; graphs[index].cell_names[j] != NULL; j++) {
			graphs[index].cell_value[j] =
				adc_nvmem_cell_calib_data(data, graphs[index].cell_names[j]);
			if (graphs[index].cell_value[j] < 0) {
				SPRD_ADC_ERR("calib err! %s:%d\n", graphs[index].cell_names[j],
					     graphs[index].cell_value[j]);
				return graphs[index].cell_value[j];
			}
		}
	}

	for (i = 0; data->var_data->calib_graphs_index[i] != SPRD_ADC_INVALID_DATA; i++) {
		index = data->var_data->calib_graphs_index[i];
		graphs[index].calibrate(&graphs[index]);
	}

	return 0;
}

static int sprd_adc_graphs_init(struct sc27xx_adc_data *data)
{
	int ret = 0, channel, scale;

	ret = sprd_adc_graphs_calibrate(data);
	if (ret)
		return ret;

	for (channel = 0; channel < SC27XX_ADC_CHANNEL_MAX; channel++)
		for (scale = 0; scale < SPRD_ADC_SCALE_MAX; scale++)
			data->graphs_index[channel] = data->var_data->get_graph(channel, scale);

	return 0;
}

static u32 sc2721_adc_get_graph(int channel, int scale)
{
	u32 graph_index = 0;

	switch (channel) {
	case 5:
		graph_index = SC2721_BIG_SCALE_GRAPH;
		break;
	default:
		graph_index = SC2721_SMALL_SCALE_GRAPH;
		break;
	}

	return graph_index;
}

static u32 sc2730_adc_get_graph(int channel, int scale)
{
	u32 graph_index = 0;

	switch (channel) {
	case 5:
		graph_index = SC2730_BIG_SCALE_GRAPH;
		break;
	default:
		graph_index = SC2730_SMALL_SCALE_GRAPH;
		break;
	}

	return graph_index;
}

static u32 ump96xx_adc_get_graph(int channel, int scale)
{
	u32 graph_index = 0;

	switch (channel) {
	case 0:
		graph_index = UMP96XX_VBAT_DET_GRAPH;
		break;
	case 11:
		graph_index = UMP96XX_BIG_SCALE_GRAPH;
		break;
	default:
		graph_index = UMP96XX_SMALL_SCALE_GRAPH;
		break;
	}

	return graph_index;
}

static int sc2720_adc_get_ratio(int channel, int scale)
{
	switch (channel) {
	/*
	 * channel 5 and channel 1 has been calibrated, so we can just
	 * return the voltage values calculated by ratio(1,1)
	 */
	case 1:
	case 5:
		return SC27XX_VOLT_RATIO(1, 1);
	case 14:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(68, 900);
		case 1:
			return SC27XX_VOLT_RATIO(68, 1760);
		case 2:
			return SC27XX_VOLT_RATIO(68, 2327);
		case 3:
			return SC27XX_VOLT_RATIO(68, 3654);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 16:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(48, 100);
		case 1:
			return SC27XX_VOLT_RATIO(480, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(480, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(48, 406);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 21:
	case 22:
	case 23:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(3, 8);
		case 1:
			return SC27XX_VOLT_RATIO(375, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(375, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(300, 3248);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	default:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(1, 1);
		case 1:
			return SC27XX_VOLT_RATIO(1000, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(1000, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(100, 406);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	}
	return SC27XX_VOLT_RATIO(1, 1);
}

static int sc2730_adc_get_ratio(int channel, int scale)
{
	switch (channel) {
	/*
	 * channel 5 and channel 1 has been calibrated, so we can just
	 * return the voltage values calculated by ratio(1,1)
	 */
	case 1:
	case 5:
		return SC27XX_VOLT_RATIO(1, 1);
	case 14:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(68, 900);
		case 1:
			return SC27XX_VOLT_RATIO(68, 1760);
		case 2:
			return SC27XX_VOLT_RATIO(68, 2327);
		case 3:
			return SC27XX_VOLT_RATIO(68, 3654);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 15:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(1, 3);
		case 1:
			return SC27XX_VOLT_RATIO(1000, 5865);
		case 2:
			return SC27XX_VOLT_RATIO(500, 3879);
		case 3:
			return SC27XX_VOLT_RATIO(500, 6090);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 16:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(48, 100);
		case 1:
			return SC27XX_VOLT_RATIO(480, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(480, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(48, 406);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 21:
	case 22:
	case 23:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(3, 8);
		case 1:
			return SC27XX_VOLT_RATIO(375, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(375, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(300, 3248);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	default:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(1, 1);
		case 1:
			return SC27XX_VOLT_RATIO(1000, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(1000, 2586);
		case 3:
			return SC27XX_VOLT_RATIO(1000, 4060);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	}
	return SC27XX_VOLT_RATIO(1, 1);
}

static int sc2721_adc_get_ratio(int channel, int scale)
{
	switch (channel) {
	/*
	 * channel 5 and channel 1 has been calibrated, so we can just
	 * return the voltage values calculated by ratio(1,1)
	 */
	case 1:
	case 5:
		return SC27XX_VOLT_RATIO(1, 1);
	case 2:
	case 3:
	case 4:
		return scale ? SC27XX_VOLT_RATIO(400, 1025) :
			SC27XX_VOLT_RATIO(1, 1);
	case 7:
	case 9:
		return scale ? SC27XX_VOLT_RATIO(100, 125) :
			SC27XX_VOLT_RATIO(1, 1);
	case 14:
		return SC27XX_VOLT_RATIO(68, 900);
	case 16:
		return SC27XX_VOLT_RATIO(48, 100);
	case 19:
		return SC27XX_VOLT_RATIO(1, 3);
	default:
		return SC27XX_VOLT_RATIO(1, 1);
	}
	return SC27XX_VOLT_RATIO(1, 1);
}

static int sc2731_adc_get_ratio(int channel, int scale)
{
	switch (channel) {
	/*
	 * channel 5 and channel 1 has been calibrated, so we can just
	 * return the voltage values calculated by ratio(1,1)
	 */
	case 1:
	case 5:
		return SC27XX_VOLT_RATIO(1, 1);
	case 2:
	case 3:
	case 4:
		return scale ? SC27XX_VOLT_RATIO(400, 1025) :
			SC27XX_VOLT_RATIO(1, 1);
	case 6:
		return SC27XX_VOLT_RATIO(375, 9000);
	case 7:
	case 8:
		return scale ? SC27XX_VOLT_RATIO(100, 125) :
			SC27XX_VOLT_RATIO(1, 1);
	case 19:
		return SC27XX_VOLT_RATIO(1, 3);
	default:
		return SC27XX_VOLT_RATIO(1, 1);
	}
	return SC27XX_VOLT_RATIO(1, 1);
}

static int ump9620_adc_get_ratio(int channel, int scale)
{
	switch (channel) {
	/*
	 * channel 11 has been calibrated, so we can just
	 * return the voltage values calculated by ratio(1,1)
	 */
	case 0:
	case 11:
		return SC27XX_VOLT_RATIO(1, 1);
	case 14:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(68, 900);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 15:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(1, 3);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	case 21:
	case 22:
	case 23:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(3, 8);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	default:
		switch (scale) {
		case 0:
			return SC27XX_VOLT_RATIO(1, 1);
		case 1:
			return SC27XX_VOLT_RATIO(1000, 1955);
		case 2:
			return SC27XX_VOLT_RATIO(1000, 2600);
		case 3:
			return SC27XX_VOLT_RATIO(1000, 4060);
		default:
			return SC27XX_VOLT_RATIO(1, 1);
		}
	}
}

static void sc2720_adc_scale_init(struct sc27xx_adc_data *data)
{
	int i;

	for (i = 0; i < SC27XX_ADC_CHANNEL_MAX; i++) {
		switch (i) {
		case 5:
			data->channel_scale[i] = 3;
			break;
		case 7:
		case 9:
			data->channel_scale[i] = 2;
			break;
		case 13:
			data->channel_scale[i] = 1;
			break;
		case 19:
		case 30:
		case 31:
			data->channel_scale[i] = 3;
			break;
		default:
			data->channel_scale[i] = 0;
			break;
		}
	}
}

static void sc2731_adc_scale_init(struct sc27xx_adc_data *data)
{
	int i;

	for (i = 0; i < SC27XX_ADC_CHANNEL_MAX; i++) {
		if (i == 5)
			data->channel_scale[i] = 1;
		else
			data->channel_scale[i] = 0;
	}
}

static void sc2730_adc_scale_init(struct sc27xx_adc_data *data)
{
	int i;

	for (i = 0; i < SC27XX_ADC_CHANNEL_MAX; i++) {
		if (i == 5 || i == 10 || i == 19 || i == 30 || i == 31)
			data->channel_scale[i] = 3;
		else if (i == 7 || i == 9)
			data->channel_scale[i] = 2;
		else if (i == 13)
			data->channel_scale[i] = 1;
		else
			data->channel_scale[i] = 0;
	}
}

static void ump9620_adc_scale_init(struct sc27xx_adc_data *data)
{
	int i;

	for (i = 0; i < SC27XX_ADC_CHANNEL_MAX; i++) {
		if (i == 10 || i == 19 || i == 30 || i == 31)
			data->channel_scale[i] = 3;
		else if (i == 7 || i == 9)
			data->channel_scale[i] = 2;
		else if (i == 0 || i == 13)
			data->channel_scale[i] = 1;
		else
			data->channel_scale[i] = 0;
	}
}

static void sc27xx_adc_regs_dump(struct sc27xx_adc_data *data, int channel, int scale)
{
	static u64 count;
	u32 module_en, adc_clk_en, adc_int_ctl, adc_int_raw, adc_ctl, adc_ch_cfg;

	regmap_read(data->regmap, data->var_data->module_en, &module_en);
	regmap_read(data->regmap, data->var_data->clk_en, &adc_clk_en);
	regmap_read(data->regmap, data->base + SC27XX_ADC_INT_CLR, &adc_int_ctl);
	regmap_read(data->regmap, data->base + SC27XX_ADC_INT_RAW, &adc_int_raw);
	regmap_read(data->regmap, data->base + SC27XX_ADC_CTL, &adc_ctl);
	regmap_read(data->regmap, data->base + SC27XX_ADC_CH_CFG, &adc_ch_cfg);

	SPRD_ADC_ERR("regs_dump[%llu]->channel: %d, scale: %d, module_en: 0x%x, adc_clk_en: 0x%x,"
		     " adc_int_ctl: 0x%x, adc_int_raw: 0x%x, adc_ctl: 0x%x, adc_ch_cfg: 0x%x\n",
		     count++, channel, scale, module_en, adc_clk_en, adc_int_ctl,
		     adc_int_raw, adc_ctl, adc_ch_cfg);
}

static int sc27xx_adc_read(struct sc27xx_adc_data *data, int channel,
			   int scale, int *val)
{
	int ret = 0, ret_volref = 0;
	u32 rawdata = 0, tmp, status;

	if (data->pm_data.pm_ctl_support && data->pm_data.dev_suspended) {
		SPRD_ADC_ERR("adc_exp: adc clk26 bas been closed, ignore.\n");
		return -EBUSY;
	}

	ret = hwspin_lock_timeout_raw(data->hwlock, SC27XX_ADC_HWLOCK_TIMEOUT);
	if (ret) {
		SPRD_ADC_ERR("timeout to get the hwspinlock\n");
		return ret;
	}

	/*
	 * According to the sc2721 chip data sheet, the reference voltage of
	 * specific channel 30 and channel 31 in ADC module needs to be set from
	 * the default 2.8v to 3.5v.
	 */
	if (data->var_data->pmic_type == SC2721_ADC) {
		if ((channel == 30) || (channel == 31)) {
			ret = regulator_set_voltage(data->volref, SC27XX_ADC_REFVOL_VDD35,
						    SC27XX_ADC_REFVOL_VDD35);
			if (ret) {
				SPRD_ADC_ERR("failed to set the volref 3.5V\n");
				hwspin_unlock_raw(data->hwlock);
				return ret;
			}
		}
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
				 SC27XX_ADC_EN, SC27XX_ADC_EN);
	if (ret)
		goto unlock_adc;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_INT_CLR,
				 SC27XX_ADC_IRQ_CLR, SC27XX_ADC_IRQ_CLR);
	if (ret)
		goto disable_adc;

	/* Configure the channel id and scale */
	tmp = (scale << data->var_data->scale_shift) & data->var_data->scale_mask;
	tmp |= channel & SC27XX_ADC_CHN_ID_MASK;
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CH_CFG,
				 SC27XX_ADC_CHN_ID_MASK |
				 data->var_data->scale_mask,
				 tmp);
	if (ret)
		goto disable_adc;

	/* Select 12bit conversion mode, and only sample 1 time */
	tmp = SC27XX_ADC_12BIT_MODE;
	tmp |= (0 << SC27XX_ADC_RUN_NUM_SHIFT) & SC27XX_ADC_RUN_NUM_MASK;
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
				 SC27XX_ADC_RUN_NUM_MASK | SC27XX_ADC_12BIT_MODE,
				 tmp);
	if (ret)
		goto disable_adc;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
				 SC27XX_ADC_CHN_RUN, SC27XX_ADC_CHN_RUN);
	if (ret)
		goto disable_adc;

	ret = regmap_read_poll_timeout(data->regmap,
				       data->base + SC27XX_ADC_INT_RAW,
				       status, (status & SC27XX_ADC_IRQ_RAW),
				       SC27XX_ADC_POLL_RAW_STATUS,
				       SC27XX_ADC_RDY_TIMEOUT);
	if (ret) {
		SPRD_ADC_ERR("read adc timeout 0x%x\n", status);
		sc27xx_adc_regs_dump(data, channel, scale);
		goto disable_adc;
	}

	ret = regmap_read(data->regmap, data->base + SC27XX_ADC_DATA, &rawdata);
	rawdata &= SC27XX_ADC_DATA_MASK;

disable_adc:
	regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
			   SC27XX_ADC_EN, 0);
unlock_adc:
	if (data->var_data->pmic_type == SC2721_ADC) {
		if ((channel == 30) || (channel == 31)) {
			ret_volref = regulator_set_voltage(data->volref,
							   SC27XX_ADC_REFVOL_VDD28,
							   SC27XX_ADC_REFVOL_VDD28);
			if (ret_volref) {
				SPRD_ADC_ERR("failed to set the volref 2.8V, ret_volref = 0x%x\n",
					     ret_volref);
				ret = ret || ret_volref;
			}
		}
	}

	hwspin_unlock_raw(data->hwlock);

	if (!ret)
		*val = rawdata;

	return ret;
}

static int sprd_adc_calculate_volt_by_graph(struct sc27xx_adc_data *data, int channel,
					    int scale, int raw_adc)
{
	int tmp;
	u32 graph_index = data->graphs_index[channel];
	struct sc27xx_adc_linear_graph *graph = &sprd_adc_linear_graphs[graph_index];

	tmp = (graph->volt0 - graph->volt1) * (raw_adc - graph->adc1);
	tmp /= (graph->adc0 - graph->adc1);
	tmp += graph->volt1;
	tmp = (tmp < 0 ? 0 : tmp);

	SPRD_ADC_DBG("by_graph_c%d: v0 %d a0 %d, v1 %d a1 %d, raw_adc 0x%x, vol_graph %d\n",
		     channel, graph->volt0, graph->adc0, graph->volt1, graph->adc1, raw_adc, tmp);

	return tmp;
}

static int sprd_adc_calculate_volt_by_ratio(struct sc27xx_adc_data *data, int channel,
					    int scale, int vol_graph)
{
	u32 numerator, denominator, ratio, vol_final;

	ratio = data->var_data->get_ratio(channel, scale);
	numerator = ratio >> SC27XX_RATIO_NUMERATOR_OFFSET;
	denominator = ratio & SC27XX_RATIO_DENOMINATOR_MASK;
	vol_final = ((vol_graph * denominator + numerator / 2) / numerator);

	SPRD_ADC_DBG("by_ratio_c%d: type %d, scale %d, nmrtr %d, dmrtr %d, vol_final %d\n",
		     channel, data->var_data->pmic_type, scale, numerator, denominator, vol_final);

	return vol_final;
}

static int sc27xx_adc_read_processed(struct sc27xx_adc_data *data,
				     int channel, int scale, int *val)
{
	int ret, raw_adc, vol_graph;

	ret = sc27xx_adc_read(data, channel, scale, &raw_adc);

	if (ret)
		return ret;

	vol_graph = sprd_adc_calculate_volt_by_graph(data, channel, scale, raw_adc);
	*val = sprd_adc_calculate_volt_by_ratio(data, channel, scale, vol_graph);

	return 0;
}

static int sc27xx_adc_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct sc27xx_adc_data *data = iio_priv(indio_dev);
	int scale = data->channel_scale[chan->channel];
	u32 graph_index = data->graphs_index[chan->channel];
	int ret, tmp;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&indio_dev->mlock);
		ret = sc27xx_adc_read(data, chan->channel, scale, &tmp);
		mutex_unlock(&indio_dev->mlock);

		if (ret)
			return ret;

		*val = tmp;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_PROCESSED:
		mutex_lock(&indio_dev->mlock);
		ret = sc27xx_adc_read_processed(data, chan->channel, scale,
						&tmp);
		mutex_unlock(&indio_dev->mlock);

		if (ret)
			return ret;

		*val = tmp;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = graph_index * 10 + scale;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int sc27xx_adc_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	struct sc27xx_adc_data *data = iio_priv(indio_dev);
	int scale, graph_index, i;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		scale = val % 10;
		graph_index = val / 10;
		if (scale < SPRD_ADC_SCALE_MAX)
			data->channel_scale[chan->channel] = scale;
		for (i = 0; data->var_data->calib_graphs_index[i] != SPRD_ADC_INVALID_DATA; i++) {
			if (graph_index == data->var_data->calib_graphs_index[i])
				data->graphs_index[chan->channel] = graph_index;
		}
		return 0;

	default:
		return -EINVAL;
	}
}

static int sprd_adc_pm_handle(struct sc27xx_adc_data *sc27xx_data, bool enable)
{
	unsigned int regval = 0, regval_bef;
	int ret;

	regmap_read(sc27xx_data->pm_data.pm_regmap, sc27xx_data->pm_data.clk26m_vote_reg,
		    &regval_bef);


	ret = regmap_update_bits(sc27xx_data->pm_data.pm_regmap,
				 sc27xx_data->pm_data.clk26m_vote_reg,
				 sc27xx_data->pm_data.clk26m_vote_reg_mask,
				 enable ? sc27xx_data->pm_data.clk26m_vote_reg_mask : 0);

	regmap_read(sc27xx_data->pm_data.pm_regmap, sc27xx_data->pm_data.clk26m_vote_reg,
		    &regval);

	SPRD_ADC_INFO("enable %d, regval_bef 0x%x, regval 0x%x\n", enable, regval_bef, regval);

	return ret;
}

static int sc27xx_adc_enable(struct sc27xx_adc_data *data)
{
	int ret;

	ret = regmap_update_bits(data->regmap, data->var_data->module_en,
				 SC27XX_MODULE_ADC_EN, SC27XX_MODULE_ADC_EN);
	if (ret)
		return ret;

	/* Enable ADC work clock */
	ret = regmap_update_bits(data->regmap, data->var_data->clk_en,
				 SC27XX_CLK_ADC_EN | SC27XX_CLK_ADC_CLK_EN,
				 SC27XX_CLK_ADC_EN | SC27XX_CLK_ADC_CLK_EN);
	if (ret)
		goto disable_adc;

	return 0;

disable_adc:
	regmap_update_bits(data->regmap, data->var_data->module_en,
			   SC27XX_MODULE_ADC_EN, 0);

	return ret;
}

static void sc27xx_adc_disable(void *_data)
{
	struct sc27xx_adc_data *data = _data;

	/* Disable ADC work clock and controller clock */
	regmap_update_bits(data->regmap, data->var_data->clk_en,
			   SC27XX_CLK_ADC_EN | SC27XX_CLK_ADC_CLK_EN, 0);

	regmap_update_bits(data->regmap, data->var_data->module_en,
			   SC27XX_MODULE_ADC_EN, 0);
}

static void sc27xx_adc_free_hwlock(void *_data)
{
	struct hwspinlock *hwlock = _data;

	hwspin_lock_free(hwlock);
}

static int sc27xx_adc_pm_init(struct sc27xx_adc_data *sc27xx_data)
{
	int ret;
	unsigned int pm_args[2];
	struct device_node *np = sc27xx_data->dev->of_node;

	sc27xx_data->pm_data.pm_ctl_support = false;
	sc27xx_data->pm_data.pm_regmap = NULL;
		syscon_regmap_lookup_by_phandle_args(np, "sprd_adc_pm_reg", 2, pm_args);
	if (!IS_ERR_OR_NULL(sc27xx_data->pm_data.pm_regmap)) {
		sc27xx_data->pm_data.pm_ctl_support = true;
		sc27xx_data->pm_data.clk26m_vote_reg = pm_args[0];
		sc27xx_data->pm_data.clk26m_vote_reg_mask = pm_args[1];
		SPRD_ADC_DBG("sprd_adc_rpm_reg reg 0x%x, mask 0x%x\n", pm_args[0], pm_args[1]);

		ret = sprd_adc_pm_handle(sc27xx_data, true);
		if (ret) {
			SPRD_ADC_ERR("failed to set the ADC clk26m bit8 on IP\n");
			return -EBUSY;
		}

		sc27xx_data->pm_data.dev_suspended = false;
	}

	return 0;

}

static const struct iio_info sc27xx_info = {
	.read_raw = &sc27xx_adc_read_raw,
	.write_raw = &sc27xx_adc_write_raw,
};

static int sc27xx_adc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sc27xx_adc_data *sc27xx_data;
	const struct sc27xx_adc_variant_data *pdata;
	struct iio_dev *indio_dev;
	int ret;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		SPRD_ADC_ERR("No matching driver data found\n");
		return -EINVAL;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*sc27xx_data));
	if (!indio_dev)
		return -ENOMEM;

	sc27xx_data = iio_priv(indio_dev);

	sc27xx_data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc27xx_data->regmap) {
		SPRD_ADC_ERR("failed to get ADC regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &sc27xx_data->base);
	if (ret) {
		SPRD_ADC_ERR("failed to get ADC base address\n");
		return ret;
	}

	sc27xx_data->irq = platform_get_irq(pdev, 0);
	if (sc27xx_data->irq < 0) {
		SPRD_ADC_ERR("failed to get ADC irq number\n");
		return sc27xx_data->irq;
	}

	ret = of_hwspin_lock_get_id(np, 0);
	if (ret < 0) {
		SPRD_ADC_ERR("failed to get hwspinlock id\n");
		return ret;
	}

	sc27xx_data->hwlock = hwspin_lock_request_specific(ret);
	if (!sc27xx_data->hwlock) {
		SPRD_ADC_ERR("failed to request hwspinlock\n");
		return -ENXIO;
	}

	ret = devm_add_action(&pdev->dev, sc27xx_adc_free_hwlock, sc27xx_data->hwlock);
	if (ret) {
		sc27xx_adc_free_hwlock(sc27xx_data->hwlock);
		SPRD_ADC_ERR("failed to add hwspinlock action\n");
		return ret;
	}

	if (pdata->pmic_type == SC2721_ADC) {
		sc27xx_data->volref = devm_regulator_get_optional(&pdev->dev, "vref");
		if (IS_ERR_OR_NULL(sc27xx_data->volref)) {
			ret = PTR_ERR(sc27xx_data->volref);
			SPRD_ADC_ERR("err! ADC volref, err: %d\n", ret);
			return ret;
		}
	}

	sc27xx_data->dev = &pdev->dev;
	sc27xx_data->var_data = pdata;
	sc27xx_data->indio_dev = indio_dev;

	sc27xx_data->var_data->init_scale(sc27xx_data);

	/* ADC channel scales calibration from nvmem device */
	ret = sprd_adc_graphs_init(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("failed to calib graphs from nvmem\n");
		return ret;
	}

	ret = sc27xx_adc_enable(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("failed to enable ADC module\n");
		return ret;
	}

	ret = devm_add_action(&pdev->dev, sc27xx_adc_disable, sc27xx_data);
	if (ret) {
		sc27xx_adc_disable(sc27xx_data);
		SPRD_ADC_ERR("failed to add ADC disable action\n");
		return ret;
	}

	ret = sc27xx_adc_pm_init(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("adc pm init err.\n");
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &sc27xx_info;
	indio_dev->channels = sc27xx_channels;
	indio_dev->num_channels = ARRAY_SIZE(sc27xx_channels);
	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret)
		SPRD_ADC_ERR("could not register iio (ADC)");

	platform_set_drvdata(pdev, indio_dev);

	return ret;
}

static int sc27xx_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct sc27xx_adc_data *sc27xx_data = iio_priv(indio_dev);
	int ret;

	if (sc27xx_data->pm_data.pm_ctl_support) {
		ret = sprd_adc_pm_handle(sc27xx_data, false);
		if (ret)
			SPRD_ADC_ERR("clean clk26m_sinout_pmic failed\n");
	}

	return 0;
}

static int sc27xx_adc_pm_suspend(struct device *dev)
{
	struct sc27xx_adc_data *sc27xx_data = iio_priv(dev_get_drvdata(dev));
	int ret;


	if (!sc27xx_data->pm_data.pm_ctl_support)
		return 0;

	mutex_lock(&sc27xx_data->indio_dev->mlock);

	ret = sprd_adc_pm_handle(sc27xx_data, false);
	if (ret) {
		SPRD_ADC_ERR("clean clk26m_sinout_pmic failed\n");
		mutex_unlock(&sc27xx_data->indio_dev->mlock);
		return 0;
	}
	sc27xx_data->pm_data.dev_suspended = true;

	mutex_unlock(&sc27xx_data->indio_dev->mlock);

	return 0;
}

static int sc27xx_adc_pm_resume(struct device *dev)
{
	int ret;
	struct sc27xx_adc_data *sc27xx_data = iio_priv(dev_get_drvdata(dev));

	if (!sc27xx_data->pm_data.pm_ctl_support)
		return 0;

	mutex_lock(&sc27xx_data->indio_dev->mlock);

	ret = sprd_adc_pm_handle(sc27xx_data, true);
	if (ret) {
		SPRD_ADC_ERR("failed to set the UMP9620 ADC clk26m bit8 on IP\n");
		mutex_unlock(&sc27xx_data->indio_dev->mlock);
		return 0;
	}
	sc27xx_data->pm_data.dev_suspended = false;

	mutex_unlock(&sc27xx_data->indio_dev->mlock);

	return 0;
}

static const struct sc27xx_adc_variant_data sc2731_data = {
	.pmic_type = SC27XX_ADC,
	.module_en = SC2731_MODULE_EN,
	.clk_en = SC2731_ARM_CLK_EN,
	.scale_shift = SC2721_ADC_SCALE_SHIFT,
	.scale_mask = SC2721_ADC_SCALE_MASK,
	.calib_graphs_index = {
		SC2721_BIG_SCALE_GRAPH,
		SC2721_SMALL_SCALE_GRAPH,
		SPRD_ADC_INVALID_DATA},
	.init_scale = sc2731_adc_scale_init,
	.get_ratio = sc2731_adc_get_ratio,
	.get_graph = sc2721_adc_get_graph,
};

static const struct sc27xx_adc_variant_data sc2721_data = {
	.pmic_type = SC2721_ADC,
	.module_en = SC2731_MODULE_EN,
	.clk_en = SC2721_ARM_CLK_EN,
	.scale_shift = SC2721_ADC_SCALE_SHIFT,
	.scale_mask = SC2721_ADC_SCALE_MASK,
	.calib_graphs_index = {
		SC2721_BIG_SCALE_GRAPH,
		SC2721_SMALL_SCALE_GRAPH,
		SPRD_ADC_INVALID_DATA},
	.init_scale = sc2731_adc_scale_init,
	.get_ratio = sc2721_adc_get_ratio,
	.get_graph = sc2721_adc_get_graph,
};

static const struct sc27xx_adc_variant_data sc2730_data = {
	.pmic_type = SC27XX_ADC,
	.module_en = SC2730_MODULE_EN,
	.clk_en = SC2730_ARM_CLK_EN,
	.scale_shift = SC27XX_ADC_SCALE_SHIFT,
	.scale_mask = SC27XX_ADC_SCALE_MASK,
	.calib_graphs_index = {
		SC2730_BIG_SCALE_GRAPH,
		SC2730_SMALL_SCALE_GRAPH,
		SPRD_ADC_INVALID_DATA},
	.init_scale = sc2730_adc_scale_init,
	.get_ratio = sc2730_adc_get_ratio,
	.get_graph = sc2730_adc_get_graph,
};

static const struct sc27xx_adc_variant_data sc2720_data = {
	.pmic_type = SC27XX_ADC,
	.module_en = SC2731_MODULE_EN,
	.clk_en = SC2721_ARM_CLK_EN,
	.scale_shift = SC27XX_ADC_SCALE_SHIFT,
	.scale_mask = SC27XX_ADC_SCALE_MASK,
	.calib_graphs_index = {
		SC2730_BIG_SCALE_GRAPH,
		SC2730_SMALL_SCALE_GRAPH,
		SPRD_ADC_INVALID_DATA},
	.init_scale = sc2720_adc_scale_init,
	.get_ratio = sc2720_adc_get_ratio,
	.get_graph = sc2730_adc_get_graph,
};

static const struct sc27xx_adc_variant_data ump9620_data = {
	.pmic_type = UMP9620_ADC,
	.module_en = UMP9620_MODULE_EN,
	.clk_en = UMP9620_ARM_CLK_EN,
	.scale_shift = SC27XX_ADC_SCALE_SHIFT,
	.scale_mask = SC27XX_ADC_SCALE_MASK,
	.calib_graphs_index = {
		UMP96XX_BIG_SCALE_GRAPH,
		UMP96XX_SMALL_SCALE_GRAPH,
		UMP96XX_VBAT_DET_GRAPH,
		SPRD_ADC_INVALID_DATA},
	.init_scale = ump9620_adc_scale_init,
	.get_ratio = ump9620_adc_get_ratio,
	.get_graph = ump96xx_adc_get_graph,
};

static const struct of_device_id sc27xx_adc_of_match[] = {
	{ .compatible = "sprd,sc2731-adc", .data = &sc2731_data},
	{ .compatible = "sprd,sc2730-adc", .data = &sc2730_data},
	{ .compatible = "sprd,sc2721-adc", .data = &sc2721_data},
	{ .compatible = "sprd,sc2720-adc", .data = &sc2720_data},
	{ .compatible = "sprd,ump9620-adc", .data = &ump9620_data},
	{ }
};

static const struct dev_pm_ops sc27xx_adc_pm_ops = {
	.suspend_noirq = sc27xx_adc_pm_suspend,
	.resume_noirq = sc27xx_adc_pm_resume,
};

static struct platform_driver sc27xx_adc_driver = {
	.probe = sc27xx_adc_probe,
	.remove = sc27xx_adc_remove,
	.driver = {
		.name = "sc27xx-adc",
		.of_match_table = sc27xx_adc_of_match,
		.pm	= &sc27xx_adc_pm_ops,
	},
};

module_platform_driver(sc27xx_adc_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC27XX ADC Driver");
MODULE_LICENSE("GPL v2");
