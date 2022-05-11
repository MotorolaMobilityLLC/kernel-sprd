/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_APB_PROT__
#define __SPRD_APB_PROT__

#include <linux/regmap.h>

#ifdef CONFIG_SPRD_APB_PROT
extern int sprd_apb_prot_write(struct regmap *map, unsigned int reg, unsigned int val);
#else
static inline int sprd_apb_prot_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	return regmap_write(map, reg, val);
}
#endif /* CONFIG_SPRD_APB_PROT */


#endif /* __SPRD_APB_PROT__ */
