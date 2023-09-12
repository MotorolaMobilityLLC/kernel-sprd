#ifndef __SPRDWL_TXRX_BUF_MM_H__
#define __SPRDWL_TXRX_BUF_MM_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "core_sc2355.h"
#include "msg.h"


enum {
	LOC_BUFF_FREE,
	LOC_TX_INTF,
	LOC_RX_INTF,
};

enum {
	SPRDWL_MEMORY_FREE,
	SPRDWL_MEMORY_ALLOC = 0x5a,
};

struct sprdwl_buf_node {
	u8 flag;
	u8 ctxt_id;
	u8 location;
	u8 rsve;
	union {
		struct sprdwl_buf_node *next;
		void *addr;
	};
	u8 buf[0];
} __packed;

#define TXRX_BUF_BLOCK_TYPE               (1)
#define TXRX_BUF_SINGLE_TYPE              (0)
#define TXRX_TX_BUF_MAX_NUM               (1024)
struct sprdwl_buf_mm {
	struct sprdwl_buf_node *head;
	struct sprdwl_buf_node *tail;
	int max_num;
	int free_num;
	int len;
	int type;
	spinlock_t freelock;
	int addr_count;
	union {
		unsigned long buf_addr[0][2];
		void *addr[0];
	};
};

#define SPRDWL_ALIGN(len, align) ((((len)+(align)-1)/(align)) * (align))
#define SPRDWL_SIPC_ALIGN		4
#define SPRDWL_BUF_SET_LOC(node, loc)	((node)->location = (loc))



extern struct sprdwl_buf_mm *sprdwl_buf_mm_init(struct sprdwl_buf_mm *mm,
						u8 *buffer, int len, int blen);
extern struct sprdwl_buf_node *sprdwl_buf_mm_alloc(struct sprdwl_buf_mm *mm);
extern int sprdwl_buf_mm_free(struct sprdwl_buf_mm *mm,
			      struct sprdwl_buf_node *node);
extern struct sprdwl_buf_node *sprdwl_alloc_tx_buf(void);
extern void sprdwl_free_tx_buf(struct sprdwl_buf_node *node);
extern int sprdwl_get_tx_buf_len(void);
extern int sprdwl_get_tx_buf_num(void);
extern int sprdwl_get_tx_buf_free_num(void);
extern int sprdwl_txrx_buf_init(void);
extern void sprdwl_txrx_buf_deinit(void);
extern int sprdwl_skb_to_tx_buf(struct sprdwl_intf *dev,
				struct sprdwl_msg_buf *msg_pos);
#endif /*__SPRDWL_TXRX_BUF_MM_H__*/
