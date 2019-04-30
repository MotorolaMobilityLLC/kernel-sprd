/*
* Copyright (C) 2019 Unisoc Communications Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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

#include "csi_api.h"
#include "compat_sensor_drv.h"
#include "sprd_sensor_core.h"
#include "sprd_sensor_drv.h"
#include "power/sensor_power.h"
#include "cam_pw_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SENSOR_CORE: %d: %s line=%d: " fmt, \
	current->pid, __func__, __LINE__

static int sprd_sensor_mipi_if_open(struct sprd_sensor_file_tag *p_file,
				    struct sensor_if_cfg_tag *if_cfg)
{
	int ret = 0;

	ret = csi_api_open(if_cfg->bps_per_lane, if_cfg->phy_id,
			   if_cfg->lane_num, p_file->sensor_id);
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
	case SPRD_SENSOR_VDD_1000MV:
		volt_value = SPRD_SENSOR_VDD_1000MV_VAL;
		break;
	case SPRD_SENSOR_VDD_CLOSED:
	default:
		volt_value = 0;
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
	int ret = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_MOT_GPIO_TAG_E);
		if (ret)
			ret = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_CAMMOT_ID_E);
	}

	return ret;
}

static int sprd_sensor_io_set_avdd(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set avdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_AVDD_GPIO_TAG_E);
		if (ret)
			ret = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_CAMAVDD_ID_E);
	}

	return ret;
}

static int sprd_sensor_io_set_dvdd(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set dvdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_DVDD_GPIO_TAG_E);
		if (ret)
			ret = sprd_sensor_set_voltage(p_file->sensor_id,
					vdd_val,
					SENSOR_REGULATOR_CAMDVDD_ID_E);
	}
	return ret;
}

static int sprd_sensor_io_set_iovdd(struct sprd_sensor_file_tag *p_file,
				    unsigned long arg)
{
	int ret = 0;
	unsigned int vdd_val;

	ret = copy_from_user(&vdd_val, (unsigned int *)arg,
			     sizeof(unsigned int));
	if (ret == 0) {
		vdd_val = sprd_sensor_get_voltage_value(vdd_val);
		pr_debug("set iovdd %d\n", vdd_val);
		ret = sprd_sensor_set_voltage_by_gpio(p_file->sensor_id,
			vdd_val,
			SPRD_SENSOR_IOVDD_GPIO_TAG_E);
		if (ret)
			ret = sprd_sensor_set_voltage(p_file->sensor_id,
					      vdd_val,
					      SENSOR_REGULATOR_VDDIO_E);
	}
	return ret;
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

static int sprd_sensor_io_read_i2c(struct sprd_sensor_file_tag *p_file,
				   unsigned long arg)
{
	int ret = 0;
	struct sensor_reg_bits_tag reg;

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

	ret = copy_from_user(&regTab, (struct sensor_reg_tab_tag *)arg,
			     sizeof(regTab));
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

	pr_info("type %d open %d mipi state %d\n", if_cfg.if_type,
		if_cfg.is_open, p_file->mipi_state);
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

static int sprd_sensor_io_grc_write_i2c(struct sprd_sensor_file_tag *p_file,
					unsigned long arg)
{
	int ret = 0;
	struct sensor_i2c_tag i2c_tab;

	ret = copy_from_user(&i2c_tab, (struct sensor_i2c_tag *)arg,
			     sizeof(i2c_tab));
	if (ret == 0)
		ret = sprd_sensor_write_i2c(&i2c_tab, p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_grc_read_i2c(struct sprd_sensor_file_tag *p_file,
				       unsigned long arg)
{
	int ret = 0;
	struct sensor_i2c_tag i2c_tab;

	ret = copy_from_user(&i2c_tab, (struct sensor_i2c_tag *)arg,
			     sizeof(i2c_tab));
	if (ret == 0)
		ret = sprd_sensor_read_i2c(&i2c_tab, p_file->sensor_id);

	return ret;
}

static int sprd_sensor_io_muti_write_i2c(struct sprd_sensor_file_tag *p_file,
					unsigned long arg)
{
	int ret = 0;
	struct sensor_muti_aec_i2c_tag aec_i2c_tab;

	ret = copy_from_user(&aec_i2c_tab, (void __user *)arg,
			     sizeof(aec_i2c_tab));
	if (ret == 0)
		ret = sprd_sensor_write_muti_i2c(&aec_i2c_tab);

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
			ret = sensor_power_on((uint32_t *)p_file,
					      pwr_cfg.op_sensor_id,
					      &pwr_cfg.dev0,
					      &pwr_cfg.dev1, &pwr_cfg.dev2);
		} else {
			ret = sensor_power_off((uint32_t *)p_file,
					       pwr_cfg.op_sensor_id,
					       &pwr_cfg.dev0,
					       &pwr_cfg.dev1, &pwr_cfg.dev2);
		}
	}

	return ret;
}

static long sprd_sensor_file_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	int ret = 0;
	struct sprd_sensor_core_module_tag *p_mod;
	struct sprd_sensor_file_tag *p_file = file->private_data;

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
		ret = sprd_cam_pw_on();
		if (ret) {
			pr_err("sensor: mm power on err\n");
			atomic_dec(&p_mod->total_users);
			goto exit;
		}
		sprd_cam_domain_eb();
		__pm_stay_awake(&p_mod->ws);
	}
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
	struct sprd_sensor_file_tag *p_file = file->private_data;
	struct sprd_sensor_core_module_tag *p_mod = NULL;

	if (!p_file)
		return -EINVAL;

	p_mod = p_file->mod_data;
	if (!p_mod)
		return -EINVAL;

	sprd_sensor_set_mclk(&p_file->sensor_mclk, 0,
			     p_file->sensor_id);
	if (p_file->mipi_state == SPRD_SENSOR_MIPI_STATE_ON_E) {
		pr_info("sensor %d mipi close\n", p_file->sensor_id);
		ret = sprd_sensor_mipi_if_close(p_file);
	}
	if (atomic_dec_return(&p_mod->total_users) == 0) {
		sprd_cam_domain_disable();
		sprd_cam_pw_off();
		__pm_relax(&p_mod->ws);
	}
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
	.compat_ioctl = compat_sensor_k_ioctl,
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
	wakeup_source_init(&p_data->ws, "Camera Sensor Waklelock");
	atomic_set(&p_data->total_users, 0);
	sprd_sensor_register_driver();
	pr_info("sensor register\n");
	csi_api_mipi_phy_cfg();
	misc_register(&sensor_dev);
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
		wakeup_source_trash(&p_data->ws);
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
