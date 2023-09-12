/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/i2c.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#include <video/sprd_mmsys_pw_domain.h>
#else
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

extern int sprd_cam_pw_on(struct generic_pm_domain *domain);
extern int sprd_cam_pw_off(struct generic_pm_domain *domain);
#endif


#include "csi_api.h"
#ifdef CONFIG_COMPAT
#include "compat_sensor_drv.h"
#endif
#include "sprd_sensor_core.h"
#include "sprd_sensor_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SENSOR_CORE: %d: %s line=%d: " fmt, \
	current->pid, __func__, __LINE__

static int csi_pattern = 0;

#ifdef CONFIG_COMPAT
struct compat_sensor_reg_tab_tag {
	compat_caddr_t sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint32_t reg_bits;
	uint32_t burst_mode;
};

struct compat_sensor_i2c_tag {
	compat_caddr_t i2c_data;
	uint16_t i2c_count;
	uint16_t slave_addr;
	uint16_t read_len;
};

struct compat_sensor_otp_data_info_tag {
	uint32_t size;
	compat_caddr_t data_ptr;
};

struct compat_sensor_otp_param_tag {
	uint32_t type;
	uint32_t start_addr;
	uint32_t len;
	compat_caddr_t buff;
	struct compat_sensor_otp_data_info_tag golden;
	struct compat_sensor_otp_data_info_tag awb;
	struct compat_sensor_otp_data_info_tag lsc;
};
#endif
static int sprd_sensor_mipi_if_open(struct sprd_sensor_file_tag *p_file,
				    struct sensor_if_cfg_tag *if_cfg)
{
	int ret = 0;

	if(if_cfg->lane_seq == 0xeeee)
		csi_pattern = 1;
	else
		csi_pattern = 0;

	ret = csi_api_open(if_cfg->bps_per_lane, if_cfg->phy_id,
			   if_cfg->lane_num, p_file->sensor_id, csi_pattern,
			   if_cfg->is_cphy, if_cfg->lane_seq);
	if (ret) {
		pr_err("fail to open csi %d\n", ret);
		return ret;
	}
	p_file->mipi_state = SPRD_SENSOR_MIPI_STATE_ON_E;
	p_file->phy_id = if_cfg->phy_id;
	p_file->if_type = SPRD_SENSOR_INTERFACE_MIPI_E;
	pr_info("open csi successfully\n");

	return ret;
}

static int sprd_sensor_mipi_if_switch(struct sprd_sensor_file_tag *p_file,
				      struct sensor_if_cfg_tag *if_cfg)
{
	int ret = 0;

	ret = csi_api_switch(p_file->sensor_id);
	if (ret) {
		pr_err("fail to open csi %d\n", ret);
		return ret;
	}
	pr_debug("switch csi successfully\n");

	return ret;
}

static int sprd_sensor_mipi_if_close(struct sprd_sensor_file_tag *p_file)
{
	int ret = 0;

	ret = csi_api_close(p_file->phy_id, p_file->sensor_id);
	p_file->mipi_state = SPRD_SENSOR_MIPI_STATE_OFF_E;

	return ret;
}

static unsigned int sprd_sensor_get_voltage_value(unsigned int vdd_val)
{
	unsigned int volt_value = 0;

	switch (vdd_val) {

	case SPRD_SENSOR_VDD_3800MV:
		volt_value = SPRD_SENSOR_VDD_3800MV_VAL;
		break;
	case SPRD_SENSOR_VDD_3300MV:
		volt_value = SPRD_SENSOR_VDD_3300MV_VAL;
		break;
	case SPRD_SENSOR_VDD_3000MV:
		volt_value = SPRD_SENSOR_VDD_3000MV_VAL;
		break;
	case SPRD_SENSOR_VDD_2800MV:
		volt_value = SPRD_SENSOR_VDD_2800MV_VAL;
		break;
	case SPRD_SENSOR_VDD_2500MV:
		volt_value = SPRD_SENSOR_VDD_2500MV_VAL;
		break;
	case SPRD_SENSOR_VDD_2000MV:
		volt_value = SPRD_SENSOR_VDD_2000MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1800MV:
		volt_value = SPRD_SENSOR_VDD_1800MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1500MV:
		volt_value = SPRD_SENSOR_VDD_1500MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1300MV:
		volt_value = SPRD_SENSOR_VDD_1300MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1200MV:
		volt_value = SPRD_SENSOR_VDD_1200MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1100MV:
		volt_value = SPRD_SENSOR_VDD_1100MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1050MV:
		volt_value = SPRD_SENSOR_VDD_1050MV_VAL;
		break;
	case SPRD_SENSOR_VDD_1000MV:
		volt_value = SPRD_SENSOR_VDD_1000MV_VAL;
		break;
	case SPRD_SENSOR_VDD_CLOSED:
		volt_value = 0;
		break;
	default:
		volt_value = vdd_val * 1000;
		break;
	}

	return volt_value;
}

