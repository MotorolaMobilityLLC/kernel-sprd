#ifndef _APDU_R3P0_H
#define _APDU_R3P0_H

/* apdu channel reg offset */
#define APDU_TEE_OFFSET	0x0
#define APDU_REE_OFFSET	0x100
#define APDU_CP0_OFFSET	0x200
#define APDU_CP1_OFFSET	0x300

#define APDU_STATUS0		(APDU_REE_OFFSET + 0x0)
#define APDU_STATUS1		(APDU_REE_OFFSET + 0x4)
#define APDU_WATER_MARK		(APDU_REE_OFFSET + 0x8)
#define APDU_INT_EN		(APDU_REE_OFFSET + 0xc)
#define APDU_INT_RAW		(APDU_REE_OFFSET + 0x10)
#define APDU_INT_MASK		(APDU_REE_OFFSET + 0x14)
#define APDU_INT_CLR		(APDU_REE_OFFSET + 0x18)
#define APDU_CNT_CLR		(APDU_REE_OFFSET + 0x1c)
#define APDU_TX_FIFO		(APDU_REE_OFFSET + 0x20)
#define APDU_RX_FIFO		(APDU_REE_OFFSET + 0x60)
#define APDU_INF_INT_EN		(APDU_REE_OFFSET + 0xb0)
#define APDU_INF_INT_RAW	(APDU_REE_OFFSET + 0xb4)
#define APDU_INF_INT_MASK	(APDU_REE_OFFSET + 0xb8)
#define APDU_INF_INT_CLR	(APDU_REE_OFFSET + 0xbc)

#define APDU_FIFO_TX_POINT_OFFSET	8
#define APDU_FIFO_RX_OFFSET		16
#define APDU_FIFO_RX_POINT_OFFSET	24
#define APDU_FIFO_LEN_MASK		GENMASK(7, 0)
#define APDU_CNT_LEN_MASK		GENMASK(15, 0)

#define APDU_INT_BITS	(BIT(9) | BIT(8) | BIT(5) | BIT(4) | BIT(3) \
				| BIT(2) | BIT(1) | BIT(0))
#define APDU_INT_RX_EMPTY_TO_NOEMPTY	BIT(0)
#define APDU_INT_MED_WR_DONE		BIT(8)
#define APDU_INT_MED_WR_ERR		BIT(9)

#define APDU_INF_INT_BITS		(0xffffffff)
#define APDU_INF_INT_GET_ATR		BIT(31)
#define APDU_INF_INT_ISESYS_INT_FAIL	BIT(30)
#define APDU_INF_INT_APDU_NO_SEC	BIT(29)
#define APDU_INF_INT_HARD_FAULT_STATUS	GENMASK(18, 14)
#define APDU_INF_INT_SELF_CHECK_STATUS	GENMASK(13, 10)
#define APDU_INF_INT_ATTACK		GENMASK(9, 0)
#define APDU_INF_INT_FAULT	(APDU_INF_INT_ATTACK | \
				APDU_INF_INT_SELF_CHECK_STATUS | \
				APDU_INF_INT_HARD_FAULT_STATUS | \
				APDU_INF_INT_APDU_NO_SEC | \
				APDU_INF_INT_ISESYS_INT_FAIL)

#define APDU_DRIVER_NAME	"apdu"

#define APDU_POWER_ON_CHECK_TIMES (10)

/* packet was alinged with 4 byte should not exceed max size+ pad byte */
#define APDU_TX_MAX_SIZE	(44 * 1024 + 4)
#define APDU_RX_MAX_SIZE	(5120 + 4)
#define APDU_ATR_DATA_MAX_SIZE	(32)
#define MED_INFO_MAX_BLOCK	(5)
/* MED_INFO_MAX_NUM groups offset and length info (each 2 word) */
#define APDU_MED_INFO_SIZE	(MED_INFO_MAX_BLOCK * 2)
/* 8--sizeof med_info_type in words*/
#define APDU_MED_INFO_PARSE_SZ	(MED_INFO_MAX_BLOCK * 8)
/* save (ISE_ATTACK_BUFFER_SIZE -1) attack status, 1 for message header */
#define ISE_ATTACK_BUFFER_SIZE	(33)
#define APDU_FIFO_LENGTH	128
#define APDU_FIFO_SIZE		(APDU_FIFO_LENGTH * 4)
#define APDU_MAGIC_NUM		0x55AA

