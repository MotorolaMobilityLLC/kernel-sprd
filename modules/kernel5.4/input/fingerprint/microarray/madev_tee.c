 /*
 * Copyright (C) 2012-2022 Microarray Co., Ltd.
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

#include "madev.h"
#include <linux/wait.h>
#include <linux/freezer.h>
/*spdev use for recording the data for other use*/
static unsigned int irq, ret;
static unsigned int is_enable_irq;
static unsigned int u1_flag;
static unsigned int u2_flag;
static int irq_flag;
static unsigned int ma_drv_reg;
static unsigned int is_screen_on;
static unsigned int ma_speed;
static unsigned int int_pin_state;
static struct notifier_block notifier;
static struct spi_device *ma_spi_t;
static DECLARE_WAIT_QUEUE_HEAD(gWaitq);
static DECLARE_WAIT_QUEUE_HEAD(U1_Waitq);
static DECLARE_WAIT_QUEUE_HEAD(U2_Waitq);
struct wakeup_source *gProcessWakeLock;
struct work_struct gWork;
struct workqueue_struct *gWorkq;
static struct fprint_dev *sdev;
static struct input_dev *ma_input_t;

static LIST_HEAD(dev_list);
static DEFINE_MUTEX(dev_lock);
static DEFINE_MUTEX(drv_lock);
static DEFINE_MUTEX(ioctl_lock);
static DECLARE_WAIT_QUEUE_HEAD(drv_waitq);

static bool irq_enabled;
static unsigned int g_factory_flag;

static void mas_work(struct work_struct *pws)
{
	irq_flag = 1;
	__pm_wakeup_event(gProcessWakeLock, jiffies_to_msecs(2*HZ));
	wake_up(&gWaitq);
}

static irqreturn_t mas_interrupt(int irq, void *dev_id)
{
	pr_debug("microarray fingerprint interrupt happened!\n");
	#ifdef DOUBLE_EDGE_IRQ
	if (mas_get_interrupt_gpio(0) == 1) {
	/*TODO IRQF_TRIGGER_RISING*/
	} else {
	/*TODO IRQF_TRIGGER_FALLING*/
	}
	#else
	queue_work(gWorkq, &gWork);
	#endif
	return IRQ_HANDLED;
}

static void mas_set_input(void)
{
	struct input_dev *input = NULL;
	input = input_allocate_device();

	if (!input) {
		MALOGW("input_allocate_device failed.");
		return;
	}
	set_bit(EV_KEY, input->evbit);
	set_bit(FINGERPRINT_SWIPE_UP, input->keybit); /*单触*/
	set_bit(FINGERPRINT_SWIPE_DOWN, input->keybit);
	set_bit(FINGERPRINT_SWIPE_LEFT, input->keybit);
	set_bit(FINGERPRINT_SWIPE_RIGHT, input->keybit);
	set_bit(FINGERPRINT_TAP, input->keybit);
	set_bit(FINGERPRINT_DTAP, input->keybit);
	set_bit(FINGERPRINT_LONGPRESS, input->keybit);

	set_bit(KEY_POWER, input->keybit);

	input->name = MA_CHR_DEV_NAME;
	input->id.bustype = BUS_SPI;
	ret = input_register_device(input);
	if (ret != 0) {
		input_free_device(input);
		MALOGW("failed to register input device.");
		return;
	}
	ma_input_t  = input;
}

