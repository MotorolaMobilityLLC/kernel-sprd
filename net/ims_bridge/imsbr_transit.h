/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_TRANSIT_H
#define _IMSBR_TRANSIT_H

#include <linux/sipc.h>

void imsbr_transit_process(struct imsbr_sipc *sipc, struct sblock *blk,
			   bool freeit);

struct call_t_function {
	void (*sblock_release) (struct imsbr_sipc *sipc, struct sblock *blk);
	int (*sblock_receive) (struct imsbr_sipc *sipc, struct sblock *blk);
	int (*sblock_send) (struct imsbr_sipc *sipc, struct sblock *blk, int size);
	void (*sblock_put) (struct imsbr_sipc *sipc, struct sblock *blk);
	int (*sblock_get) (struct imsbr_sipc *sipc, struct sblock *blk, int size);
	void (*sipc_handler) (int event, void *data);
	void (*sipc_destroy) (struct imsbr_sipc *sipc);
};
void call_transit_function(struct call_t_function *ctf);

#endif
