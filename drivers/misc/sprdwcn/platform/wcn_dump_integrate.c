/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "bufring.h"
#include "wcn_glb.h"
#include "wcn_log.h"

static struct mdbg_ring_t	*mdev_ring;
gnss_dump_callback gnss_dump_handle;

static int mdbg_snap_shoot_iram_data(void *buf, u32 addr, u32 len)
{
	struct regmap *regmap;
	u32 i;
	u8 *ptr = NULL;

	WCN_INFO("start snap_shoot iram data!addr:%x,len:%d", addr, len);
	if (marlin_get_module_status() == 0) {
		WCN_ERR("module status off:can not get iram data!\n");
		return -1;
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
		regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
	else
		regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);
	wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
	for (i = 0; i < len / 4; i++) {
		ptr = buf + i * 4;
		wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
	}
	WCN_INFO("snap_shoot iram data success\n");

	return 0;
}

int mdbg_snap_shoot_iram(void *buf)
{
	u32 ret;

	ret = mdbg_snap_shoot_iram_data(buf,
			0x18000000, 1024 * 32);

	return ret;
}

static void mdbg_dump_str(char *str, int str_len, u32 type)
{
	u8 *pad_str;
	u32 pad_len;
	u32 mod_len;

	if (!str)
		return;
	WCN_INFO("mdbg dump str:%s  str_len:%d\n", str, str_len);
	msleep(20);

	mdbg_ring_write(mdev_ring, str, str_len);
	/* reg dump:4 bytes align */
	if (type == DUMP_STR_TYPE_REG) {
		mod_len = str_len % 4;
		if (mod_len != 0) {
			pad_len = 4 - mod_len;
			pad_str = kzalloc(pad_len, GFP_KERNEL);
			if (!pad_str)
				return;
			mdbg_ring_write(mdev_ring, pad_str, pad_len);
			WCN_INFO("dump reg need pad len:%d str len:%d\n",
					pad_len, str_len);
			kfree(pad_str);
		}
	}
	wake_up_log_wait();
	WCN_INFO("dump str finish!");
}

static int mdbg_dump_ap_register_data(phys_addr_t addr, u32 len,
	char *str, int str_len)
{
	u32 value = 0;
	u8 *ptr = NULL;

	ptr = (u8 *)&value;
	mdbg_dump_str(str, str_len, DUMP_STR_TYPE_REG);
	wcn_read_data_from_phy_addr(addr, &value, len);
	mdbg_ring_write(mdev_ring, ptr, len);
	wake_up_log_wait();

	return 0;
}

static int mdbg_dump_cp_register_data(u32 addr, u32 len, char *str, int str_len)
{
	struct regmap *regmap;
	u32 i;
	u32 count, trans_size;
	u8 *buf = NULL;
	u8 *ptr = NULL;

	WCN_INFO("start dump cp register!addr:%x,len:%d", addr, len);
	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}

	mdbg_dump_str(str, str_len, DUMP_STR_TYPE_REG);

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
		regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
	else
		regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);

	wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
	for (i = 0; i < len / 4; i++) {
		ptr = buf + i * 4;
		wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
	}
	count = 0;
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		mdbg_ring_write(mdev_ring, buf + count, trans_size);
		count += trans_size;
		wake_up_log_wait();
	}

	kfree(buf);
	WCN_INFO("dump cp register finish count %u\n", count);

	return count;
}

