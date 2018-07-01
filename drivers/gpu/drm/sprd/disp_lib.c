/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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
#define pr_fmt(__fmt) "[drm][%20s] "__fmt, __func__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "disp_lib.h"

int str_to_u32_array(const char *p, unsigned int base, u32 array[])
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < 255; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou32(str, base, &array[i]);
		if (ret) {
			pr_err("input format error\n");
			break;
		}

		length++;
	}

	return length;
}
EXPORT_SYMBOL_GPL(str_to_u32_array);

int str_to_u8_array(const char *p, unsigned int base, u8 array[])
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < 255; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou8(str, base, &array[i]);
		if (ret) {
			pr_err("input format error\n");
			break;
		}

		length++;
	}

	return length;
}
EXPORT_SYMBOL_GPL(str_to_u8_array);

void *disp_ops_attach(const char *str, struct list_head *head)
{
	struct ops_list *list;
	const char *ver;

	list_for_each_entry(list, head, head) {
		ver = list->entry->ver;
		if (!strcmp(str, ver))
			return list->entry->ops;
	}

	pr_err("attach disp ops %s failed\n", str);
	return NULL;
}
EXPORT_SYMBOL_GPL(disp_ops_attach);

int disp_ops_register(struct ops_entry *entry, struct list_head *head)
{
	struct ops_list *list;

	list = kzalloc(sizeof(struct ops_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	list->entry = entry;
	list_add(&list->head, head);

	return 0;
}
EXPORT_SYMBOL_GPL(disp_ops_register);

struct device *dev_get_prev(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *endpoint;
	struct device_node *prev_node;
	struct platform_device *prev_pdev;

	endpoint = of_graph_get_endpoint_by_regs(np, 1, 0);
	if (!endpoint) {
		pr_err("error: %s/port1/endpoint0 was not found\n", np->name);
		return NULL;
	}

	prev_node = of_graph_get_remote_port_parent(endpoint);
	if (!prev_node) {
		pr_err("error: device node was not found by endpoint0\n");
		return NULL;
	}

	prev_pdev = of_find_device_by_node(prev_node);
	if (prev_pdev == NULL) {
		pr_err("find %s platform device failed\n", prev_node->name);
		return NULL;
	}

	return &prev_pdev->dev;
}
EXPORT_SYMBOL_GPL(dev_get_prev);

struct device *dev_get_next(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *endpoint;
	struct device_node *next_node;
	struct platform_device *next_pdev;

	endpoint = of_graph_get_endpoint_by_regs(np, 0, 0);
	if (!endpoint) {
		pr_err("error: %s/port0/endpoint0 was not found\n", np->name);
		return NULL;
	}

	next_node = of_graph_get_remote_port_parent(endpoint);
	if (!next_node) {
		pr_err("error: device node was not found by endpoint0\n");
		return NULL;
	}

	next_pdev = of_find_device_by_node(next_node);
	if (next_pdev == NULL) {
		pr_err("find %s platform device failed\n", next_node->name);
		return NULL;
	}

	return &next_pdev->dev;
}
EXPORT_SYMBOL_GPL(dev_get_next);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@unisoc.com");
MODULE_DESCRIPTION("display common API library");