/* static int mas_ioctl (struct inode *node, struct file *filp, unsigned int cmd, uns igned long arg)
** this function only supported while the linux kernel version under v2.6.36,while the kernel version under v2.6.36, use
* this line
*/
static long mas_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ioctl_ret = 0;
	int cp_ret;
	unsigned int version = MA_DRV_VERSION;

	switch (cmd) {
	case MA_IOC_DELK:
		__pm_wakeup_event(gProcessWakeLock, jiffies_to_msecs(5*HZ));
		break;
	case MA_IOC_SLEP:
		irq_flag = 0;
		ioctl_ret = wait_event_freezekillable_unsafe(gWaitq,
				irq_flag != 0);
		return ioctl_ret;
	case MA_IOC_WKUP:
		/*wake up, schedule the process into the runqueue*/
		irq_flag = 1;
		wake_up(&gWaitq);
		break;
	case MA_IOC_ENCK:
		/*if the spi clock is not opening always, do this methods*/
		mas_enable_spi_clock(ma_spi_t);
		break;
	case MA_IOC_DICK:
		/*disable the spi clock*/
		mas_disable_spi_clock(ma_spi_t);
		break;
	case MA_IOC_EINT:
		if (irq_enabled) {
			/*irq_enabled = true;*/
			enable_irq(irq);
		}
		break;
	case MA_IOC_DINT:
		if (irq_enabled) {
			disable_irq_nosync(irq);
			/*irq_enabled = false;*/
		}
		break;
	case MA_IOC_TPDW:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 1);
		input_sync(ma_input_t); /*tap down*/
		break;
	case MA_IOC_TPUP:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 0);
		input_sync(ma_input_t);  /*tap up*/
		break;
	case MA_IOC_SGTP:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_TAP, 0);
		input_sync(ma_input_t);  /*single tap*/
		break;
	case MA_IOC_DBTP:
		input_report_key(ma_input_t, FINGERPRINT_DTAP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_DTAP, 0);
		input_sync(ma_input_t);	/*double tap*/
		break;
	case MA_IOC_LGTP:
		input_report_key(ma_input_t, FINGERPRINT_LONGPRESS, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_LONGPRESS, 0);
		input_sync(ma_input_t);	 /*long tap*/
		break;
	case MA_IOC_EIRQ:
		if (irq_enabled != true) {
			irq_enabled = true;
#ifdef IRQ_TRIGGER_HIGH
			ioctl_ret = request_irq(irq, mas_interrupt,
			IRQF_TRIGGER_HIGH|IRQF_NO_SUSPEND, MA_EINT_NAME,
			NULL);
#else
			ioctl_ret = request_irq(irq, mas_interrupt,
			IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND, MA_EINT_NAME,
			NULL);
#endif
			if (ioctl_ret < 0) {
				MALOGE("request irq");
				break;
			}
			enable_irq_wake(irq);
			is_enable_irq = ioctl_ret;
		}
		break;
	case MA_IOC_DIRQ:
		if (is_enable_irq > 0) {
		  free_irq(irq, NULL);
		}
		break;
	case MA_IOC_NAVW:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_UP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_UP, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVA:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_LEFT, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_LEFT, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVS:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_DOWN, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_DOWN, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVD:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_RIGHT, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_RIGHT, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_SPAR:
		mutex_lock(&ioctl_lock);
		ret = copy_from_user(&ma_drv_reg, (unsigned int *)arg, sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_GPAR:
		mutex_lock(&ioctl_lock);
		ret = copy_to_user((unsigned int *)arg, &ma_drv_reg, sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_PWOF:
		mas_switch_power(1);
		break;
	case MA_IOC_PWON:
		mas_switch_power(0);
		break;
	case MA_IOC_GVER:
		mutex_lock(&ioctl_lock);
		ret = copy_to_user((unsigned int *)arg, &version, sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_STSP:
		cp_ret = copy_from_user(&ma_speed, (unsigned int *)arg,
				sizeof(unsigned int));
		ma_spi_change(ma_spi_t, ma_speed, 0);
		break;
	case MA_IOC_FD_WAIT_CMD:
		u2_flag = 0;
		ioctl_ret = wait_event_interruptible(U2_Waitq, u2_flag != 0);
		break;
	case MA_IOC_TEST_WAKE_FD:
		u2_flag = 1;
		wake_up_interruptible(&U2_Waitq);
		break;
	case MA_IOC_TEST_WAIT_FD_RET:
		u1_flag = 0;
		ioctl_ret = wait_event_interruptible(U1_Waitq,  u1_flag != 0);
		mutex_lock(&ioctl_lock);
		cp_ret = copy_to_user((unsigned int *)arg, &ma_drv_reg,
				sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_FD_WAKE_TEST_RET:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_from_user(&ma_drv_reg, (unsigned int *)arg,
				sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		msleep(20);
		u1_flag = 1;
		wake_up_interruptible(&U1_Waitq);
		break;
	case MA_IOC_GET_SCREEN:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_to_user((unsigned int *)arg, &is_screen_on,
				sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_GET_INT_STATE:
		int_pin_state = mas_get_interrupt_gpio(0);
		if (int_pin_state == 0 || int_pin_state == 1) {
			mutex_lock(&ioctl_lock);
			cp_ret = copy_to_user((unsigned int *)arg,
				&int_pin_state, sizeof(unsigned int));
			mutex_unlock(&ioctl_lock);
		}
		break;
	case MA_IOC_GET_FACTORY_FLAG:
		mutex_lock(&ioctl_lock);
		ret = copy_to_user((unsigned int *)arg, &g_factory_flag,
				sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_SET_FACTORY_FLAG:
		mutex_lock(&ioctl_lock);
		ret = copy_from_user(&g_factory_flag, (unsigned int *)arg,
				sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	default:
		ioctl_ret = -EINVAL;
		MALOGW("mas_ioctl no such cmd");
	}
	return ioctl_ret;
}


#ifdef CONFIG_COMPAT
static long mas_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);
	return retval;
}
#endif

void *kernel_memaddr;
unsigned long kernel_memesize;

int mas_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long page;

	if (!kernel_memaddr) {
		kernel_memaddr = kmalloc(128*1024, GFP_KERNEL);
		if (!kernel_memaddr)
			return -1;
	}
	page = virt_to_phys((void *)kernel_memaddr) >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, page,
		(vma->vm_end - vma->vm_start), vma->vm_page_prot))
		return -1;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	return 0;
}


/*---------------------------------- fops start------------------------------------*/
static const struct file_operations sfops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mas_ioctl,
	.mmap = mas_mmap,
	/*using the previous line replacing the unlock_ioctl while the linux kernel under version2.6.36*/
	/*.ioctl = mas_ioctl,*/
#ifdef CONFIG_COMPAT
	.compat_ioctl = mas_compat_ioctl,
#endif

};
/*---------------------------------- fops end ---------------------------------*/