static int sprd_sensor_io_set_pd(struct sprd_sensor_file_tag *p_file,
				 unsigned long arg)
{
	int ret = 0;
	unsigned char power_level;

	ret = copy_from_user(&power_level, (unsigned char *)arg,
			     sizeof(unsigned char));
	if (ret == 0)
		ret = sprd_sensor_set_pd_level(p_file->sensor_id, power_level);
	return ret;
}

static int sprd_sensor_io_set_cammot(struct sprd_sensor_file_tag *p_file,
				     unsigned long arg)
{
	int ret = 0, ret1 = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_MOT_GPIO_TAG_E);
		//if (ret)
			ret1 = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_CAMMOT_ID_E);
	}

	return ret & ret1;
}

static int sprd_sensor_io_set_avdd(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0, ret1 = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set avdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_AVDD_GPIO_TAG_E);
	//	if (ret)
			ret1 = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_CAMAVDD_ID_E);
	}

	return ret & ret1;
}

static int sprd_sensor_io_set_dvdd(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0, ret1 = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set dvdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_DVDD_GPIO_TAG_E);
		//if (ret)
			ret1 = sprd_sensor_set_voltage(p_file->sensor_id,
					vdd_val,
					SENSOR_REGULATOR_CAMDVDD_ID_E);
	}
	return ret & ret1;
}

static int sprd_sensor_io_set_iovdd(struct sprd_sensor_file_tag *p_file,
				    unsigned long arg)
{
	int ret = 0, ret1 = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set iovdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_IOVDD_GPIO_TAG_E);
	//	if (ret)
			ret1 = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_VDDIO_E);
	}
	return ret & ret1;
}