static void mdbg_dump_ap_register(void)
{
	mdbg_dump_ap_register_data(DUMP_REG_PMU_SLEEP_CTRL, sizeof(u32),
		"start_dump_pmu_sleep_ctrl_reg",
		strlen("start_dump_pmu_sleep_ctrl_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_PMU_SLEEP_STATUS,
		sizeof(u32), "start_dump_pmu_sleep_status_reg",
		strlen("start_dump_pmu_sleep_status_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_PMU_PD_WCN_SYS_CFG, sizeof(u32),
		"start_dump_pmu_wcn_sys_cfg_reg",
		strlen("start_dump_pmu_wcn_sys_cfg_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_PMU_PD_WIFI_WRAP_CFG,
		sizeof(u32), "start_dump_pmu_pd_wifi_wrap_reg",
		strlen("start_dump_pmu_pd_wifi_wrap_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_PMU_WCN_SYS_DSLP_ENA,
		sizeof(u32), "start_dump_pmu_wcn_sys_dslp_ena_reg",
		strlen("start_dump_pmu_wcn_sys_dslp_ena_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_PMU_WIFI_WRAP_DSLP_ENA,
		sizeof(u32), "start_dump_pmu_wifi_wrap_dslp_ena_reg",
		strlen("start_dump_pmu_wifi_wrap_dslp_ena_reg"));
	mdbg_dump_ap_register_data(DUMP_REG_AON_APB_WCN_SYS_CFG2, sizeof(u32),
		"start_dump_aon_apb_wcn_sys_cfg2_reg",
		strlen("start_dump_aon_apb_wcn_sys_cfg2_reg"));
}

static void mdbg_dump_cp_register(void)
{
	u32 count;

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_CTRL_ADDR,
			DUMP_REG_BTWF_CTRL_LEN,
			"start_dump_btwf_ctrl_reg",
			strlen("start_dump_btwf_ctrl_reg"));
	WCN_INFO("dump btwf_ctrl_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_AHB_CTRL_ADDR,
			DUMP_REG_BTWF_AHB_CTRL_LEN,
			"start_dump_ahb_ctrl_reg",
			strlen("start_dump_ahb_ctrl_reg"));
	WCN_INFO("dump ahb_ctrl_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_INTC_ADDR,
			DUMP_REG_BTWF_INTC_LEN,
			"start_dump_intc_reg",
			strlen("start_dump_intc_reg"));
	WCN_INFO("dump intc_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_SYSTEM_TIMER_ADDR,
			DUMP_REG_BTWF_SYSTEM_TIMER_LEN,
			"start_dump_systimer_reg",
			strlen("start_dump_systimer_reg"));
	WCN_INFO("dump systimer_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_TIMER0_ADDR,
			DUMP_REG_BTWF_TIMER0_LEN,
			"start_dump_timer0_reg",
			strlen("start_dump_timer0_reg"));
	WCN_INFO("dump imer0_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_TIMER1_ADDR,
			DUMP_REG_BTWF_TIMER1_LEN,
			"start_dump_timer1_reg",
			strlen("start_dump_timer1_reg"));
	WCN_INFO("dump timer1_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_TIMER2_ADDR,
			DUMP_REG_BTWF_TIMER2_LEN,
			"start_dump_timer2_reg",
			strlen("start_dump_timer2_reg"));
	WCN_INFO("dump timer2_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BTWF_WATCHDOG_ADDR,
			DUMP_REG_BTWF_WATCHDOG_LEN,
			"start_dump_wdg_reg",
			strlen("start_dump_wdg_reg"));
	WCN_INFO("dump wdg_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_COM_AHB_CTRL_ADDR,
			DUMP_REG_COM_AHB_CTRL_LEN,
			"start_dump_ahb_com_ctrl_reg",
			strlen("start_dump_ahb_com_ctrl_reg"));
	WCN_INFO("dump ahb_com_ctrl_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_MANU_CLK_CTRL_ADDR,
			DUMP_REG_MANU_CLK_CTRL_LEN,
			"start_dump_manu_clk_ctrl_reg",
			strlen("start_dump_manu_clk_ctrl_reg"));
	WCN_INFO("dump manu_clk_ctrl_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_WIFI_ADDR,
			DUMP_REG_WIFI_LEN,
			"start_dump_wifi_reg",
			strlen("start_dump_wifi_reg"));
	WCN_INFO("dump wifi_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_FM_ADDR,
			DUMP_REG_FM_LEN,
			"start_dump_fm_reg",
			strlen("start_dump_fm_reg"));
	WCN_INFO("dump fm_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BT_CMD_ADDR,
			DUMP_REG_BT_CMD_LEN,
			"start_dump_bt_cmd_reg",
			strlen("start_dump_bt_cmd_reg"));
	WCN_INFO("dump bt_cmd_reg %u ok!\n", count);

	count = mdbg_dump_cp_register_data(DUMP_REG_BT_ADDR,
			DUMP_REG_BT_LEN,
			"start_dump_bt_reg",
			strlen("start_dump_bt_reg"));
	WCN_INFO("dump bt_reg %u ok!\n", count);
}

static void mdbg_dump_register(void)
{
	mdbg_dump_ap_register();
	mdbg_dump_cp_register();
	WCN_INFO("dump register ok!\n");
}

static void mdbg_dump_iram(void)
{
	u32 count;

	count = mdbg_dump_cp_register_data(DUMP_IRAM_START_ADDR,
			MDBG_CP_IRAM_DATA_NUM * 4,
			NULL,
			0);
	count = mdbg_dump_cp_register_data(DUMP_GNSS_IRAM_START_ADDR,
			MDBG_CP_IRAM_DATA_NUM * 2,
			NULL,
			0);
	WCN_INFO("dump iram finish count %u!\n", count);
}

static int mdbg_dump_share_memory(u32 len)
{
	u32 count, trans_size;
	void *virt_addr;
	phys_addr_t base_addr;
	u32 time = 0;
	unsigned int cnt;

	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}
	if (len == 0)
		return -1;
	base_addr = wcn_get_btwf_base_addr();
	WCN_INFO("dump sharememory start!");
	WCN_INFO("ring->pbuff=%p, ring->end=%p.\n",
				mdev_ring->pbuff, mdev_ring->end);
	virt_addr = wcn_mem_ram_vmap_nocache(base_addr, len, &cnt);
	if (!virt_addr) {
		WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
		return -1;
	}
	count = 0;
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		/* copy data from ddr to ring buf  */

		mdbg_ring_write(mdev_ring, virt_addr + count, trans_size);
		count += trans_size;
		wake_up_log_wait();

		if (mdbg_ring_over_loop(
			mdev_ring, trans_size, MDBG_RING_W)) {
			WCN_INFO("ringbuf overloop:wait for read\n");
			while (!mdbg_ring_over_loop(
				mdev_ring, trans_size, MDBG_RING_R)) {
				msleep(DUMP_WAIT_TIMEOUT);
				time++;
				WCN_INFO("wait time %d\n", time);
				wake_up_log_wait();
				if (time > DUMP_WAIT_COUNT) {
					WCN_INFO("ringbuf overloop timeout!\n");
					break;
				}
			}
		}
	}
	wcn_mem_ram_unmap(virt_addr, cnt);
	WCN_INFO("share memory dump finish! total count %u\n", count);

	return 0;
}

void mdbg_dump_gnss_register(gnss_dump_callback callback_func, void *para)
{
	gnss_dump_handle = (gnss_dump_callback)callback_func;
	WCN_INFO("gnss_dump register success!\n");
}

void mdbg_dump_gnss_unregister(void)
{
	gnss_dump_handle = NULL;
}

static int btwf_dump_mem(void)
{
	u32 cp2_status = 0;
	phys_addr_t sleep_addr;

	if (wcn_get_btwf_power_status() == WCN_POWER_STATUS_OFF) {
		WCN_INFO("wcn power status off:can not dump btwf!\n");
		return -1;
	}

	mdbg_send("at+sleep_switch=0\r", strlen("at+sleep_switch=0\r") + 1,
		  MDBG_SUBTYPE_AT);
	msleep(500);
	sleep_addr = wcn_get_btwf_sleep_addr();
	wcn_read_data_from_phy_addr(sleep_addr, &cp2_status, sizeof(u32));
	mdev_ring = mdbg_dev->ring_dev->ring;
	mdbg_hold_cpu();
	msleep(100);
	mdbg_ring_reset(mdev_ring);
	mdbg_dump_share_memory(MDBG_SHARE_MEMORY_SIZE);
	mdbg_dump_iram();
	if (cp2_status == WCN_CP2_STATUS_DUMP_REG)
		mdbg_dump_register();

	return 0;
}

void mdbg_dump_mem(void)
{
	/* dump both btwf and gnss */
	/* dump btwf */
	btwf_dump_mem();
	/* dump gnss */
	if (gnss_dump_handle) {
		WCN_INFO("need dump gnss\n");
		gnss_dump_handle();
	}
	mdbg_dump_str("marlin_memdump_finish", strlen("marlin_memdump_finish"),
					DUMP_STR_TYPE_DATA);
}
