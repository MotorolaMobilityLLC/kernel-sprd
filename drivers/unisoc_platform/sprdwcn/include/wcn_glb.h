#ifndef __WCN_GLB_H__
#define __WCN_GLB_H__

#include <misc/marlin_platform.h>

#include "bufring.h"
#include "loopcheck.h"
#include "mdbg_type.h"
//#include "rdc_debug.h"
#include "reset.h"
#include "sysfs.h"
#include "wcn_dbg.h"
#include "wcn_parn_parser.h"
#include "wcn_txrx.h"
#include "wcn_log.h"

#if (!defined CONFIG_WCN_INTEG) && (!defined CONFIG_SC2355) && (!defined CONFIG_UMW2652) && (!defined CONFIG_UMW2653)
#define MDBG_RX_RING_SIZE	(128 * 1024)
static inline int mdbg_dump_mem(void)
{
	return 0;
}
#endif

#ifdef CONFIG_WCN_INTEG
#include "wcn_integrate_glb.h"
#endif

#ifdef CONFIG_SC2355
#include "sc2355_glb.h"
#include "wcn_dump.h"
#endif

#ifdef CONFIG_UMW2652
#include "umw2652_glb.h"
#include "wcn_dump.h"
#endif

#ifdef CONFIG_UMW2653
#include "umw2653_glb.h"
#include "wcn_dump.h"
#endif

#endif
