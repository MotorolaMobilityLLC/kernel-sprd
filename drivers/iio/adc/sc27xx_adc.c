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
#include <linux/sort.h>

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
#define SC27XX_ADC_AVERAGE_SHIFT	8
#define SC27XX_ADC_AVERAGE_MASK		GENMASK(10, 8)

/* Bits and mask definition for SC27XX_ADC_CH_CFG register */
#define SC27XX_ADC_CHN_ID_MASK		GENMASK(4, 0)

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
#define RATIO(n, d)		\
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
#define SPRD_ADC_INIT_MAGIC		(0xa7a77a7a)
#define ADC_MESURE_NUMBER_SW		(15)
#define ADC_MESURE_NUMBER_HW_DEF	(3)/* 2 << 3 = 8 times */

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
	SC2720_ADC,
	SC2721_ADC,
	SC2730_ADC,
	SC2731_ADC,
	UMP9620_ADC,
};

enum SPRD_ADC_GRAPH_TYPE {
	ONE_CELL_BIG_GRAPH,
	ONE_CELL_SMALL_GRAPH,
	TWO_CELL_BIG_GRAPH,
	TWO_CELL_SMALL_GRAPH,
	TWO_CELL_VBAT_DET_GRAPH,
	SPRD_ADC_GRAPH_TYPE_MAX
};

enum SPRD_ADC_REG_TYPE {
	REG_MODULE_EN,
	REG_CLK_EN,
	REG_SCALE,
	REG_ISEN0 = 12,/* CURRENT MODE */
	REG_ISEN1,
	REG_ISEN2,
	REG_ISEN3,
	SPRD_ADC_REG_TYPE_MAX
};

enum SPRD_ADC_REG_BASE {
	BASE_GLB,
	BASE_ANA
};

struct sprd_adc_pm_data {
	struct regmap *pm_regmap;
	u32 clk26m_vote_reg;/* adc clk26 votre reg */
	u32 clk26m_vote_reg_mask;/* adc clk26 votre reg mask */
	bool pm_ctl_support;
	bool dev_suspended;
};


#define CH_DATA_INIT(sl, graph, filter, isen, r0, r1, r2, r3)	\
{									\
	.scale = sl,							\
	.graph_index = graph,						\
	.isen_info = isen,						\
	.filter_info = filter,						\
	.inited = SPRD_ADC_INIT_MAGIC,					\
	.ratio = {r0, r1, r2, r3},					\
}
/*bit[0-7]: scale
 *bit[7-15]: graph_index
 *bit[16-23]: filter_info(bit16: sw filter support, bit[17-23]: hw filter val(2<<n))
 *bit[24-31]: isen_info (bit24: isen support, bit[25-32]: isen val)
 */
struct sprd_adc_channel_data {
	int scale;
	int graph_index;
	int ratio[SPRD_ADC_SCALE_MAX];
	int inited;
	int filter_info;
	int isen_info;
};

#define REG_BIT_INIT(b_base, reg_address, b_mask, b_offset, func, verse)	\
{										\
	.base = b_base,								\
	.reg_addr = reg_address,						\
	.mask = b_mask,								\
	.offset = b_offset,							\
	.inited = SPRD_ADC_INIT_MAGIC,						\
	.get_setval = func,							\
	.reverse = verse							\
}

struct reg_bit {
	u32 base;
	u32 reg_addr;
	u32 mask;
	u32 offset;
	u32 inited;
	bool reverse;
	u32 (*get_setval)(void *pri, int ch, bool set);
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
	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX];
	const struct sc27xx_adc_variant_data *var_data;
	struct sprd_adc_pm_data pm_data;
};

/*
 * Since different PMICs of SC27xx series can have different
 * address and ratio, we should save ratio config and base
 * in the device data structure.
 */
struct sc27xx_adc_variant_data {
	const enum sc27xx_pmic_type pmic_type;
	const struct reg_bit *const reg_list;
	const u32 glb_reg_base;
	const u32 adc_reg_base_offset;
	const u32 calib_graphs_index[SPRD_ADC_GRAPH_TYPE_MAX];
	void (*const ch_data_init)(struct sc27xx_adc_data *data);
};

struct sc27xx_adc_linear_graph {
	const char *cell_names[SPRD_ADC_CELL_MAX+1];/* must end with NULL point */
	int cell_value[SPRD_ADC_CELL_MAX+1];
	void (*const calibrate)(struct sc27xx_adc_linear_graph *graph);
	const int volt0;
	int adc0;
	const int volt1;
	int adc1;
};

