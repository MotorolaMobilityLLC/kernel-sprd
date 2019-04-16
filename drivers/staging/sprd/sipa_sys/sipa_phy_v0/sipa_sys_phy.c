/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include<linux/regmap.h>
#include "sipa_sys_phy.h"

int sipa_sys_force_wakeup(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->forcewakeup_reg,
					 cfg->forcewakeup_mask,
					 cfg->forcewakeup_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_force_wakeup);

int sipa_sys_set_sipa_enable(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->sys_regmap) {
		ret = regmap_update_bits(cfg->sys_regmap,
					 cfg->ipaeb_reg,
					 cfg->ipaeb_mask, cfg->ipaeb_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_set_sipa_enable);

int sipa_sys_clear_force_shutdown(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->forceshutdown_reg,
					 cfg->forceshutdown_mask,
					 ~cfg->forceshutdown_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_clear_force_shutdown);

int sipa_sys_set_auto_shutdown(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->autoshutdown_reg,
					 cfg->autoshutdown_mask,
					 cfg->autoshutdown_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_set_auto_shutdown);

int sipa_sys_clear_force_deepsleep(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->forcedslp_reg,
					 cfg->forcedslp_mask,
					 ~cfg->forcedslp_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_clear_force_deepsleep);

int sipa_sys_set_deepsleep(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->dslpeb_reg,
					 cfg->dslpeb_mask, cfg->dslpeb_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_set_deepsleep);

int sipa_sys_clear_force_lightsleep(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->forcelslp_reg,
					 cfg->forcelslp_mask,
					 ~cfg->forcelslp_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_clear_force_lightsleep);

int sipa_sys_set_lightsleep(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->lslpeb_reg,
					 cfg->lslpeb_mask, cfg->lslpeb_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_set_lightsleep);

int sipa_sys_set_smart_lightsleep(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->pmu_regmap) {
		ret = regmap_update_bits(cfg->pmu_regmap,
					 cfg->smartlslp_reg,
					 cfg->smartlslp_mask,
					 cfg->smartlslp_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_set_smart_lightsleep);

int sipa_sys_disable_ipacm4(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->sys_regmap) {
		ret = regmap_update_bits(cfg->sys_regmap,
					 cfg->cm4eb_reg,
					 cfg->cm4eb_mask, ~cfg->cm4eb_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_disable_ipacm4);

int sipa_sys_auto_gate_enable(struct sipa_sys_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->sys_regmap) {
		ret = regmap_update_bits(cfg->sys_regmap,
					 cfg->autogateb_reg,
					 cfg->autogateb_mask,
					 cfg->autogateb_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);

		ret = regmap_update_bits(cfg->sys_regmap,
					 cfg->s5autogateb_reg,
					 cfg->s5autogateb_mask,
					 cfg->s5autogateb_mask);
		if (ret < 0)
			pr_warn("%s: regmap s5 autogate update bits failed",
				__func__);

	}
	return ret;
}
EXPORT_SYMBOL(sipa_sys_auto_gate_enable);

void sipa_sys_proc_init(struct sipa_sys_cfg_tag *cfg)
{
	/*sipa enable */
	sipa_sys_force_wakeup(cfg);
	sipa_sys_set_sipa_enable(cfg);

	/*step1 clear force shutdown:0x32280538[25] */
	sipa_sys_clear_force_shutdown(cfg);
	/*set auto shutdown enable:0x32280538[24] */
	sipa_sys_set_auto_shutdown(cfg);

	/*step2 clear ipa force deep sleep:0x32280544[6] */
	sipa_sys_clear_force_deepsleep(cfg);
	/*set ipa deep sleep enable:0x3228022c[0] */
	sipa_sys_set_deepsleep(cfg);

	/*step3 clear ipa force light sleep:0x32280548[6] */
	sipa_sys_clear_force_lightsleep(cfg);
	/*set ipa light sleep enable:0x32280230[6] */
	sipa_sys_set_lightsleep(cfg);
	/*set ipa smart light sleep enable:0x32280230[7] */
	sipa_sys_set_smart_lightsleep(cfg);

	/*step4 disable ipa sys cm4 */
	sipa_sys_disable_ipacm4(cfg);
	/*step5 auto gate enable */
	sipa_sys_auto_gate_enable(cfg);
}
EXPORT_SYMBOL(sipa_sys_proc_init);
