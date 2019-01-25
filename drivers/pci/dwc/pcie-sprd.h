#include <linux/regmap.h>

#include "pcie-designware.h"

struct syscon_pcie {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct sprd_pcie {
	struct pcie_port pp;
	struct dw_pcie *pci;
	struct clk *pcie_eb;

	struct syscon_pcie pcie2_eb;
	struct syscon_pcie pcieh_frc_on;
	struct syscon_pcie pciev_frc_on;
	struct syscon_pcie pcie2_frc_wakeup;
	struct syscon_pcie pcie2_perst;
	struct syscon_pcie pcie2_phy_pwron;
	struct syscon_pcie ipa_sys_dly;
	struct syscon_pcie pciepllh_pd;
	struct syscon_pcie pciepllh_divn;
	struct syscon_pcie pciepllh_diff_or_sign_sel;
	struct syscon_pcie pciepllh_kdelta;
	struct syscon_pcie pciepllh_reserved;
	struct syscon_pcie pciepllh_cp_en;
	struct syscon_pcie pciepllh_icp;
	struct syscon_pcie pciepllh_ldo_trim;
	struct syscon_pcie pcie2_phy_sw_en;
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};
