/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _CORE_H
#define _CORE_H

enum autotest_type {
	AT_GPIO,
	AT_PINCTRL,

	/* for user to add new test type */
	AT_MAX,
};

struct autotest_handler {
	const char *label;
	enum autotest_type type;
	struct list_head node;
	int (*pre_test) (struct autotest_handler *handler, void *data);
	int (*start_test) (struct autotest_handler *handler, void *data);
	int (*post_test) (struct autotest_handler *handler, void *data);
	void *data;
};

int sprd_autotest_register_handler(struct autotest_handler *handler);
void sprd_autotest_unregister_handler(struct autotest_handler *handler);

#endif
