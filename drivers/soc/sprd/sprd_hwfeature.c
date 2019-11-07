// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>
#include <linux/string.h>

/* After bootloader turns on dtb auto creating, 0 should be set, otherwise 1 */
#define HWFEATURE_EARLY_PARAM_PARSER_ENABLED    1
#define HWFEATURE_STR_SIZE_LIMIT_KEY            1024
#define HWFEATURE_HASH_BITS                     4
#define HWFEATURE_TABLE_SIZE                    (1 << HWFEATURE_HASH_BITS)
#define HWFEATURE_TABLE_MASK                    (HWFEATURE_TABLE_SIZE - 1)

static struct hlist_head hwf_table[HWFEATURE_TABLE_SIZE];
struct hwf_property {
	struct hlist_node hlist;
	const char *key;
	const char *value;
};
static void __init sprd_kproperty_append(const char *key, const char *value);

#if HWFEATURE_EARLY_PARAM_PARSER_ENABLED
static void __init *hwfeature_strdup(char *old_str)
{
	char *p;
	unsigned long len = strlen(old_str) + 1;

	p = kzalloc(len, GFP_ATOMIC);
	if (!p) {
		pr_err("hwfeature kzalloc failed\n");
		return NULL;
	}

	memcpy(p, old_str, len);
	return p;
}

#define HWFEATURE_EARLY_INIT_MAX 4
static struct {
	char key[HWFEATURE_STR_SIZE_LIMIT_KEY];
	char value[HWFEATURE_STR_SIZE_LIMIT];
} early_init_hwpara_data[HWFEATURE_EARLY_INIT_MAX] __initdata;
static int early_init_hwpara_idx __initdata = -1;

static void __init early_init_hwpara_data_append(const char *key, const char *value)
{
	int idx;

	if (early_init_hwpara_idx >= HWFEATURE_EARLY_INIT_MAX) {
		pr_warn("hwfeature temp cache is full\n");
		return;
	}

	idx = ++early_init_hwpara_idx;
	strlcpy(early_init_hwpara_data[idx].key, key, HWFEATURE_STR_SIZE_LIMIT_KEY);
	strlcpy(early_init_hwpara_data[idx].value, value, HWFEATURE_STR_SIZE_LIMIT);
}

static int __init hwfeature_lwfq_type(char *str)
{
	/* kzalloc is not ready for use at this stage */
	early_init_hwpara_data_append("lwfq/type", str);
	return 0;
}
early_param("androidboot.lwfq.type", hwfeature_lwfq_type);

static void __init early_init_hwpara(void)
{
	unsigned int i;
	char *key;
	char *value;

	for (i = 0; i <= early_init_hwpara_idx; i++) {
		key = early_init_hwpara_data[i].key;
		value = early_init_hwpara_data[i].value;
		sprd_kproperty_append(hwfeature_strdup(key), hwfeature_strdup(value));
	}
}
#else
static void __init early_init_hwpara(void) {}
#endif

static unsigned int hash_str(const char *str)
{
	const unsigned int hash_mult = 2654435387U;
	unsigned int h = 0;

	while (*str)
		h = (h + (unsigned int) *str++) * hash_mult;

	return h & HWFEATURE_TABLE_MASK;
}

static void __init sprd_kproperty_append(const char *key, const char *value)
{
	struct hlist_head *head;
	struct hwf_property *hp;

	hp = kzalloc(sizeof(*hp), GFP_ATOMIC);
	if (!hp) {
		pr_err("hwfeature kzalloc failed\n");
		return;
	}

	INIT_HLIST_NODE(&hp->hlist);
	hp->key = key;
	hp->value = value;

	head = &hwf_table[hash_str(key)];
	hlist_add_head(&hp->hlist, head);
}

static int __init hwfeature_init(void)
{
	int i;

	for (i = 0; i < HWFEATURE_TABLE_SIZE; i++)
		INIT_HLIST_HEAD(&hwf_table[i]);

	early_init_hwpara();

	return 0;
}
early_initcall(hwfeature_init);

void sprd_kproperty_get(const char *key, char *value, const char *default_value)
{
	struct hlist_head *head;
	struct hwf_property *hp;

	head = &hwf_table[hash_str(key)];
	hlist_for_each_entry(hp, head, hlist) {
		if (!strcmp(hp->key, key)) {
			strlcpy(value, hp->value, HWFEATURE_STR_SIZE_LIMIT);
			return;
		}
	}

	if (default_value == NULL)
		default_value = HWFEATURE_KPROPERTY_DEFAULT_VALUE;
	strlcpy(value, default_value, HWFEATURE_STR_SIZE_LIMIT);
}
EXPORT_SYMBOL(sprd_kproperty_get);