static int sprd_sensor_io_set_mclk(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0;
	unsigned int mclk;

	ret = copy_from_user(&mclk, (unsigned int *)arg, sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_set_mclk(&p_file->sensor_mclk, mclk,
					   p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_set_reset(struct sprd_sensor_file_tag *p_file,
				    unsigned long arg)
{
	int ret = 0;
	unsigned int rst_val[2];

	ret = copy_from_user(rst_val, (unsigned int *)arg,
			     2 * sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_reset(p_file->sensor_id, rst_val[0],
					rst_val[1]);

	return ret;
}

static int sprd_sensor_io_set_reset_level(struct sprd_sensor_file_tag *p_file,
					  unsigned long arg)
{
	int ret = 0;
	unsigned int level;

	ret = copy_from_user(&level, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_set_rst_level(p_file->sensor_id, level);

	return ret;
}

static int sprd_sensor_io_set_mipi_switch(struct sprd_sensor_file_tag *p_file,
					  unsigned long arg)
{
	int ret = 0;
	unsigned int level;

	ret = copy_from_user(&level, (unsigned int *)arg, sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_set_mipi_level(p_file->sensor_id, level);

	return ret;
}

static int sprd_sensor_io_set_i2c_addr(struct sprd_sensor_file_tag *p_file,
				       unsigned long arg)
{
	int ret = 0;
	unsigned short i2c_addr;

	ret = copy_from_user(&i2c_addr, (unsigned short *)arg,
			     sizeof(unsigned short));
	if (ret != 0)
		return ret;
	/* this addr means using csi pattern */
	if (i2c_addr == 0xee) {
		pr_info("enable csi testpattern mode!\n");
		csi_pattern = 1;
		return ret;
	}

	if (ret == 0)
		ret = sprd_sensor_set_i2c_addr(p_file->sensor_id, i2c_addr);

	return ret;
}

static int sprd_sensor_io_set_i2c_clk(struct sprd_sensor_file_tag *p_file,
				      unsigned long arg)
{
	int ret = 0;
	unsigned int clock;

	ret = copy_from_user(&clock, (unsigned int *)arg, sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_set_i2c_clk(p_file->sensor_id, clock);

	return ret;
}

static int sprd_sensor_io_set_i2c_burst(struct sprd_sensor_file_tag *p_file,
				      unsigned long arg)
{
	int ret = 0;
	unsigned int burst_mode;

	ret = copy_from_user(&burst_mode, (unsigned int *)arg, sizeof(unsigned int));
	if (ret == 0)
		ret = sprd_sensor_set_i2c_burst(p_file->sensor_id, burst_mode);

	return ret;
}

static int sprd_sensor_io_read_i2c(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0;
	struct sensor_reg_bits_tag reg;

	if (csi_pattern)
		return 0;

	ret = copy_from_user(&reg, (struct sensor_reg_bits_tag *)arg,
			     sizeof(reg));

	ret = sprd_sensor_read_reg(p_file->sensor_id, &reg);
	if (ret == 0)
		ret = copy_to_user((struct sensor_reg_bits_tag *)arg, &reg,
				   sizeof(reg));

	return ret;
}

static int sprd_sensor_io_write_i2c(struct sprd_sensor_file_tag *p_file,
				    unsigned long arg)
{
	int ret = 0;
	struct sensor_reg_bits_tag reg;

	if (csi_pattern)
		return 0;

	ret = copy_from_user(&reg, (struct sensor_reg_bits_tag *)arg,
			     sizeof(reg));
	if (ret == 0)
		ret = sprd_sensor_write_reg(p_file->sensor_id, &reg);

	return ret;
}

static int sprd_sensor_io_write_i2c_regs(struct sprd_sensor_file_tag *p_file,
					 unsigned long arg)
{
	int ret = 0;
	struct sensor_reg_tab_tag regTab;
#ifdef CONFIG_COMPAT
	struct compat_sensor_reg_tab_tag __user *uparam;
	u32 sensor_reg_tab_ptr;
	uparam = (struct compat_sensor_reg_tab_tag __user *)arg;
	ret |= get_user(regTab.reg_count, &uparam->reg_count);
	ret |= get_user(regTab.reg_bits, &uparam->reg_bits);
	ret |= get_user(regTab.burst_mode, &uparam->burst_mode);
	ret |= get_user(sensor_reg_tab_ptr, &uparam->sensor_reg_tab_ptr);
	regTab.sensor_reg_tab_ptr = (struct sensor_reg_tag *)(unsigned long)sensor_reg_tab_ptr;
#else
	ret = copy_from_user(&regTab, (struct sensor_reg_tab_tag *)arg,//(void __user *)arg,//(struct sensor_reg_tab_tag *)arg,
			     sizeof(regTab));
#endif
	if (csi_pattern)
		return 0;
	if (ret == 0)
		ret = sprd_sensor_write_regtab(&regTab, p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_if_cfg(struct sprd_sensor_file_tag *p_file,
				 unsigned long arg)
{
	int ret = 0;
	struct sensor_if_cfg_tag if_cfg;

	ret = copy_from_user((void *)&if_cfg, (struct sensor_if_cfg_tag *)arg,
			     sizeof(if_cfg));
	if (ret)
		return ret;

	pr_info("ret = %d,type %d open %d mipi state %d  is cphy %d\n",
		ret, if_cfg.if_type, if_cfg.is_open, p_file->mipi_state, if_cfg.is_cphy);
	if (if_cfg.if_type == SPRD_SENSOR_INTERFACE_MIPI_E) {
		if (if_cfg.is_open == SPRD_SENSOR_INTERFACE_OPEN) {
			if (p_file->mipi_state == SPRD_SENSOR_MIPI_STATE_OFF_E)
				ret = sprd_sensor_mipi_if_open(p_file, &if_cfg);
			else
				pr_debug("mipi already on\n");
		} else {
			if (p_file->mipi_state == SPRD_SENSOR_MIPI_STATE_ON_E)
				ret = sprd_sensor_mipi_if_close(p_file);
			else
				pr_debug("mipi already off\n");
		}
	}

	return ret;
}

static int sprd_sensor_io_if_switch(struct sprd_sensor_file_tag *p_file,
				 unsigned long arg)
{
	int ret = 0;
	struct sensor_if_cfg_tag if_cfg;

	ret = copy_from_user((void *)&if_cfg, (struct sensor_if_cfg_tag *)arg,
			     sizeof(if_cfg));
	if (ret)
		return -EFAULT;

	ret = sprd_sensor_mipi_if_switch(p_file, &if_cfg);

	return ret;
}

static int sprd_sensor_io_if_dump(struct sprd_sensor_file_tag *p_file,
				 unsigned long arg)
{
	int ret = 0;
	pr_info("E\n");
	csi_api_reg_trace();

	return ret;
}

static int sprd_sensor_io_grc_write_i2c(struct sprd_sensor_file_tag *p_file,
					unsigned long arg)
{
	int ret = 0;
	struct sensor_i2c_tag i2c_tab;
#ifdef CONFIG_COMPAT
	struct compat_sensor_i2c_tag __user *uparam;
	u32 i2c_data;
	uparam = (struct compat_sensor_i2c_tag __user *)arg;
	ret |= get_user(i2c_tab.i2c_count, &uparam->i2c_count);
	ret |= get_user(i2c_tab.slave_addr, &uparam->slave_addr);
	ret |= get_user(i2c_tab.read_len, &uparam->read_len);
	ret |= get_user(i2c_data, &uparam->i2c_data);
	i2c_tab.i2c_data = (uint8_t  *)(unsigned long)i2c_data;
#else
	ret = copy_from_user(&i2c_tab, (struct sensor_i2c_tag *)arg,
			     sizeof(i2c_tab));
#endif
	if (csi_pattern)
		return 0;

	if (ret == 0)
		ret = sprd_sensor_write_i2c(&i2c_tab, p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_grc_read_i2c(struct sprd_sensor_file_tag *p_file,
				       unsigned long arg)
{
	int ret = 0;
	struct sensor_i2c_tag i2c_tab;
#ifdef CONFIG_COMPAT
	struct compat_sensor_i2c_tag __user *uparam;
	u32 i2c_data;
	uparam = (struct compat_sensor_i2c_tag __user *)arg;
	ret |= get_user(i2c_tab.i2c_count, &uparam->i2c_count);
	ret |= get_user(i2c_tab.slave_addr, &uparam->slave_addr);
	ret |= get_user(i2c_tab.read_len, &uparam->read_len);
	ret |= get_user(i2c_data, &uparam->i2c_data);
	i2c_tab.i2c_data = (uint8_t  *)(unsigned long)i2c_data;
#else
	ret = copy_from_user(&i2c_tab, (struct sensor_i2c_tag *)arg,
			     sizeof(i2c_tab));
#endif
	if (csi_pattern)
		return 0;

	if (ret == 0)
		ret = sprd_sensor_read_i2c(&i2c_tab, p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_muti_write_i2c(struct sprd_sensor_file_tag *p_file,
					unsigned long arg)
{
	int ret = 0;
	struct sensor_muti_aec_i2c_tag aec_i2c_tab = {0};

	ret = copy_from_user(&aec_i2c_tab, (void __user *)arg,
			     sizeof(aec_i2c_tab));

	if (csi_pattern)
		return 0;

	if (ret == 0)
		ret = sprd_sensor_write_muti_i2c(&aec_i2c_tab);

	if (ret == 0)
		ret = copy_to_user((struct sensor_muti_aec_i2c_tag *)arg, &aec_i2c_tab,
				   sizeof(aec_i2c_tab));

	return ret;
}

static int sprd_sensor_io_power_cfg(struct sprd_sensor_file_tag *p_file,
				    unsigned long arg)
{
	int ret = 0;
	struct sensor_power_info_tag pwr_cfg;

	ret = copy_from_user(&pwr_cfg, (struct sensor_power_info_tag *)arg,
			     sizeof(struct sensor_power_info_tag));
	if (ret == 0) {
		if (pwr_cfg.is_on) {
			/*ret = sensor_power_on((uint32_t *)p_file,
					      pwr_cfg.op_sensor_id,
					      &pwr_cfg.dev0,
					      &pwr_cfg.dev1, &pwr_cfg.dev2);*/
		} else {
			/*ret = sensor_power_off((uint32_t *)p_file,
					       pwr_cfg.op_sensor_id,
					       &pwr_cfg.dev0,
					       &pwr_cfg.dev1, &pwr_cfg.dev2);*/
		}
	}

	return ret;
}

static int sprd_sensor_io_set_private_key(struct sprd_sensor_file_tag *p_file,
					unsigned long arg)
{
	int ret = 0;
	unsigned int private_key;

	ret = copy_from_user(&private_key, (unsigned int *)arg, sizeof(unsigned int));
	if(ret) {
		pr_info("copy private_key from user failed.\n");
		return ret;
	}

	if(private_key == SENSOR_IOC_PRIVATE_KEY) {
		p_file->private_key = 1;
		pr_info("sensor set private key successfully\n");
        } else {
        	pr_info("sensor set private key faild\n");
        	return 1;
        }
	return ret;
}

static long sprd_sensor_file_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	int ret = 0;
	struct sprd_sensor_core_module_tag *p_mod;
	struct sprd_sensor_file_tag *p_file = file->private_data;

	if(cmd == SENSOR_IO_PRI_KEY) {
		ret = sprd_sensor_io_set_private_key(p_file, arg);
          	return ret;
	}
  
	if(p_file->private_key != 1) {
		pr_info("sensor match private key failed, permission deny!\n");
		return ret;
	}

	p_mod = p_file->mod_data;
	if (cmd == SENSOR_IO_SET_ID) {
		mutex_lock(&p_mod->sensor_id_lock);
		ret = copy_from_user(&p_file->sensor_id, (unsigned int *)arg,
				     sizeof(unsigned int));
		pr_debug("sensor id %d cmd 0x%x\n", p_file->sensor_id, cmd);

		mutex_unlock(&p_mod->sensor_id_lock);
	}


	sprd_sensor_sync_lock(p_file->sensor_id);
	switch (cmd) {
	case SENSOR_IO_PD:
		ret = sprd_sensor_io_set_pd(p_file, arg);
		break;
	case SENSOR_IO_SET_CAMMOT:
		ret = sprd_sensor_io_set_cammot(p_file, arg);
		break;
	case SENSOR_IO_SET_AVDD:
		ret = sprd_sensor_io_set_avdd(p_file, arg);
		break;
	case SENSOR_IO_SET_DVDD:
		ret = sprd_sensor_io_set_dvdd(p_file, arg);
		break;
	case SENSOR_IO_SET_IOVDD:
		ret = sprd_sensor_io_set_iovdd(p_file, arg);
		break;
	case SENSOR_IO_SET_MCLK:
		ret = sprd_sensor_io_set_mclk(p_file, arg);
		break;
	case SENSOR_IO_RST:
		ret = sprd_sensor_io_set_reset(p_file, arg);
		break;
	case SENSOR_IO_RST_LEVEL:
		ret = sprd_sensor_io_set_reset_level(p_file, arg);
		break;
	case SENSOR_IO_SET_MIPI_SWITCH:
		ret = sprd_sensor_io_set_mipi_switch(p_file, arg);
		break;
	case SENSOR_IO_I2C_ADDR:
		ret = sprd_sensor_io_set_i2c_addr(p_file, arg);
		break;
	case SENSOR_IO_SET_I2CCLOCK:
		ret = sprd_sensor_io_set_i2c_clk(p_file, arg);
		break;
	case SENSOR_IO_I2C_READ:
		ret = sprd_sensor_io_read_i2c(p_file, arg);
		break;
	case SENSOR_IO_I2C_WRITE:
		ret = sprd_sensor_io_write_i2c(p_file, arg);
		break;
	case SENSOR_IO_I2C_WRITE_REGS:
		ret = sprd_sensor_io_write_i2c_regs(p_file, arg);
		break;
	case SENSOR_IO_IF_CFG:
		ret = sprd_sensor_io_if_cfg(p_file, arg);
		break;
	case SENSOR_IO_IF_SWITCH:
		ret = sprd_sensor_io_if_switch(p_file, arg);
		break;
	case SENSOR_IO_IF_DUMP:
		ret = sprd_sensor_io_if_dump(p_file, arg);
		break;
	case SENSOR_IO_GRC_I2C_WRITE:
		ret = sprd_sensor_io_grc_write_i2c(p_file, arg);
		break;
	case SENSOR_IO_GRC_I2C_READ:
		ret = sprd_sensor_io_grc_read_i2c(p_file, arg);
		break;
	case SENSOR_IO_MUTI_I2C_WRITE:
		ret = sprd_sensor_io_muti_write_i2c(p_file, arg);
		break;
	case SENSOR_IO_POWER_CFG:
		ret = sprd_sensor_io_power_cfg(p_file, arg);
		break;
	case SENSOR_IO_SET_I2CBURST:
		ret = sprd_sensor_io_set_i2c_burst(p_file, arg);
		break;
	}
	sprd_sensor_sync_unlock(p_file->sensor_id);

	return ret;
}

static int sprd_sensor_file_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct sprd_sensor_file_tag *p_file;
	struct sprd_sensor_core_module_tag *p_mod;
	struct miscdevice *md = (struct miscdevice *)file->private_data;
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(0);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

	p_file = NULL;
	if (!md) {
		ret = -EFAULT;
		pr_err("sensor misc device not found\n");
		goto exit;
	}

	p_mod = md->this_device->platform_data;
	if (!p_mod) {
		ret = -EFAULT;
		pr_err("sensor: no module data\n");
		goto exit;
	}

	p_file = kzalloc(sizeof(*p_file), GFP_KERNEL);
	if (!p_file) {
		ret = -ENOMEM;
		pr_err("sensor: no memory\n");
		goto exit;
	}

	if (atomic_inc_return(&p_mod->total_users) == 1) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		ret = sprd_cam_pw_on();
		if (ret) {
			pr_err("sensor: mm power on err\n");
			atomic_dec(&p_mod->total_users);
			goto exit;
		}
		sprd_cam_domain_eb();
#else
		ret = pm_runtime_get_sync(&p_dev->i2c_info->dev);
#endif
		__pm_stay_awake(p_mod->ws);
/*		wake_lock(&p_mod->wakelock);*/

	}
	p_file->sensor_id = SPRD_SENSOR_ID_INIT;
	p_file->private_key = 0;
	p_file->md = md;
	file->private_data = p_file;
	p_file->mod_data = p_mod;
	pr_info("open sensor file successfully\n");

	return ret;

exit:
	pr_err("fail to open sensor file %d\n", ret);
	if (p_file) {
		kfree(p_file);
		p_file = NULL;
	}

	return ret;
}

static int sprd_sensor_file_release(struct inode *node, struct file *file)
{
	int ret = 0;
	int i = 0;
	struct sprd_sensor_file_tag *p_file = file->private_data;
	struct sprd_sensor_core_module_tag *p_mod = NULL;
	int power[3][2] = {
		{SPRD_SENSOR_AVDD_GPIO_TAG_E, SENSOR_REGULATOR_CAMAVDD_ID_E},
		{SPRD_SENSOR_DVDD_GPIO_TAG_E, SENSOR_REGULATOR_CAMDVDD_ID_E},
		{SPRD_SENSOR_IOVDD_GPIO_TAG_E, SENSOR_REGULATOR_VDDIO_E},
	};
	struct sprd_sensor_dev_info_tag *p_dev = NULL;

	p_dev = sprd_sensor_get_dev_context(0);
	if (!p_dev || !p_dev->i2c_info) {
		pr_err("%s, error\n", __func__);
		return -EINVAL;
	}

	if (!p_file)
		return -EINVAL;

	p_mod = p_file->mod_data;
	if (!p_mod)
		return -EINVAL;

	sprd_sensor_set_mclk(&p_file->sensor_mclk, 0,
			     p_file->sensor_id);
	if (p_file->mipi_state == SPRD_SENSOR_MIPI_STATE_ON_E) {
		pr_info("sensor %d mipi close\n", p_file->sensor_id);
		#ifndef MCLK_NEW_PROCESS1
			ret = sprd_sensor_mipi_if_close(p_file);
		#endif
	}
	for (i = 0; i < 3; i++) {
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
				0, power[i][0]);
		if (ret)
			ret = sprd_sensor_set_voltage(p_file->sensor_id,
					0, power[i][1]);
	}
	if (atomic_dec_return(&p_mod->total_users) == 0) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		sprd_cam_domain_disable();
		sprd_cam_pw_off();
#else
		pm_runtime_put_sync(&p_dev->i2c_info->dev);
#endif
		__pm_relax(p_mod->ws);
/*		wake_unlock(&p_mod->wakelock);*/

	}
	p_file->private_key = 0;
	kfree(p_file);
	p_file = NULL;
	file->private_data = NULL;
	pr_info("sensor: release %d\n", ret);

	return ret;
}

void sprd_sensor_free(struct sprd_sensor_mem_tag *mem_ptr)
{
	if (mem_ptr->buf_ptr != NULL) {
		kfree(mem_ptr->buf_ptr);
		mem_ptr->buf_ptr = NULL;
		mem_ptr->size = 0;
	}
}

static const struct file_operations sensor_fops = {
	.owner = THIS_MODULE,
	.open = sprd_sensor_file_open,
	.unlocked_ioctl = sprd_sensor_file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_sensor_k_ioctl,
#endif
	.release = sprd_sensor_file_release,
};

static struct miscdevice sensor_dev = {
	.minor = SPRD_SENSOR_MINOR,
	.name = SPRD_SENSOR_DEVICE_NAME,
	.fops = &sensor_fops,
};

static char _sensor_type_info[255];
static ssize_t sprd_get_sensor_name_info(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{

	pr_info("sprd_sensor: _sensor_type_info %s\n", _sensor_type_info);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  _sensor_type_info);
}

static ssize_t sprd_sensor_set_sensor_name_info(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t size)
{
	if (strlen(buf) >= 255) {
		pr_err("out of the maxnum 255.\n");
		return -EINVAL;
	}
	memset(_sensor_type_info, 0, 255);
	memcpy(_sensor_type_info, buf, strlen(buf));
	return size;

}

static DEVICE_ATTR(camera_sensor_name, 0644, sprd_get_sensor_name_info,
		   sprd_sensor_set_sensor_name_info);
static int sprd_sensor_core_module_init(void)
{
	struct sprd_sensor_core_module_tag *p_data = NULL;
	int ret = 0;

	p_data = kzalloc(sizeof(*p_data), GFP_KERNEL);
	if (!p_data)
		return -ENOMEM;
	mutex_init(&p_data->sensor_id_lock);
	/*
	wake_lock_init(&p_data->wakelock, WAKE_LOCK_SUSPEND,
		       "Camera Sensor Waklelock");
		       */
	p_data->ws = wakeup_source_create("Camera Sensor Wakeup Source");
	wakeup_source_add(p_data->ws);

	atomic_set(&p_data->total_users, 0);
	sprd_sensor_register_driver();
	pr_info("sensor register\n");
	csi_api_mipi_phy_cfg();
	ret = misc_register(&sensor_dev);
	pr_info("create device node\n");
	sensor_dev.this_device->platform_data = (void *)p_data;
	ret = device_create_file(sensor_dev.this_device,
				 &dev_attr_camera_sensor_name);
	if (ret < 0)
		pr_err("fail to create sensor name list file");

	return 0;
}

static void sprd_sensor_core_module_exit(void)
{
	struct sprd_sensor_core_module_tag *p_data = NULL;

	p_data = sensor_dev.this_device->platform_data;

	device_remove_file(sensor_dev.this_device,
			   &dev_attr_camera_sensor_name);
	sprd_sensor_unregister_driver();
	if (p_data) {
		mutex_destroy(&p_data->sensor_id_lock);
		wakeup_source_remove(p_data->ws);
		wakeup_source_destroy(p_data->ws);
		kfree(p_data);
		p_data = NULL;
	}
	sensor_dev.this_device->platform_data = NULL;
	misc_deregister(&sensor_dev);
}

int sprd_sensor_malloc(struct sprd_sensor_mem_tag *mem_ptr, unsigned int size)
{
	int ret = 0;

	if (mem_ptr->buf_ptr == NULL) {
		mem_ptr->buf_ptr = kzalloc(size, GFP_KERNEL);
		if (mem_ptr->buf_ptr != NULL)
			mem_ptr->size = size;
		else
			ret = -ENOMEM;
	} else if (size > mem_ptr->size) {
		kfree(mem_ptr->buf_ptr);
		mem_ptr->buf_ptr = NULL;
		mem_ptr->size = 0;
		mem_ptr->buf_ptr = kzalloc(size, GFP_KERNEL);
		if (mem_ptr->buf_ptr != NULL)
			mem_ptr->size = size;
		else
			ret = -ENOMEM;
	}
	return ret;
}

module_init(sprd_sensor_core_module_init);
module_exit(sprd_sensor_core_module_exit);
MODULE_DESCRIPTION("Spreadtrum Camera Sensor Driver");
MODULE_LICENSE("GPL");