static void sprd_adc_calib_with_one_cell(struct sc27xx_adc_linear_graph *graph);
static void sprd_adc_calib_with_two_cell(struct sc27xx_adc_linear_graph *graph);
static u32 sprd_adc_get_isen(void *pri, int ch, bool enable);
static inline u32 GET_REG_ADDR(struct sc27xx_adc_data *data, int index)
{
	u32 base = ((data->var_data->reg_list[index].base == BASE_GLB)
		    ? (data->var_data->glb_reg_base)
		    : (data->base - data->var_data->adc_reg_base_offset));
	return (base + data->var_data->reg_list[index].reg_addr);
}
/*
 * According to the datasheet, we can convert one ADC value to one voltage value
 * through 2 points in the linear graph. If the voltage is less than 1.2v, we
 * should use the small-scale graph, and if more than 1.2v, we should use the
 * big-scale graph.
 */
static struct sc27xx_adc_linear_graph sprd_adc_linear_graphs[] = {
	[ONE_CELL_BIG_GRAPH] = {
		.cell_names = {"big_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  4200,
		.adc0  =  850,
		.volt1 =  3600,
		.adc1  =  728,
	},
	[ONE_CELL_SMALL_GRAPH] = {
		.cell_names = {"small_scale_calib", NULL},
		.calibrate = sprd_adc_calib_with_one_cell,
		.volt0 =  1000,
		.adc0  =  838,
		.volt1 =  100,
		.adc1  =  84,
	},
	[TWO_CELL_BIG_GRAPH] = {
		.cell_names = {"big_scale_calib1", "big_scale_calib2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  4200,
		.adc0  =  3310,
		.volt1 =  3600,
		.adc1  =  2832,
	},
	[TWO_CELL_SMALL_GRAPH] = {
		.cell_names = {"small_scale_calib1", "small_scale_calib2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  1000,
		.adc0  =  3413,
		.volt1 =  100,
		.adc1  =  341,
	},
	[TWO_CELL_VBAT_DET_GRAPH] = {
		.cell_names = {"vbat_det_cal1", "vbat_det_cal2", NULL},
		.calibrate = sprd_adc_calib_with_two_cell,
		.volt0 =  1400,
		.adc0  =  3482,
		.volt1 =  200,
		.adc1  =  476,
	},
};

static const struct reg_bit regs_sc2720[] = {
	[REG_MODULE_EN] = REG_BIT_INIT(BASE_GLB, 0x08, BIT(5), 5, NULL, false),
	[REG_CLK_EN] = REG_BIT_INIT(BASE_GLB, 0x0c, GENMASK(6, 5), 5, NULL, false),
	[REG_SCALE]  = REG_BIT_INIT(BASE_ANA, 0xffff, GENMASK(10, 9), 9, NULL, false),
	[REG_ISEN0]  = REG_BIT_INIT(BASE_GLB, 0x1F0, BIT(13), 13, NULL, false),
	[REG_ISEN1]  = REG_BIT_INIT(BASE_GLB, 0x1F0, GENMASK(11, 9), 9, sprd_adc_get_isen, false),
	[REG_ISEN2]  = REG_BIT_INIT(BASE_ANA, 0xB0, BIT(0), 0, NULL, false),
};

static const struct reg_bit regs_sc2721[] = {
	[REG_MODULE_EN] = REG_BIT_INIT(BASE_GLB, 0x08, BIT(5), 5, NULL, false),
	[REG_CLK_EN] = REG_BIT_INIT(BASE_GLB, 0x0c, GENMASK(6, 5), 5, NULL, false),
	[REG_SCALE] = REG_BIT_INIT(BASE_ANA, 0xffff, BIT(5), 5, NULL, false),
	[REG_ISEN0] = REG_BIT_INIT(BASE_GLB, 0x2A4, BIT(13), 13, NULL, false),
	[REG_ISEN1] = REG_BIT_INIT(BASE_GLB, 0x2A4, GENMASK(12, 10), 10, sprd_adc_get_isen, false),
	[REG_ISEN2] = REG_BIT_INIT(BASE_ANA, 0xB0, BIT(0), 0, NULL, false),
};

static const struct reg_bit regs_sc2730[] = {
	[REG_MODULE_EN] = REG_BIT_INIT(BASE_GLB, 0x08, BIT(5), 5, NULL, false),
	[REG_CLK_EN] = REG_BIT_INIT(BASE_GLB, 0x0c, GENMASK(6, 5), 5, NULL, false),
	[REG_SCALE] = REG_BIT_INIT(BASE_ANA, 0xffff, GENMASK(10, 9), 9, NULL, false),
	[REG_ISEN0] = REG_BIT_INIT(BASE_GLB, 0x384, BIT(0), 0, NULL, true),
	[REG_ISEN1] = REG_BIT_INIT(BASE_GLB, 0x384, BIT(13), 13, NULL, false),
	[REG_ISEN2] = REG_BIT_INIT(BASE_GLB, 0x384, GENMASK(11, 9), 9, sprd_adc_get_isen, false),
	[REG_ISEN3] = REG_BIT_INIT(BASE_ANA, 0xB0, BIT(0), 0, NULL, false),
};

static const struct reg_bit regs_sc2731[] = {
	[REG_MODULE_EN] = REG_BIT_INIT(BASE_GLB, 0x08, BIT(5), 5, NULL, false),
	[REG_CLK_EN] = REG_BIT_INIT(BASE_GLB, 0x10, GENMASK(6, 5), 5, NULL, false),
	[REG_SCALE] = REG_BIT_INIT(BASE_ANA, 0xffff, BIT(5), 5, NULL, false),
	[REG_ISEN0] = REG_BIT_INIT(BASE_GLB, 0x324, BIT(4), 4, NULL, false),
	[REG_ISEN1] = REG_BIT_INIT(BASE_GLB, 0x2B4, BIT(6), 6, NULL, false),
	[REG_ISEN2] = REG_BIT_INIT(BASE_GLB, 0x324, GENMASK(3, 0), 0, sprd_adc_get_isen, false),
	[REG_ISEN3] = REG_BIT_INIT(BASE_ANA, 0xB0, BIT(0), 0, NULL, false),
};

static const struct reg_bit regs_ump9620[] = {
	[REG_MODULE_EN] = REG_BIT_INIT(BASE_GLB, 0x08, BIT(5), 5, NULL, false),
	[REG_CLK_EN] = REG_BIT_INIT(BASE_GLB, 0x0c, GENMASK(6, 5), 5, NULL, false),
	[REG_SCALE] = REG_BIT_INIT(BASE_ANA, 0xffff, GENMASK(10, 9), 9, NULL, false),
	[REG_ISEN0] = REG_BIT_INIT(BASE_GLB, 0x384, BIT(0), 0, NULL, true),
	[REG_ISEN1] = REG_BIT_INIT(BASE_GLB, 0x384, BIT(13), 13, NULL, false),
	[REG_ISEN2] = REG_BIT_INIT(BASE_GLB, 0x384, GENMASK(11, 9), 9, sprd_adc_get_isen, false),
	[REG_ISEN3] = REG_BIT_INIT(BASE_ANA, 0xB0, BIT(0), 0, NULL, false),
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

static void sc2720_ch_data_init(struct sc27xx_adc_data *data)
{
	int ch;
	struct sprd_adc_channel_data ch_data_def =
		CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
			     RATIO(1, 1), RATIO(1000, 1955), RATIO(1000, 2586), RATIO(100, 406));

	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX] = {
		[1] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
				   RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[5] = CH_DATA_INIT(3, ONE_CELL_BIG_GRAPH, 0, 0,
				   RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[7] = CH_DATA_INIT(2, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				   RATIO(1000, 2586), RATIO(100, 406)),
		[9] = CH_DATA_INIT(2, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				   RATIO(1000, 2586), RATIO(100, 406)),
		[13] = CH_DATA_INIT(1, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(100, 406)),
		[14] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(68, 900), RATIO(68, 1760),
				    RATIO(68, 2327), RATIO(68, 3654)),
		[16] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(48, 100), RATIO(480, 1955),
				    RATIO(480, 2586), RATIO(48, 406)),
		[19] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(100, 406)),
		[21] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[22] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[23] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[30] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(100, 406)),
		[31] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(100, 406)),
	};

	for (ch = 0; ch < SC27XX_ADC_CHANNEL_MAX; ch++)
		data->ch_data[ch] = ((ch_data[ch].inited == SPRD_ADC_INIT_MAGIC)
				     ? ch_data[ch] : ch_data_def);
}