static int init_file_node(void)
{
	int ret;
	ret = alloc_chrdev_region(&sdev->idd, 0, 1, MA_CHR_DEV_NAME);
	if (ret < 0) {
		MALOGW("alloc_chrdev_region error!");
		return -1;
	}
	sdev->chd = cdev_alloc();
	if (!sdev->chd) {
		MALOGW("cdev_alloc error!");
		return -1;
	}
	sdev->chd->owner = THIS_MODULE;
	sdev->chd->ops = &sfops;
	ret = cdev_add(sdev->chd, sdev->idd, 1);
	if (ret) {
		MALOGE("cdev_add");
		return -1;
	}
	sdev->cls = class_create(THIS_MODULE, MA_CHR_DEV_NAME);
	if (IS_ERR(sdev->cls)) {
		MALOGE("class_create");
		return -1;
	}
	sdev->dev = device_create(sdev->cls, NULL, sdev->idd, NULL, MA_CHR_FILE_NAME);
	ret = IS_ERR(sdev->dev) ? PTR_ERR(sdev->dev) : 0;
	if (ret) {
		MALOGE("device_create");
		return -1;
	}
	return 0;
}

static int deinit_file_node(void)
{
	unregister_chrdev_region(sdev->idd, 1);
	cdev_del(sdev->chd);
	return 0;
}

static int init_interrupt(struct platform_device *pdev)
{
	irq = mas_get_irq(pdev);
	printk("MAFP_%s: mas_get_irq = %d\n", __func__, irq);
	if (!irq) {
		ret = 1;
		MALOGE("mas_get_irq");
	}

	return ret;
}
static int deinit_interrupt(void)
{
	return 0;
}


