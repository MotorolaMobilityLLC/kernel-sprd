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

