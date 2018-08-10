#include <misc/wcn_bus.h>

#include "bus_common.h"
#include "sdiohal.h"

/* for debug,module not need it */
static struct mchn_ops_t AT_TX_OPS;
static struct mchn_ops_t AT_RX_OPS;
static struct mchn_ops_t LOOPCHECK_RX_OPS;
static struct mchn_ops_t ASSERT_RX_OPS;
static struct mchn_ops_t RING_RX_OPS;
static unsigned int (*at_tx_f)(void *addr);

static int sdiohal_register_pt_rx_process(unsigned int type,
					  unsigned int subtype, void *func)
{
	struct mchn_ops_t *debug_ops_rx;

	if ((type == 3) && (subtype == 0)) {
		RING_RX_OPS.channel = 15;
		RING_RX_OPS.hif_type = 0;
		RING_RX_OPS.inout = 0;
		RING_RX_OPS.pool_size = 1;
		RING_RX_OPS.pop_link = func;
		debug_ops_rx = &RING_RX_OPS;

	} else if ((type == 3) && (subtype == 1)) {
		LOOPCHECK_RX_OPS.channel = 12;
		LOOPCHECK_RX_OPS.hif_type = 0;
		LOOPCHECK_RX_OPS.inout = 0;
		LOOPCHECK_RX_OPS.pool_size = 1;
		LOOPCHECK_RX_OPS.pop_link = func;
		debug_ops_rx = &LOOPCHECK_RX_OPS;

	} else if ((type == 3) && (subtype == 2)) {
		AT_RX_OPS.channel = 13;
		AT_RX_OPS.hif_type = 0;
		AT_RX_OPS.inout = 0;
		AT_RX_OPS.pool_size = 1;
		AT_RX_OPS.pop_link = func;
		debug_ops_rx = &AT_RX_OPS;

	} else if ((type == 3) && (subtype == 3)) {
		ASSERT_RX_OPS.channel = 14;
		ASSERT_RX_OPS.hif_type = 0;
		ASSERT_RX_OPS.inout = 0;
		ASSERT_RX_OPS.pool_size = 1;
		ASSERT_RX_OPS.pop_link = func;
		debug_ops_rx = &ASSERT_RX_OPS;

	} else {
		pr_err("sdio register rx error: type[%d]sub[%d]\n",
			type, subtype);
		return -1;
	}

	sprdwcn_bus_chn_init(debug_ops_rx);

	return 0;
}

static int mdbg_tx_cb(int channel, struct mbuf_t *head,
		      struct mbuf_t *tail, int num)
{
	int i;
	struct mbuf_t *mbuf_ptr;

	for (i = 0, mbuf_ptr = head; i < num; i++, mbuf_ptr = mbuf_ptr->next) {
		at_tx_f(mbuf_ptr->buf);
		mbuf_ptr->buf = NULL;
	}

	sprdwcn_bus_list_free(channel, head, tail, num);

	return 0;
}

static int sdiohal_register_pt_tx_release(unsigned int type,
					  unsigned int subtype, void *func)
{
	if ((type == 3) && (subtype == 2)) {
		AT_TX_OPS.channel = 0;
		AT_TX_OPS.hif_type = 0;
		AT_TX_OPS.inout = 1;
		AT_TX_OPS.pool_size = 5;
		AT_TX_OPS.pop_link = mdbg_tx_cb;
		at_tx_f = func;

	} else {
		pr_err("sdio register tx error: type[%d]sub[%d]\n",
			type, subtype);
		return -1;
	}

	sprdwcn_bus_chn_init(&AT_TX_OPS);

	return 0;
}

static int sdiohal_pt_write(void *buf, unsigned int len, unsigned int type,
						unsigned int subtype)
{
	struct mbuf_t *head, *tail;
	int num = 1;
	unsigned char *full_buf = NULL;

	if (!((type == 3) && (subtype == 2))) {
		pr_err("%s error\n", __func__);
		return -1;
	}

	full_buf = kmalloc(len + 4, GFP_KERNEL);
	memcpy(full_buf + 4, buf, len);
	kfree(buf);

	if (!sprdwcn_bus_list_alloc(AT_TX_OPS.channel, &head, &tail, &num)) {
		head->buf = full_buf;
		head->len = len;
		head->next = NULL;
		sprdwcn_bus_push_list(AT_TX_OPS.channel, head, tail, num);
	}

	return 0;

}