static void sc2721_ch_data_init(struct sc27xx_adc_data *data)
{
	int ch;
	struct sprd_adc_channel_data ch_data_def =
		CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
			     RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1));

	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX] = {
		[2] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[3] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[4] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[5] = CH_DATA_INIT(1, ONE_CELL_BIG_GRAPH, 0, 0, RATIO(1, 1), RATIO(1, 1),
				   RATIO(1, 1), RATIO(1, 1)),
		[7] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(100, 125),
				   RATIO(100, 125), RATIO(100, 125)),
		[9] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(100, 125),
				   RATIO(100, 125), RATIO(100, 125)),
		[14] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(68, 900), RATIO(68, 900),
				    RATIO(68, 900), RATIO(68, 900)),
		[16] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(48, 100), RATIO(48, 100),
				   RATIO(48, 100), RATIO(48, 100)),
		[19] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 3), RATIO(1, 3),
				    RATIO(1, 3), RATIO(1, 3)),
	};

	for (ch = 0; ch < SC27XX_ADC_CHANNEL_MAX; ch++)
		data->ch_data[ch] = ((ch_data[ch].inited == SPRD_ADC_INIT_MAGIC)
				     ? ch_data[ch] : ch_data_def);
}

static void sc2730_ch_data_init(struct sc27xx_adc_data *data)
{
	int ch;
	struct sprd_adc_channel_data ch_data_def =
		CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
			     RATIO(1, 1), RATIO(1000, 1955), RATIO(1000, 2586), RATIO(1000, 4060));

	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX] = {
		[1] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
				   RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[3] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0x9, RATIO(1, 1), RATIO(1000, 1955),
				   RATIO(1000, 2586), RATIO(1000, 4060)),
		[5] = CH_DATA_INIT(3, ONE_CELL_BIG_GRAPH, 0, 0, RATIO(1, 1), RATIO(1, 1),
				   RATIO(1, 1), RATIO(1, 1)),
		[7] = CH_DATA_INIT(2, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				   RATIO(1000, 2586), RATIO(1000, 4060)),
		[9] = CH_DATA_INIT(2, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				   RATIO(1000, 2586), RATIO(1000, 4060)),
		[10] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(1000, 4060)),
		[13] = CH_DATA_INIT(1, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(1000, 4060)),
		[14] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(68, 900), RATIO(68, 1760),
				    RATIO(68, 2327), RATIO(68, 3654)),
		[15] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 3), RATIO(1000, 5865),
				    RATIO(500, 3879), RATIO(500, 6090)),
		[16] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(48, 100), RATIO(480, 1955),
				    RATIO(480, 2586), RATIO(48, 406)),
		[19] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(1000, 4060)),
		[21] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[22] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[23] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(3, 8), RATIO(375, 1955),
				    RATIO(375, 2586), RATIO(300, 3248)),
		[30] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(1000, 4060)),
		[31] = CH_DATA_INIT(3, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(1000, 1955),
				    RATIO(1000, 2586), RATIO(1000, 4060)),
	};

	for (ch = 0; ch < SC27XX_ADC_CHANNEL_MAX; ch++)
		data->ch_data[ch] = ((ch_data[ch].inited == SPRD_ADC_INIT_MAGIC)
				     ? ch_data[ch] : ch_data_def);
}

