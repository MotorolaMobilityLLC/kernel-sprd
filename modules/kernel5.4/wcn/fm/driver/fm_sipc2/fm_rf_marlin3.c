#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>
#include <linux/kdev_t.h>
#include <linux/proc_fs.h>
#include "fm_rf_marlin3.h"

#include "unisoc_fm_log.h"
#include <linux/platform_device.h>
#include <linux/firmware.h>

extern struct device *fm_miscdev;

#define SYSTEM_FM_CONFIG_FILE "fm_board_config.ini"

#define CF_TAB(NAME, MEM_OFFSET, TYPE) \
	{ NAME, (size_t)(&(((struct fm_config_t *)(0))->MEM_OFFSET)), TYPE}

#define OFS_MARK_STRING \
	"#-----------------------------------------------------------------\r\n"

static struct nvm_name_table g_config_table[] = {

	/*[SETCTION 13]fm_config:coex_config_t
	 */
	CF_TAB("Fm_modem_work_freq", fm_modem_work_freq, 2),
	CF_TAB("Stra_sel", stra_sel, 2),
	CF_TAB("Seek_ch_th", seek_ch_th, 2),
	CF_TAB("Mono_pwr_th1", mono_pwr_th1, 2),
	CF_TAB("Mono_pwr_th2", mono_pwr_th2, 2),
	CF_TAB("Mono_pwr_th3", mono_pwr_th3, 2),
	CF_TAB("Seek_chan_mode", seek_chan_mode, 2),
	CF_TAB("Seek_vldch_fo_th1", seek_vldch_fo_th1, 2),
	CF_TAB("Seek_vldch_fo_th2", seek_vldch_fo_th2, 2),
	CF_TAB("Noise_pwr_th0", noise_pwr_th0, 2),
	CF_TAB("Noise_pwr_th1", noise_pwr_th1, 2),
	CF_TAB("Noise_pwr_th2", noise_pwr_th2, 2),
	CF_TAB("Pdp_th", pdp_th, 2),
	CF_TAB("Pdp_dev1", pdp_dev1, 2),
	CF_TAB("Pdp_dev2", pdp_dev2, 2),
	CF_TAB("Seek_sample_num_div", seek_sample_num_div, 2),
	CF_TAB("Boundary_offset", boundary_offset, 2),
	CF_TAB("Fm_db_comp", fm_db_comp, 2),
	CF_TAB("Fm_cali_reserved", fm_cali_reserved[0], 2)
};

static int find_type(char key)
{
	if ((key >= 'a' && key <= 'w') ||
		(key >= 'y' && key <= 'z') ||
		(key >= 'A' && key <= 'W') ||
		(key >= 'Y' && key <= 'Z') ||
		('_' == key))
		return 1;
	if ((key >= '0' && key <= '9') ||
		('-' == key))
		return 2;
	if (('x' == key) ||
		('X' == key) ||
		('.' == key))
		return 3;
	if ((key == '\0') ||
		('\r' == key) ||
		('\n' == key) ||
		('#' == key))
		return 4;
	return 0;
}

static int fm_nvm_set_cmd(struct nvm_name_table *pTable,
	struct nvm_cali_cmd *cmd, void *p_data)
{
	int i;
	unsigned char *p;

	if ((1 != pTable->type) &&
		(2 != pTable->type) &&
		(4 != pTable->type))
		return -1;

	p = (unsigned char *)(p_data) + pTable->mem_offset;

	dev_unisoc_fm_info(fm_miscdev,"[g_table]%s, offset:%u, num:%u, value:\
			%d %d %d %d %d %d %d %d %d %d \n",
			pTable->itm, pTable->mem_offset, cmd->num,
			cmd->par[0], cmd->par[1], cmd->par[2],
			cmd->par[3], cmd->par[4], cmd->par[5],
			cmd->par[6], cmd->par[7], cmd->par[8],
			cmd->par[9]);

