/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : slp_mgr.c
 * Abstract : This file is a implementation for  sleep manager
 *
 * Authors	: sam.sun
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <misc/wcn_bus.h>

#include "slp_mgr.h"
#include "slp_sdio.h"

static struct slp_mgr_t slp_mgr;

struct slp_mgr_t *slp_get_info(void)
{
	return &slp_mgr;
}

void slp_mgr_drv_sleep(enum slp_subsys subsys, bool enable)
{
	mutex_lock(&(slp_mgr.drv_slp_lock));
	if (enable)
		slp_mgr.active_module &= ~(BIT(subsys));
	else
		slp_mgr.active_module |= (BIT(subsys));
	if ((slp_mgr.active_module == 0) &&
		(subsys > PACKER_RX)) {
		SLP_MGR_INFO("dt done,allow sleep\n");
		slp_allow_sleep();
		atomic_set(&(slp_mgr.cp2_state), STAY_SLPING);
	}
	mutex_unlock(&(slp_mgr.drv_slp_lock));
}

int slp_mgr_wakeup(enum slp_subsys subsys)
{
	int ret = 0;

	mutex_lock(&(slp_mgr.wakeup_lock));

	if (STAY_SLPING == (atomic_read(&(slp_mgr.cp2_state)))) {

		SLP_MGR_INFO("subsys-%d wakeup", subsys);

		ret = ap_wakeup_cp();

		/* for wakup success */
		ret = wait_for_completion_timeout(&(
				slp_mgr.wakeup_ack_completion),
					msecs_to_jiffies(3000));
		if (ret == 0) {
			SLP_MGR_ERR("wakeup fail, ret-%d !!!!", ret);
			/* avoid re-call wakeup api after wakeup fail */
			atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
			mutex_unlock(&(slp_mgr.wakeup_lock));
			return -ETIMEDOUT;
		}

		atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
	}

	mutex_unlock(&(slp_mgr.wakeup_lock));

	return 0;
}

int slp_mgr_init(void)
{
	SLP_MGR_INFO("%s enter\n", __func__);

	atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
	mutex_init(&(slp_mgr.drv_slp_lock));
	mutex_init(&(slp_mgr.wakeup_lock));
	init_completion(&(slp_mgr.wakeup_ack_completion));
	slp_pub_int_regcb();
#ifdef SLP_MGR_TEST
	slp_test_init();
#endif

	SLP_MGR_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(slp_mgr_init);

int slp_mgr_deinit(void)
{
	SLP_MGR_INFO("%s enter\n", __func__);
	atomic_set(&(slp_mgr.cp2_state), STAY_SLPING);
	slp_mgr.active_module = 0;
	mutex_destroy(&(slp_mgr.drv_slp_lock));
	mutex_destroy(&(slp_mgr.wakeup_lock));
	SLP_MGR_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(slp_mgr_deinit);