static void sc2731_ch_data_init(struct sc27xx_adc_data *data)
{
	int ch;
	struct sprd_adc_channel_data ch_data_def =
		CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0,
			     RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1));

	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX] = {
		[2] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[3] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[4] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(400, 1025),
				   RATIO(400, 1025), RATIO(400, 1025)),
		[5] = CH_DATA_INIT(1, ONE_CELL_BIG_GRAPH, 0, 0, RATIO(1, 1), RATIO(1, 1),
				   RATIO(1, 1), RATIO(1, 1)),
		[6] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(375, 9000),
				   RATIO(375, 9000), RATIO(375, 9000), RATIO(375, 9000)),
		[7] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(100, 125),
				   RATIO(100, 125), RATIO(100, 125)),
		[8] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1), RATIO(100, 125),
				   RATIO(100, 125), RATIO(100, 125)),
		[19] = CH_DATA_INIT(0, ONE_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 3), RATIO(1, 3),
				    RATIO(1, 3), RATIO(1, 3)),
	};

	for (ch = 0; ch < SC27XX_ADC_CHANNEL_MAX; ch++)
		data->ch_data[ch] = ((ch_data[ch].inited == SPRD_ADC_INIT_MAGIC)
				     ? ch_data[ch] : ch_data_def);
}