	for (i = 0; i < cmd->num; i++) {
		if (1 == pTable->type)
			*((unsigned char *)p + i)
			= (unsigned char)(cmd->par[i]);
		else if (2 == pTable->type)
			*((unsigned short *)p + i)
			= (unsigned short)(cmd->par[i]);
		else if (4 == pTable->type)
			*((unsigned int *)p + i)
			= (unsigned int)(cmd->par[i]);
		else
			dev_unisoc_fm_info(fm_miscdev,"%s, type err\n", __func__);
	}
	return 0;
}

static void get_cmd_par(const u8 *str, struct nvm_cali_cmd *cmd)
{
	int i, j, bufType, cType, flag;
	char tmp[128];
	char c;
	long val;

	bufType = -1;
	cType = 0;
	flag = 0;
	memset(cmd, 0, sizeof(struct nvm_cali_cmd));

	for (i = 0, j = 0;; i++) {
		c = str[i];
		cType = find_type(c);
		if ((1 == cType) ||
			(2 == cType) ||
			(3 == cType)) {
			tmp[j] = c;
			j++;
			if (-1 == bufType) {
				if (2 == cType)
					bufType = 2;
				else
					bufType = 1;
			} else if (2 == bufType) {
				if (1 == cType)
					bufType = 1;
			}
			continue;
		}
		if (-1 != bufType) {
			tmp[j] = '\0';

			if ((1 == bufType) && (0 == flag)) {
				strcpy(cmd->itm, tmp);
				flag = 1;
			} else {
				if (kstrtol(tmp, 0, &val))
					dev_unisoc_fm_info(fm_miscdev," %s ", tmp);
			/* dev_unisoc_fm_err(fm_miscdev,"kstrtol %s: error\n", tmp); */
				cmd->par[cmd->num] = val & 0xFFFFFFFF;
				cmd->num++;
			}
			bufType = -1;
			j = 0;
		}
		if (0 == cType)
			continue;
		if (4 == cType)
			return;
	}
}

static struct nvm_name_table *cf_table_match(struct nvm_cali_cmd *cmd)
{
	int i;
	struct nvm_name_table *pTable = NULL;
	int len = sizeof(g_config_table) / sizeof(struct nvm_name_table);

	if ((NULL == cmd) || (NULL == cmd->itm))
		return NULL;
	for (i = 0; i < len; i++) {
		if (NULL == g_config_table[i].itm)
			continue;
		if (0 != strcmp(g_config_table[i].itm, cmd->itm))
			continue;
		pTable = &g_config_table[i];
		break;
	}
	return pTable;
}

static int fm_nvm_buf_operate(const u8 *pBuf, int file_len, void *p_data)
{
	int i, p;
	struct nvm_cali_cmd cmd;
	struct nvm_name_table *pTable = NULL;

	if ((NULL == pBuf) || (0 == file_len))
		return -1;
	for (i = 0, p = 0; i < file_len; i++) {
		if (('\n' == *(pBuf + i)) ||
			('\r' == *(pBuf + i)) ||
			('\0' == *(pBuf + i))) {
			if (5 <= (i - p)) {
				get_cmd_par((pBuf + p), &cmd);
				pTable = cf_table_match(&cmd);

				if (NULL != pTable)
					fm_nvm_set_cmd(pTable, &cmd, p_data);
			}
			p = i + 1;
		}
	}
	return 0;
}

static int fm_nvm_parse(const char *path, void *p_data)
{
	int ret;
	const struct firmware *fm_fw;
	const u8 *buffer;
	size_t buffer_len;
	ret = request_firmware(&fm_fw, path, fm_miscdev);
	if (ret < 0) {
		dev_unisoc_fm_info(fm_miscdev,"%s, Failed to load firmware file\n", __func__);
		return ret;
	}
	buffer = fm_fw->data;
	buffer_len = fm_fw->size;

	ret = fm_nvm_buf_operate(buffer, buffer_len, p_data);
	if (!ret) {
		release_firmware(fm_fw);
	}
	dev_unisoc_fm_info(fm_miscdev,"%s(), ok!\n", __func__);
	return 0;
}

int get_fm_config_param(struct fm_config_t *p)
{
	return fm_nvm_parse(SYSTEM_FM_CONFIG_FILE, (void *)p);
}

