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

#define pr_fmt(fmt) "sipa_dele: %s " fmt, __func__

#include <linux/sipa.h>
#include <linux/sipc.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "sipa_dele_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

static struct cp_delegator *s_cp_delegator;

int cp_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_cp_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_cp_delegator),
				      GFP_KERNEL);
	if (!s_cp_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_cp_delegator->delegator,
				  params);
	if (ret)
		return ret;

	sipa_delegator_start(&s_cp_delegator->delegator);

	return 0;
}