#define APDU_RESET			_IO('U', 0)
#define APDU_CLR_FIFO			_IO('U', 1)
#define APDU_SET_WATER			_IO('U', 2)
#define APDU_CHECK_CLR_FIFO_DONE	_IO('U', 3)
#define APDU_CHECK_MED_WR_ERROR_STATUS	_IO('U', 4)
#define APDU_CHECK_FAULT_STATUS		_IO('U', 5)
#define APDU_GET_ATR_INF		_IO('U', 6)
#define APDU_SET_MED_HIGH_ADDR		_IO('U', 7)
#define APDU_MED_REWRITE_INFO_PARSE	_IO('U', 8)
#define APDU_NORMAL_PWR_ON_CFG		_IO('U', 9)
#define APDU_ENTER_APDU_LOOP		_IO('U', 10)
#define APDU_FAULT_INT_RESOLVE_DONE	_IO('U', 11)
#define APDU_NORMAL_POWER_ON_ISE	_IO('U', 12)
#define APDU_SOFT_RESET_ISE	_IO('U', 13)
#define APDU_ION_MAP_MEDDDR_IN_KERNEL	_IO('U', 14)
#define APDU_ION_UNMAP_MEDDDR_IN_KERNEL	_IO('U', 15)
#define APDU_POWEROFF_ISE_AON_DOMAIN	_IO('U', 16)
#define APDU_SOFT_RESET_ISE_AON_DOMAIN	_IO('U', 17)
#define APDU_DUMP_MEDDDR_COUNTER_DATA	_IO('U', 18)
#define APDU_SAVE_MEDDDR_COUNTER_DATA	_IO('U', 19)
#define APDU_LOAD_MEDDDR_DATA_IN_KERNEL	_IO('U', 20)
#define APDU_SAVE_MEDDDR_DATA_IN_KERNEL	_IO('U', 21)
#define APDU_SET_CURRENT_SLOT_IN_KERNEL	_IO('U', 22)
#define APDU_NORMAL_POWER_DOWN_ISE _IO('U', 26)

#define ISE_BUSY_STATUS		0x60
#define AP_WAIT_TIMES		(20)
#define DIV_CEILING(a, b)	(((a) + ((b) - 1)) / (b))

#define APDU_NETLINK		30
#define APDU_USER_PORT		100
#define DDR_BASE		(0x80000000)
#define MED_DDR_SIZE		(0x1C00000)

#define MESSAGE_HEADER_MED_INFO	0x5AA55AA5
#define MESSAGE_HEADER_FAULT	0x6BB66BB6

/* MAX_WAIT_TIME milliseconds --ISE system hangs up and exits*/
#define MAX_WAIT_TIME		(5000)

#define ISE_AON_RAM_CFG		0x80
//default is 1 , software memory shutdown control, high active
#define REG_RAM_PD_ISE_AON_BIT (BIT(2))

//force close rco 150M PD for timing violation before
//relase reset signal(B8C) 10us, default is 0
#define RCO150M_REL_CFG 0x9F0
//RCO150M force OFF; 0:RCO150M not force OFF  1:RCO150M force OFF
#define RCO150M_RFC_OFF	(BIT(1))

#define ADM_SOFT_RST	0xB8C
//ise aon domain soft reset, default is 1,
//should set as 0 when reset ise aon domain
//0 keep aon domain in normal mode, 1 reset ise aon domain
#define ISE_AON_SOFT_RESET_BIT	(BIT(5))

