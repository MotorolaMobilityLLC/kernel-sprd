#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define SYST_CNT			0x0
#define SYST_CNT_SHDW		0x4
#define SYST_CNT_EXT		0x8
#define SYST_CNT_SHDW_EXT	0xc

static void __iomem *sprd_sysfrt_addr_base;

u64 sprd_sysfrt_read(void)
{
	u32 val_lo, val_hi;

	if (!sprd_sysfrt_addr_base)
		return 0;

	val_lo = readl_relaxed(sprd_sysfrt_addr_base + SYST_CNT_SHDW);
	val_hi = readl_relaxed(sprd_sysfrt_addr_base + SYST_CNT_SHDW_EXT);

	return  (((u64) val_hi) << 32 | val_lo);
}
EXPORT_SYMBOL(sprd_sysfrt_read);

static int __init sprd_sysfrt_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "sprd,sysfrt-timer");
	if (np) {
		sprd_sysfrt_addr_base = of_iomap(np, 0);
		if (!sprd_sysfrt_addr_base) {
			pr_err("Can't map sprd sysfrt timer reg!\n");
			return -ENOMEM;
		}
	}

	return 0;
}

device_initcall(sprd_sysfrt_init);

