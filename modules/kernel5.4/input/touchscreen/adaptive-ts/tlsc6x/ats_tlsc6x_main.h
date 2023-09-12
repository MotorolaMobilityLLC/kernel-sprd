/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION			DATE			AUTHOR
 *
 */

#ifndef __tlsc6x_main_h__
#define __tlsc6x_main_h__

//#define TLSC_TPD_PROXIMITY
#define TLSC_APK_DEBUG		/* apk debugger, close:undef */
//#define TLSC_AUTO_UPGRADE         //cfg auto update
//#define TLSC_ESD_HELPER_EN	/* esd helper, close:undef */
/* #defineTLSC_FORCE_UPGRADE */
// #define TP_GESTRUE
/* #define TLSC_TP_PROC_SELF_TEST */
// #define TLSC_BUILDIN_BOOT     // firware update
#define TLSC_FACTORY_TEST
/*********************************************************/
#define MAX_CHIP_ID   (20)

#define tlsc_info(x...) pr_notice("[TS][tlsc] " x)
#define tlsc_err(x...) pr_err("[TS][tlsc][error] " x)
#define TLSC_FUNC_ENTER() pr_notice("[TS][tlsc]%s: Enter\n", __func__)

struct tlsc6x_platform_data {
	u32 irq_gpio_flags;
	u32 reset_gpio_flags;
	u32 irq_gpio_number;
	u32 reset_gpio_number;
	u32 tpd_firmware_update;
	u32 tpd_ps_status;
	u32 tpd_sensorhub_status;
	u32 have_virtualkey;
	u32 virtualkeys[12];
	u32 x_res_max;
	u32 y_res_max;
};

struct tlsc6x_stest_crtra {
    unsigned short xch_n;
    unsigned short ych_n;
    unsigned short allch_n;
    unsigned short st_nor_os_L1;
    unsigned short st_nor_os_L2;
    unsigned short st_nor_os_bar;
    unsigned short st_nor_os_key;
    unsigned short m_os_nor_std;
    unsigned short ffset;
    unsigned short fsset;
    unsigned short  fsbse_max;
    unsigned short  fsbse_bar;
    unsigned char remap[48];
    unsigned short rawmax[48];
    unsigned short rawmin[48];
};

struct tlsc6x_updfile_header {
	u32 sig;
	u32 resv;
	u32 n_cfg;
	u32 n_match;
	u32 len_cfg;
	u32 len_boot;
};
extern struct mutex i2c_rw_access;
extern int tlsc6x_esdHelperFreeze;
extern unsigned int g_tlsc6x_cfg_ver;
extern unsigned int g_tlsc6x_boot_ver;
extern unsigned short g_tlsc6x_chip_code;
extern unsigned int g_needKeepRamCode;
extern struct i2c_client *g_tlsc6x_client;

extern int tlsc6x_tp_dect(struct i2c_client *client);
extern int tlsc6x_auto_upgrade_buidin(void);
extern int tlsc6x_load_gesture_binlib(void);
extern void tlsc6x_data_crash_deal(void);

extern int tlsx6x_update_burn_cfg(u16 *ptcfg);
extern int tlsx6x_update_running_cfg(u16 *ptcfg);
extern int tlsc6x_set_dd_mode_sub(void);
extern int tlsc6x_set_nor_mode_sub(void);
extern int tlsc6x_set_dd_mode(void);
extern int tlsc6x_set_nor_mode(void);
extern int tlsc6x_load_ext_binlib(u8 *pdata, u16 len);
extern int tlsc6x_update_f_combboot(u8 *pdata, u16 len);
extern int  tlsc6x_get_firmware_data(struct i2c_client *client);

extern int tlsc6x_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
extern int tlsc6x_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
extern int tlsc6x_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
extern int tlsc6x_i2c_write(struct i2c_client *client, char *writebuf, int writelen);
extern int tlsc6x_i2c_read_sub(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
extern int tlsc6x_i2c_write_sub(struct i2c_client *client, char *writebuf, int writelen);

#if (defined TPD_AUTO_UPGRADE_PATH) || (defined TLSC_APK_DEBUG)
extern int tlsc6x_proc_cfg_update(u8 *dir, int behave);
#endif
extern void tlsc6x_tpd_reset_force(void);
extern int tlsc6x_fif_write(char *fname, u8 *pdata, u16 len);
#ifdef CONFIG_FACTORY_TEST_EN
void tlsc_factory_test(char* szOut, int* len);
#endif
#endif