/* RX  : for release buf, need achieve it */
static int sdiohal_pt_read_release(unsigned int fifo_id)
{
	return 0;
}

static int sdiohal_driver_register(void)
{
	return 0;
}

static void sdiohal_driver_unregister(void)
{

}

static int sdio_preinit(void)
{
	sdiohal_init();
	return 0;
}

static void sdio_preexit(void)
{
	sdiohal_exit();
}

static int sdio_buf_list_alloc(int chn, struct mbuf_t **head,
			       struct mbuf_t **tail, int *num)
{
	return buf_list_alloc(chn, head, tail, num);
}

static int sdio_buf_list_free(int chn, struct mbuf_t *head,
			      struct mbuf_t *tail, int num)
{
	return buf_list_free(chn, head, tail, num);
}

static int sdio_list_push(int chn, struct mbuf_t *head,
			  struct mbuf_t *tail, int num)
{
	return sdiohal_list_push(chn, head, tail, num);
}

static int sdio_chn_init(struct mchn_ops_t *ops)
{
	return bus_chn_init(ops, HW_TYPE_SDIO);
}

static int sdio_chn_deinit(struct mchn_ops_t *ops)
{
	return bus_chn_deinit(ops);
}

static int sdio_direct_read(unsigned int addr,
				void *buf, unsigned int len)
{
	return sdiohal_dt_read(addr, buf, len);
}

static int sdio_direct_write(unsigned int addr,
				void *buf, unsigned int len)
{
	return sdiohal_dt_write(addr, buf, len);
}

static int sdio_readbyte(unsigned int addr, unsigned char *val)
{
	return sdiohal_aon_readb(addr, val);
}

static int sdio_writebyte(unsigned int addr, unsigned char val)
{
	return sdiohal_aon_writeb(addr, val);
}

static unsigned int sdio_get_carddump_status(void)
{
	return sdiohal_get_carddump_status();
}

static void sdio_set_carddump_status(unsigned int flag)
{
	return sdiohal_set_carddump_status(flag);
}

static unsigned long long sdio_get_rx_total_cnt(void)
{
	return sdiohal_get_rx_total_cnt();
}

static int sdio_runtime_get(void)
{
	return sdiohal_runtime_get();
}

static int sdio_runtime_put(void)
{
	return sdiohal_runtime_put();
}

static int sdio_rescan(void)
{
	return sdiohal_scan_card();
}

static void sdio_register_rescan_cb(void *func)
{
	return sdiohal_register_scan_notify(func);
}

static void sdio_remove_card(void)
{
	return sdiohal_remove_card();
}

static struct sprdwcn_bus_ops sdiohal_bus_ops = {
	.preinit = sdio_preinit,
	.deinit = sdio_preexit,
	.chn_init = sdio_chn_init,
	.chn_deinit = sdio_chn_deinit,
	.list_alloc = sdio_buf_list_alloc,
	.list_free = sdio_buf_list_free,
	.push_list = sdio_list_push,
	.direct_read = sdio_direct_read,
	.direct_write = sdio_direct_write,
	.readbyte = sdio_readbyte,
	.writebyte = sdio_writebyte,
	.read_l = sdiohal_readl,
	.write_l = sdiohal_writel,

	.get_carddump_status = sdio_get_carddump_status,
	.set_carddump_status = sdio_set_carddump_status,
	.get_rx_total_cnt = sdio_get_rx_total_cnt,

	.runtime_get = sdio_runtime_get,
	.runtime_put = sdio_runtime_put,

	/* v3 temp */
	.register_rescan_cb = sdio_register_rescan_cb,
	.rescan = sdio_rescan,
	.remove_card = sdio_remove_card,

	/* v2 */
	.register_pt_rx_process = sdiohal_register_pt_rx_process,
	.register_pt_tx_release = sdiohal_register_pt_tx_release,
	.pt_write = sdiohal_pt_write,
	.pt_read_release = sdiohal_pt_read_release,
	.driver_register = sdiohal_driver_register,
	.driver_unregister = sdiohal_driver_unregister,
};

void module_bus_init(void)
{
	module_ops_register(&sdiohal_bus_ops);
}
EXPORT_SYMBOL(module_bus_init);

