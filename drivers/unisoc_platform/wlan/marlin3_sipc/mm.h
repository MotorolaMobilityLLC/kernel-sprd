#ifndef __SPRDWL_MM_H__
#define __SPRDWL_MM_H__

#include <linux/skbuff.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>

#define SPRDWL_PHYS_LEN 5
#define SPRDWL_PHYS_MASK ((1ULL << 40) - 1)
#define SPRDWL_MH_ADDRESS_BIT (1ULL << 39)
#define SPRDWL_MH_SIPC_ADDRESS_BIT 0x00F0FFFFFF
#define SPRDWL_MH_SIPC_ADDRESS_BASE 0x87000000

#ifdef SIPC_SUPPORT
#define SPRDWL_MAX_MH_BUF 450
#define SPRDWL_MAX_ADD_MH_BUF_ONCE 52
#else
#define SPRDWL_MAX_MH_BUF 500
#define SPRDWL_MAX_ADD_MH_BUF_ONCE 100
#endif

#define SPRDWL_ADDR_BUF_LEN (sizeof(struct sprdwl_addr_hdr) +\
			     sizeof(struct sprdwl_addr_trans_value) +\
			     (SPRDWL_MAX_ADD_MH_BUF_ONCE * SPRDWL_PHYS_LEN))

struct sprdwl_mm {
	int hif_offset;
	struct sk_buff_head buffer_list;
	/* hdr point to hdr of addr buf */
	void *hdr;
	/* addr_trans point to addr trans of addr buf */
	void *addr_trans;
	atomic_t alloc_num;
};

struct sprdwl_mm_tmp {
	int len;
	void *hdr;
};

int sprdwl_mm_init(struct sprdwl_mm *mm_entry, void *intf);
int sprdwl_mm_deinit(struct sprdwl_mm *mm_entry, void *intf);
void mm_mh_data_process(struct sprdwl_mm *mm_entry, void *data,
			int len, int buffer_type);
void mm_mh_data_event_process(struct sprdwl_mm *mm_entry, void *data,
			      int len, int buffer_type);
unsigned long mm_virt_to_phys(struct device *dev, void *buffer, size_t size,
			      enum dma_data_direction direction);
void *mm_phys_to_virt(struct device *dev, unsigned long pcie_addr, size_t size,
		      enum dma_data_direction direction, bool is_mh);
int sprdwl_tx_addr_buf_unmap(void *tx_msg,
			int complete, int tx_count);
int mm_buffer_alloc(struct sprdwl_mm *mm_entry, int need_num);
void print_mm_list(char *buf);
void print_buf_list_num(char *buf);
void mm_flush_buffer(struct sprdwl_mm *mm_entry);
#endif /* __SPRDWL_MM_H__ */
