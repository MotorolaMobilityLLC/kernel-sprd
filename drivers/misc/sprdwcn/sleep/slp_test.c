#include <linux/delay.h>
#include <linux/kthread.h>
#include <misc/wcn_bus.h>

#include "slp_mgr.h"
#include "slp_sdio.h"
#include "wcn_glb_reg.h"

static int test_cnt;
static int sleep_test_thread(void *data)
{
	unsigned int ram_val;

	while (1) {
		if (test_cnt)
			msleep(5000);
		else
			msleep(30000);

		slp_mgr_drv_sleep(MARLIN_GNSS, FALSE);
		slp_mgr_wakeup(MARLIN_GNSS);

		sprdwcn_bus_reg_read(CP_START_ADDR, &ram_val, 0x4);
		SLP_MGR_INFO("ram_val is 0x%x\n", ram_val);

		msleep(5000);
		slp_mgr_drv_sleep(MARLIN_GNSS, TRUE);
		test_cnt++;
	}

	return 0;
}

static struct task_struct *slp_test_task;
int slp_test_init(void)
{
	SLP_MGR_INFO("create slp_mgr test thread\n");
	if (!slp_test_task)
		slp_test_task = kthread_create(sleep_test_thread,
			NULL, "sleep_test_thread");
	if (slp_test_task) {
		wake_up_process(slp_test_task);
		return 0;
	}

	SLP_MGR_ERR("create sleep_test_thread fail\n");

	return -1;
}
