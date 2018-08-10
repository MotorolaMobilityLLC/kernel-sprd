/*
 * Copyright (c) 2017 Spreadtrum
 *
 * WCN partition parser for different CPU have different path with EMMC and NAND
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/dirent.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/fs_struct.h>
#include <linux/module.h>
#include <linux/path.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "mdbg_type.h"
#include "wcn_parn_parser.h"


#define ROOT_PATH "/"
#define ETC_PATH "/etc"
#define FSTAB_PATH "/etc/fstab"
#define CONF_COMMENT '#'
#define CONF_LF '\n'
#define CONF_DELIMITERS " =\n\r\t"
#define CONF_VALUES_DELIMITERS "=\n\r\t"
#define CONF_MAX_LINE_LEN 255
static const char *prefix = "fstab.";
static char FSTAB_NAME[255];

static char *fgets(char *buf, int buf_len, struct file *fp)
{
	int ret;
	int i = 0;

	ret = kernel_read(fp, buf, buf_len, &fp->f_pos);

	if (ret <= 0)
		return NULL;

	while (buf[i++] != '\n' && i < ret)
		;

	if (i <= ret)
		fp->f_pos += i;
	else
		return NULL;

	if (i < buf_len)
		buf[i] = 0;

	return buf;
}



static int load_fstab_conf(const char *p_path, char *WCN_PATH)
{
	struct file *p_file;
	char *p_name;
	char line[CONF_MAX_LINE_LEN+1];
	char *p;
	char *temp;
	bool match_flag;

	match_flag = false;
	p = line;
	pr_info("Attempt to load conf from %s\n", p_path);

	p_file = filp_open(p_path, O_RDONLY, 0);
	if (IS_ERR(p_file)) {
		pr_err("%s open file %s error not find\n",
			p_path, __func__);
		return PTR_ERR(p_file);
	}

	/* read line by line */
	while (fgets(line, CONF_MAX_LINE_LEN+1, p_file) != NULL) {

		if ((line[0] == CONF_COMMENT) || (line[0] == CONF_LF))
			continue;

		p = line;
		p_name = strsep(&p, CONF_DELIMITERS);
		pr_info("wcn p_name %s\n", p_name);
		if (p_name != NULL) {
			temp = strstr(p_name, "userdata");
			if (temp != NULL) {
				snprintf(WCN_PATH, strlen(p_name)+1,
					"%s", p_name);
				WCN_PATH[strlen(WCN_PATH) - strlen(temp)]
					= '\0';
				snprintf(WCN_PATH, strlen(WCN_PATH)+9,
					"%s%s", WCN_PATH, "wcnmodem");
				match_flag = true;
				break;
			}
		}
	}

	filp_close(p_file, NULL);
	if (match_flag)
		return 0;
	else
		return -1;
}

static int prefixcmp(const char *str, const char *prefix)
{
	for (; ; str++, prefix++)
		if (!*prefix)
			return 0;
		else if (*str != *prefix)
			return (unsigned char)*prefix - (unsigned char)*str;
}

static int find_callback(struct dir_context *ctx, const char *name, int namlen,
		     loff_t offset, u64 ino, unsigned int d_type)
{
	int tmp;

	tmp = prefixcmp(name, prefix);
	if (tmp == 0) {
		snprintf(FSTAB_NAME, strlen(name)+2, "/%s", name);
		FSTAB_NAME[strlen(name)+3] = '\0';
		WCN_INFO("FSTAB_NAME is %s\n", FSTAB_NAME);
	}

	return 0;
}

static struct dir_context ctx =  {
	.actor = find_callback,
};

int parse_firmware_path(char *FIRMWARE_PATH)
{
	u32 ret;
	struct file *file1;

	WCN_INFO("%s entry\n", __func__);

	file1 = filp_open(ROOT_PATH, O_DIRECTORY, 0);
	if (IS_ERR(file1)) {
		pr_err("%s open dir %s error: %d\n",
			__func__, ROOT_PATH, IS_ERR(file1));
		return PTR_ERR(file1);
	}

	iterate_dir(file1, &ctx);
	fput(file1);
	ret = load_fstab_conf(FSTAB_NAME, FIRMWARE_PATH);
	pr_info("%s %d ret %d\n", __func__, __LINE__, ret);
	if (ret != 0) {
		file1 = NULL;
		file1 = filp_open(ETC_PATH, O_DIRECTORY, 0);
		if (IS_ERR(file1)) {
			pr_err("%s open file %s error\n", ETC_PATH, __func__);
			return PTR_ERR(file1);
		}
		iterate_dir(file1, &ctx);
		fput(file1);
		ret = load_fstab_conf(FSTAB_NAME, FIRMWARE_PATH);
		if (ret != 0)
			ret = load_fstab_conf(FSTAB_PATH, FIRMWARE_PATH);
	}

	return ret;
}