#define PD_ISE_CFG_0    0x3f0
//enable power domain "PD_ISE" automaticially power off when ise into deep sleep
//0:power domain SHUTDOWN by "PD_ISE_FORCE_SHUTDOWN",
//default is 0 when in force mode
//1:power domain automatically SHUTDOWN
#define PD_ISE_FORCE_SHUTDOWN_EN_BIT (BIT(24))
//when PD_ISE_FORCE_SHUTDOWN_EN_BIT is 0,
//we can set SHUTDOWN bit as 0 to poweron ise
#define PD_ISE_FORCE_SHUTDOWN_BIT (BIT(25))

#define SOFT_RST_SEL_0 0xBA8
#define SOFT_RST_SEL_ISE_BIT (BIT(27))

#define FORCE_DEEP_SLEEP_CFG_0 0x818
#define ISE_FORCE_DEEP_SLEEP_REG (BIT(5))

#define AON_CALI_RCO 0xCA4
#define AON_RF_CALI_RCO (BIT(4))
#define AON_RF_CHECK_RCO (BIT(5))

#define AON_APB_EB1 0x4
#define AON_PIN_REG_ENABLE (BIT(11))

#define SP_SYS_SOFT_RST 0x90
#define SP_SYS_SOFT_RST_BIT (BIT(4))
#define SP_CORE_SOFT_RST_BIT (BIT(0))

#define GATE_EN_SEL6_CFG 0x68
//clock gating enable select.
//0: soft register control
//1: hw(pmu) auto control
#define CGM_XBUF_26M_ISE_AUTO_GATE_SEL (BIT(22))
#define CGM_XBUF_2M_ISE_AUTO_GATE_SEL (BIT(23))

#define GATE_EN_SW_CTL6_CFG 0x8c
//clock gating enable sw control
#define CGM_XBUF_26M_ISE_FORCE_EN (BIT(22))
#define CGM_XBUF_2M_ISE_FORCE_EN (BIT(23))


#define ISE_SYS_SOFT_RST_0  0xB98
#define ISE_SYS_SOFT_RST  0xBa8
//ise sys soft reset.
//0 keep ise sys in normal mode; 1 reset ise sys
#define ISE_SYS_SOFT_RST_BITS       (BIT(27))


#define MEDDDR_RESERVED_ADDRESS (0xBC000000)
#define MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS (0x200000)
#define MEDDDR_MAX_SIZE (0x1000000)
#define MEDDDR_COUNTER_AREA_MAX_SIZE (0x600) //0x24
#define ISEDATA_DEV_PATH "dev/block/by-name/isedata"

struct sprd_apdu_device {
	struct device *dev;
	struct miscdevice misc;
	wait_queue_head_t read_wq;
	int rx_done;
	void *tx_buf;
	void *rx_buf;
	u32 *atr;
	u32 *med_rewrite;
	u32 *ise_fault_buf;

	/* synchronize access to our device file */
	struct mutex mutex;
	void __iomem *base;
	void __iomem *pub_ise_base;
	void __iomem *pmu_base;
	void __iomem *aon_clock_base;
	void __iomem *aon_rf_base;

	void *medddr_address;
	int slot;

	struct regmap *pub_reg_base;
	int irq;

	u32 med_wr_done;
	u32 med_wr_error;
	u32 ise_fault_status;
	u32 ise_fault_point;
	u32 ise_fault_allow_to_send_flag;
	u32 atr_rcv_status;
	u32 pub_ise_reg_offset;
	u32 pub_ise_bit_offset;
};

struct med_info_type {
	u32 ap_side_data_offset;
	u32 ap_side_data_length;
	u32 level1_rng_offset;
	u32 level1_rng_length;
	u32 level2_rng_offset;
	u32 level2_rng_length;
	u32 level3_rng_offset;
	u32 level3_rng_length;
};

extern struct net init_net;

#endif

