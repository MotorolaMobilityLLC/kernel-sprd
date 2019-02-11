#include <linux/regmap.h>

#include "pcie-designware.h"

struct sprd_pcie {
	struct pcie_port pp;
	struct dw_pcie *pci;
	struct clk *pcie_eb;
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *evn);

