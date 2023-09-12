/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-10-12 09:49:25
 *
 */


#ifndef ANLG_PHY_G3_H
#define ANLG_PHY_G3_H



#define REG_ANLG_PHY_G3_ANALOG_EFUSE_EFS0_ENK           (0x0000)
#define REG_ANLG_PHY_G3_ANALOG_EFUSE_EFS1_ENK           (0x0004)
#define REG_ANLG_PHY_G3_ANALOG_EFUSE_REG_SEL_CFG_0      (0x0008)

/* REG_ANLG_PHY_G3_ANALOG_EFUSE_EFS0_ENK */

#define BIT_ANLG_PHY_G3_ANALOG_EFUSE_EFS0_ENK1          BIT(1)
#define BIT_ANLG_PHY_G3_ANALOG_EFUSE_EFS0_ENK2          BIT(0)

/* REG_ANLG_PHY_G3_ANALOG_EFUSE_EFS1_ENK */

#define BIT_ANLG_PHY_G3_ANALOG_EFUSE_EFS1_ENK1          BIT(1)
#define BIT_ANLG_PHY_G3_ANALOG_EFUSE_EFS1_ENK2          BIT(0)

/* REG_ANLG_PHY_G3_ANALOG_EFUSE_REG_SEL_CFG_0 */

#define BIT_ANLG_PHY_G3_DBG_SEL_ANALOG_EFUSE_EFS0_ENK1  BIT(3)
#define BIT_ANLG_PHY_G3_DBG_SEL_ANALOG_EFUSE_EFS0_ENK2  BIT(2)
#define BIT_ANLG_PHY_G3_DBG_SEL_ANALOG_EFUSE_EFS1_ENK1  BIT(1)
#define BIT_ANLG_PHY_G3_DBG_SEL_ANALOG_EFUSE_EFS1_ENK2  BIT(0)


#endif /* ANLG_PHY_G3_H */

