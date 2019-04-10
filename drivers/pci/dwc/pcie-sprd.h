#include <linux/regmap.h>

#include "pcie-designware.h"

struct sprd_pcie {
	const char *label;
	struct dw_pcie *pci;
	struct clk *pcie_eb;

#ifdef CONFIG_SPRD_IPA_INTC
	u32 interrupt_line;
#endif
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *evn);

