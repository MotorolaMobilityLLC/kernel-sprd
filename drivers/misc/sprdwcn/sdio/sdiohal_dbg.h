#ifndef __SDIO_DBG_H__
#define __SDIO_DBG_H_

#include "../include/wcn_dbg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "WCN SDIOHAL" fmt

#endif