static void ump9620_ch_data_init(struct sc27xx_adc_data *data)
{
	int ch;
	struct sprd_adc_channel_data ch_data_def =
		CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
			     RATIO(1, 1), RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060));

	struct sprd_adc_channel_data ch_data[SC27XX_ADC_CHANNEL_MAX] = {
		[0] = CH_DATA_INIT(1, TWO_CELL_VBAT_DET_GRAPH, 0, 0,
				   RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[5] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0x9, RATIO(1, 1),
				   RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[7] = CH_DATA_INIT(2, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				   RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[9] = CH_DATA_INIT(2, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				   RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[10] = CH_DATA_INIT(3, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				    RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[11] = CH_DATA_INIT(0, TWO_CELL_BIG_GRAPH, 0, 0,
				    RATIO(1, 1), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[13] = CH_DATA_INIT(1, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				    RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[14] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
				    RATIO(68, 900), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[15] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
				    RATIO(1, 3), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[19] = CH_DATA_INIT(3, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				    RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[21] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
				    RATIO(3, 8), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[22] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
				    RATIO(3, 8), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[23] = CH_DATA_INIT(0, TWO_CELL_SMALL_GRAPH, 0, 0,
				    RATIO(3, 8), RATIO(1, 1), RATIO(1, 1), RATIO(1, 1)),
		[30] = CH_DATA_INIT(3, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				    RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
		[31] = CH_DATA_INIT(3, TWO_CELL_SMALL_GRAPH, 0, 0, RATIO(1, 1),
				    RATIO(1000, 1955), RATIO(1000, 2600), RATIO(1000, 4060)),
	};

	for (ch = 0; ch < SC27XX_ADC_CHANNEL_MAX; ch++)
		data->ch_data[ch] = ((ch_data[ch].inited == SPRD_ADC_INIT_MAGIC)
				     ? ch_data[ch] : ch_data_def);
}

static void sc27xx_adc_regs_dump(struct sc27xx_adc_data *data, int channel, int scale)
{
	static u64 count;
	u32 module_en, adc_clk_en, adc_int_ctl, adc_int_raw, adc_ctl, adc_ch_cfg;

	regmap_read(data->regmap, GET_REG_ADDR(data, REG_MODULE_EN), &module_en);
	regmap_read(data->regmap, GET_REG_ADDR(data, REG_CLK_EN), &adc_clk_en);
	regmap_read(data->regmap, data->base + SC27XX_ADC_INT_CLR, &adc_int_ctl);
	regmap_read(data->regmap, data->base + SC27XX_ADC_INT_RAW, &adc_int_raw);
	regmap_read(data->regmap, data->base + SC27XX_ADC_CTL, &adc_ctl);
	regmap_read(data->regmap, data->base + SC27XX_ADC_CH_CFG, &adc_ch_cfg);

	SPRD_ADC_ERR("regs_dump[%llu]->channel: %d, scale: %d, module_en: 0x%x, adc_clk_en: 0x%x,"
		     " adc_int_ctl: 0x%x, adc_int_raw: 0x%x, adc_ctl: 0x%x, adc_ch_cfg: 0x%x\n",
		     count++, channel, scale, module_en, adc_clk_en, adc_int_ctl,
		     adc_int_raw, adc_ctl, adc_ch_cfg);
}

static u32 sprd_adc_get_isen(void *pri, int ch, bool enable)
{
	struct sc27xx_adc_data *data = (struct sc27xx_adc_data *)pri;

	if (!enable)
		return 0;

	return (data->ch_data[ch].isen_info >> 1);
}

static int sprd_adc_isen_enable(struct sc27xx_adc_data *data, int channel)
{
	int i, ret;
	u32 reg_addr, mask, val, read_val, offset;
	bool isen_support = data->ch_data[channel].isen_info & 0x1;

	if (!isen_support)
		return 0;

	for (i = REG_ISEN0; i <= REG_ISEN3; i++) {
		if (data->var_data->reg_list[i].inited != SPRD_ADC_INIT_MAGIC)
			continue;

		reg_addr = GET_REG_ADDR(data, i);
		mask = data->var_data->reg_list[i].mask;
		offset = data->var_data->reg_list[i].offset;
		val = ((data->var_data->reg_list[i].get_setval != NULL)
		       ? (data->var_data->reg_list[i].get_setval(data, channel, true) << offset)
		       : data->var_data->reg_list[i].mask);
		val = (data->var_data->reg_list[i].reverse ? 0 : val);
		ret = regmap_update_bits(data->regmap, reg_addr, mask, val);
		ret = regmap_read(data->regmap, reg_addr, &read_val);
		SPRD_ADC_DBG("isen_enable: reg 0x%x, mask: 0x%x, val: 0x%x, read_val: 0x%x\n",
			     reg_addr, mask, val, read_val);
		if (ret) {
			SPRD_ADC_ERR("isen config err: reg[%d], ret %d\n", i, ret);
			return ret;
		}
	}
	udelay(500);

	return 0;
}

static int sprd_adc_isen_diable(struct sc27xx_adc_data *data, int channel)
{
	int i, ret;
	u32 reg_addr, mask, val, read_val;
	bool isen_support = data->ch_data[channel].isen_info & 0x1;

	if (!isen_support)
		return 0;

	for (i = REG_ISEN3; i >= REG_ISEN0; i--) {
		if (data->var_data->reg_list[i].inited != SPRD_ADC_INIT_MAGIC)
			continue;

		reg_addr = GET_REG_ADDR(data, i);
		mask = data->var_data->reg_list[i].mask;
		val = ((data->var_data->reg_list[i].get_setval != NULL)
		       ? data->var_data->reg_list[i].get_setval(data, channel, false) : 0);
		val = (data->var_data->reg_list[i].reverse ? mask : val);
		ret = regmap_update_bits(data->regmap, reg_addr, mask, val);
		ret = regmap_read(data->regmap, reg_addr, &read_val);
		SPRD_ADC_DBG("isen_diable: reg 0x%x, mask: 0x%x, val: 0x%x, read_val: 0x%x\n",
			     reg_addr, mask, val, read_val);
		if (ret) {
			SPRD_ADC_ERR("isen config err: reg[%d], ret %d\n", i, ret);
			return ret;
		}
	}
	udelay(500);

	return 0;
}


static int sprd_adc_data_average(int *vals, int len)
{
	int i, sum = 0;

	for (i = 0; i < len; i++)
		sum += vals[i];
	return DIV_ROUND_CLOSEST(sum, len);
}

static int compare_val(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static int sprd_adc_get_val_with_sw_filter(struct sc27xx_adc_data *data, int ch)
{
	int data_buf[ADC_MESURE_NUMBER_SW] = { 0 }, ret = 0, num = ADC_MESURE_NUMBER_SW;
	int count, result;
	unsigned int  rawdata;

	for (count = 0; count < ADC_MESURE_NUMBER_SW; count++) {
		ret |= regmap_read(data->regmap, data->base + SC27XX_ADC_DATA, &rawdata);
		rawdata &= SC27XX_ADC_DATA_MASK;
		data_buf[count] = rawdata;
		udelay(10);
		if (ret)
			return -EINVAL;
	}

	sort(data_buf, ADC_MESURE_NUMBER_SW, sizeof(int), compare_val, NULL);
	result = sprd_adc_data_average(&data_buf[num / 5], (num - num * 2 / 5));

	for (count = 0; count < ADC_MESURE_NUMBER_SW; count++)
		SPRD_ADC_DBG("data_buf[%d]=%d ", count, data_buf[count]);
	SPRD_ADC_DBG("result=%d\n", result);

	return result;
}

static int sc27xx_adc_read(struct sc27xx_adc_data *data, int channel,
			   int scale, int *val)
{
	int ret = 0, ret_volref = 0, sample_num_sw;
	u32 rawdata = 0, tmp, status, scale_shift, scale_mask;
	bool filter_sw = data->ch_data[channel].filter_info & 0x1;
	int sample_num_hw = data->ch_data[channel].filter_info >> 1;

	if (data->pm_data.pm_ctl_support && data->pm_data.dev_suspended) {
		SPRD_ADC_ERR("adc_exp: adc clk26 bas been closed, ignore.\n");
		return -EBUSY;
	}

	SPRD_ADC_DBG("ch_data[%d]: scale %d, graph %d, filter_info 0x%x, isen_info 0x%x\n",
		     channel, data->ch_data[channel].scale, data->ch_data[channel].graph_index,
		     data->ch_data[channel].filter_info,
		     data->ch_data[channel].isen_info);

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

	ret = sprd_adc_isen_enable(data, channel);
	if (ret)
		goto unlock_adc;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
				 SC27XX_ADC_EN, SC27XX_ADC_EN);
	if (ret)
		goto unlock_adc;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_INT_CLR,
				 SC27XX_ADC_IRQ_CLR, SC27XX_ADC_IRQ_CLR);
	if (ret)
		goto disable_adc;

	/* Configure the channel id and scale */
	scale_shift = data->var_data->reg_list[REG_SCALE].offset;
	scale_mask = data->var_data->reg_list[REG_SCALE].mask;
	tmp = (scale << scale_shift) & scale_mask;
	tmp |= channel & SC27XX_ADC_CHN_ID_MASK;
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CH_CFG,
				 SC27XX_ADC_CHN_ID_MASK |
				 scale_mask,
				 tmp);
	if (ret)
		goto disable_adc;

	/* Select 12bit conversion mode, and only sample 1 time */
	tmp = SC27XX_ADC_12BIT_MODE;
	sample_num_sw = (filter_sw ? ADC_MESURE_NUMBER_SW - 1 : 0);
	sample_num_hw = ((sample_num_hw > 0) ? sample_num_hw : ADC_MESURE_NUMBER_HW_DEF);
	tmp |= (sample_num_sw << SC27XX_ADC_RUN_NUM_SHIFT) & SC27XX_ADC_RUN_NUM_MASK;
	tmp |= (sample_num_hw << SC27XX_ADC_AVERAGE_SHIFT) & SC27XX_ADC_AVERAGE_MASK;
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_ADC_CTL,
				 SC27XX_ADC_RUN_NUM_MASK | SC27XX_ADC_12BIT_MODE |
				 SC27XX_ADC_AVERAGE_MASK,
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

	if (filter_sw) {
		rawdata = sprd_adc_get_val_with_sw_filter(data, channel);
	} else {
		ret = regmap_read(data->regmap, data->base + SC27XX_ADC_DATA, &rawdata);
		rawdata &= SC27XX_ADC_DATA_MASK;
	}
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
	ret = sprd_adc_isen_diable(data, channel);

	hwspin_unlock_raw(data->hwlock);

	if (!ret)
		*val = rawdata;

	return ret;
}

static int sprd_adc_calculate_volt_by_graph(struct sc27xx_adc_data *data, int channel,
					    int scale, int raw_adc)
{
	int tmp;
	int graph_index = data->ch_data[channel].graph_index;
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

	ratio = data->ch_data[channel].ratio[scale];
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

static int sprd_adc_ch_data_encode(struct sc27xx_adc_data *data, int ch)
{
	int scale = data->ch_data[ch].scale & 0xff;
	int graph_index = data->ch_data[ch].graph_index & 0xff;
	int isen_info = data->ch_data[ch].isen_info & 0xff;
	int filter_info = data->ch_data[ch].filter_info & 0xff;

	return (scale | (graph_index << 8) | (filter_info << 16) | (isen_info << 24));
}

static void sprd_adc_ch_data_decode(struct sc27xx_adc_data *data, int ch, int val)
{
	data->ch_data[ch].scale = (val & 0xff);
	data->ch_data[ch].graph_index = ((val >> 8) & 0xff);
	data->ch_data[ch].filter_info = ((val >> 16) & 0xff);
	data->ch_data[ch].isen_info = ((val >> 24) & 0xff);
}

static int sc27xx_adc_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct sc27xx_adc_data *data = iio_priv(indio_dev);
	int scale = data->ch_data[chan->channel].scale;
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
		*val = sprd_adc_ch_data_encode(data, chan->channel);
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

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		sprd_adc_ch_data_decode(data, chan->channel, val);
		return 0;

	default:
		return -EINVAL;
	}
}

static int sprd_adc_pm_handle(struct sc27xx_adc_data *sc27xx_data, bool enable)
{

	return regmap_update_bits(sc27xx_data->pm_data.pm_regmap,
				 sc27xx_data->pm_data.clk26m_vote_reg,
				 sc27xx_data->pm_data.clk26m_vote_reg_mask,
				 enable ? sc27xx_data->pm_data.clk26m_vote_reg_mask : 0);
}

static int sc27xx_adc_enable(struct sc27xx_adc_data *data)
{
	int ret;
	u32 reg_addr, mask;

	reg_addr = GET_REG_ADDR(data, REG_MODULE_EN);
	mask = data->var_data->reg_list[REG_MODULE_EN].mask;
	ret = regmap_update_bits(data->regmap, reg_addr, mask, mask);
	if (ret)
		return ret;

	/* Enable ADC work clock */
	reg_addr = GET_REG_ADDR(data, REG_CLK_EN);
	mask = data->var_data->reg_list[REG_CLK_EN].mask;
	ret = regmap_update_bits(data->regmap, reg_addr, mask, mask);
	if (ret)
		goto disable_adc;

	return 0;

disable_adc:
	reg_addr = GET_REG_ADDR(data, REG_MODULE_EN);
	mask = data->var_data->reg_list[REG_MODULE_EN].mask;
	regmap_update_bits(data->regmap, reg_addr, mask, 0);

	return ret;
}

static void sc27xx_adc_disable(void *_data)
{
	struct sc27xx_adc_data *data = _data;
	u32 reg_addr, mask;

	/* Disable ADC work clock and controller clock */
	reg_addr = GET_REG_ADDR(data, REG_CLK_EN);
	mask = data->var_data->reg_list[REG_CLK_EN].mask;
	regmap_update_bits(data->regmap, reg_addr, mask, 0);

	reg_addr = GET_REG_ADDR(data, REG_MODULE_EN);
	mask = data->var_data->reg_list[REG_MODULE_EN].mask;
	regmap_update_bits(data->regmap, reg_addr, mask, 0);
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
	sc27xx_data->pm_data.pm_regmap =
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

static int sprd_adc_ch_data_init(struct sc27xx_adc_data *data)
{
	struct device_node *np = data->dev->of_node;
	int size, ret, ch, ch_data_val, i;
	u32 *ch_data_overide;

	data->var_data->ch_data_init(data);

	size = of_property_count_elems_of_size(np, "ch_data_overide", sizeof(u32));
	if (size <= 0)
		return 0;

	if (size % 2) {
		SPRD_ADC_ERR("Pair of ch data err!\n");
		return -EINVAL;
	}

	ch_data_overide = devm_kcalloc(data->dev, size, sizeof(u32), GFP_KERNEL);
	if (!ch_data_overide)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "ch_data_overide", ch_data_overide, size);
	if (ret < 0) {
		SPRD_ADC_ERR("Failed to read ch data from dt: %d\n", ret);
		return ret;
	}

	for (i = 0; i < size; i += 2) {
		ch = ch_data_overide[i];
		ch_data_val = ch_data_overide[i+1];
		sprd_adc_ch_data_decode(data, ch, ch_data_val);
	}

	devm_kfree(data->dev, ch_data_overide);

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

	/* ADC channel scales calibration from nvmem device */
	ret = sprd_adc_graphs_calibrate(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("failed to calib graphs from nvmem\n");
		return ret;
	}

	ret = sprd_adc_ch_data_init(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("ch data init err.\n");
		return ret;
	}

	ret = sc27xx_adc_pm_init(sc27xx_data);
	if (ret) {
		SPRD_ADC_ERR("adc pm init err.\n");
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

static const struct sc27xx_adc_variant_data sc2720_data = {
	.pmic_type = SC2720_ADC,
	.glb_reg_base = 0xc00,
	.adc_reg_base_offset = 0x4,
	.reg_list = regs_sc2720,
	.calib_graphs_index = {ONE_CELL_BIG_GRAPH, ONE_CELL_SMALL_GRAPH, SPRD_ADC_INVALID_DATA},
	.ch_data_init = sc2720_ch_data_init,
};

static const struct sc27xx_adc_variant_data sc2721_data = {
	.pmic_type = SC2721_ADC,
	.glb_reg_base = 0xc00,
	.adc_reg_base_offset = 0x0,
	.reg_list = regs_sc2721,
	.calib_graphs_index = {ONE_CELL_BIG_GRAPH, ONE_CELL_SMALL_GRAPH, SPRD_ADC_INVALID_DATA},
	.ch_data_init = sc2721_ch_data_init,
};

static const struct sc27xx_adc_variant_data sc2730_data = {
	.pmic_type = SC2730_ADC,
	.glb_reg_base = 0x1800,
	.adc_reg_base_offset = 0x4,
	.reg_list = regs_sc2730,
	.calib_graphs_index = {ONE_CELL_BIG_GRAPH, ONE_CELL_SMALL_GRAPH, SPRD_ADC_INVALID_DATA},
	.ch_data_init = sc2730_ch_data_init,
};

static const struct sc27xx_adc_variant_data sc2731_data = {
	.pmic_type = SC2731_ADC,
	.glb_reg_base = 0xc00,
	.adc_reg_base_offset = 0x0,
	.reg_list = regs_sc2731,
	.calib_graphs_index = {ONE_CELL_BIG_GRAPH, ONE_CELL_SMALL_GRAPH, SPRD_ADC_INVALID_DATA},
	.ch_data_init = sc2731_ch_data_init,
};

static const struct sc27xx_adc_variant_data ump9620_data = {
	.pmic_type = UMP9620_ADC,
	.glb_reg_base = 0x2000,
	.adc_reg_base_offset = 0x4,
	.reg_list = regs_ump9620,
	.calib_graphs_index = {TWO_CELL_BIG_GRAPH, TWO_CELL_SMALL_GRAPH, TWO_CELL_VBAT_DET_GRAPH,
			       SPRD_ADC_INVALID_DATA},
	.ch_data_init = ump9620_ch_data_init,
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