static int init_vars(void)
{
	sdev = kmalloc(sizeof(struct fprint_dev), GFP_KERNEL);
	if (sdev == NULL) {
		MALOGE("malloc sdev space failed!\n");
	}
	ma_drv_reg = 0;
//	wakeup_source_init(&gProcessWakeLock, "microarray_process_wakelock");
	gProcessWakeLock = wakeup_source_register(NULL, "microarray_process_wakelock");
	if (!gProcessWakeLock) {
		return -ENOMEM;
	}

	INIT_WORK(&gWork, mas_work);
	gWorkq = create_singlethread_workqueue("mas_workqueue");
	if (!gWorkq) {
		MALOGW("create_single_workqueue error!");
		return -ENOMEM;
	}
	return 0;
}
static int deinit_vars(void)
{
	if (gWorkq) {
		destroy_workqueue(gWorkq);
		gWorkq = NULL;
	}
//	wakeup_source_trash(&gProcessWakeLock);
	if (gProcessWakeLock) {
		wakeup_source_unregister(gProcessWakeLock);
		gProcessWakeLock = NULL;
	}
	return 0;
}

static int init_spi(struct spi_device *spi)
{
	ma_spi_t = spi;
	return 0;
}

static int deinit_spi(struct spi_device *spi)
{
	return 0;
}

static int mas_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;
	if (event != FB_EVENT_BLANK)  {
	  return 0;
	}
	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
		is_screen_on = 1;
		break;
	case FB_BLANK_POWERDOWN:
		is_screen_on = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int init_notifier_call(void);
static int deinit_notifier_call(void);
static int init_notifier_call(void)
{
	notifier.notifier_call = mas_fb_notifier_callback;
	fb_register_client(&notifier);
	is_screen_on = 1;
	return 0;
}

static int deinit_notifier_call(void)
{
	fb_unregister_client(&notifier);
	return 0;
}

int mas_plat_probe(struct platform_device *pdev)
{

	ret = mas_finger_get_gpio_info(pdev);
	if (ret) {
		MALOGE("mas_plat_probe do mas_finger_get_gpio_info");
	}
	ret = mas_finger_set_gpio_info(1);
	if (ret) {
		MALOGE("mas_plat_probe do mas_finger_set_gpio_info");
	}
	MALOGD("start");
	ret = init_vars();
	if (ret)
		goto err1;
	MALOGD("poing 1");
	ret = mas_set_enable_gpio(pdev);
	if (ret)
		goto err1;
	ret = init_interrupt(pdev);
	if (ret)
		goto err2;
	MALOGD("poing 2");
	ret = init_file_node();
	if (ret)
		goto err3;
	MALOGD("poing 3");
	ret = init_spi(NULL);
	if (ret)
		goto err4;
	MALOGD("poing 4");
	mas_set_input();
	MALOGF("end");
	ret = init_notifier_call();
	if (ret != 0)
		goto err5;

	return ret;

err5:
	deinit_notifier_call();
err4:
	deinit_spi(NULL);
err3:
	deinit_file_node();
err2:
	deinit_interrupt();
err1:
	deinit_vars();
	return ret;
}

int mas_plat_remove(struct platform_device *pdev)
{
	mas_finger_set_gpio_info(0);
	deinit_file_node();
	deinit_interrupt();
	deinit_vars();
	return 0;
}

static int __init mas_init(void)
{
	int ret = 0;
	irq_enabled = false;
#ifdef IRQ_TRIGGER_HIGH
	MALOGD("__init mas_init IRQ_TRIGGER_HIGH!");
#endif
	ret = mas_get_platform();
	if (ret) {
	 MALOGE("mas_get_platform");
	}
	return ret;
}

static void __exit mas_exit(void)
{
}

module_init(mas_init);
module_exit(mas_exit);

MODULE_AUTHOR("Microarray");
MODULE_DESCRIPTION("Driver for microarray fingerprint sensor");
MODULE_LICENSE("GPL");
