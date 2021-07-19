#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sched/types.h>

#define INV_CPUS 256
static DEFINE_SPINLOCK(thread_lock);

static struct task_struct *pthread;
wait_queue_head_t waitq;
static unsigned int ipi_cpu = INV_CPUS;
static unsigned int thread_cpu = INV_CPUS;
static unsigned int state;
static unsigned int thread_state;

static ssize_t ipi_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = sprintf(buf, "%u\n", ipi_cpu);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ipi_write(struct file *file,
			     const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input == ipi_cpu)
		return count;

	if (input > nr_cpu_ids)
		return -EINVAL;

	ipi_cpu = input;

	return count;
}

static ssize_t ipi_trigger_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = sprintf(buf, "%u\n", state);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}


static ssize_t ipi_trigger_write(struct file *file,
			      const char __user *user_buf, size_t count,
			      loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input > 1)
		return -EINVAL;

	state = input;

	/* send IPI to the designated cpu*/
	if (input == 1) {
		if (ipi_cpu != INV_CPUS &&
		    cpu_online(ipi_cpu))
			smp_send_reschedule(ipi_cpu);
	}

	return count;
}

bool ipi_is_triggered(unsigned int cpu)
{
	if (cpu != ipi_cpu)
		return false;

	return (state == 1) ? true : false;
}

static int loop_thread_func(void *data)
{
	unsigned long flags;

	while (!kthread_should_stop()) {
		wait_event_interruptible(waitq,
					 thread_state == 1 ||
					 kthread_should_stop());

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&thread_lock, flags);
		while (1)
			cpu_relax();
		spin_unlock_irqrestore(&thread_lock, flags);

		/* impossible to come here */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}

static ssize_t thread_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = sprintf(buf, "%u\n", thread_cpu);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t thread_write(struct file *file,
			     const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input == thread_cpu)
		return count;

	if (input > nr_cpu_ids)
		return -EINVAL;

	/* stop bounded thread on previous cpu*/
	if (thread_cpu != INV_CPUS && pthread) {
		kthread_stop(pthread);
		pthread = NULL;
	}

	thread_cpu = input;

	if (!pthread && cpu_online(thread_cpu)) {
		pthread = kthread_create_on_cpu(loop_thread_func, NULL,
						   thread_cpu, "loop_thread");
		if (IS_ERR(pthread)) {
			pr_err("%s: Create thread on %d cpu fail\n",
			       __func__, thread_cpu);
			return PTR_ERR(pthread);
		}
		wake_up_process(pthread);
	}
	return count;
}

static ssize_t thread_trigger_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	char buf[5];
	size_t len;

	len = sprintf(buf, "%u\n", thread_state);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t thread_trigger_write(struct file *file,
			      const char __user *user_buf, size_t count,
			      loff_t *ppos)
{
	int ret;
	unsigned long input;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtoul_from_user(user_buf, count, 10, &input);
	if (ret)
		return -EINVAL;

	if (input == thread_state)
		return count;

	if (input > 2)
		return -EINVAL;

	thread_state = input;
	if (input == 1)
		wake_up_interruptible(&waitq);

	return count;
}

static const struct file_operations ipi_fops = {
	.open	= simple_open,
	.read	= ipi_read,
	.write	= ipi_write,
	.llseek	= default_llseek,
};

static const struct file_operations ipi_trigger_fops = {
	.open	= simple_open,
	.read	= ipi_trigger_read,
	.write	= ipi_trigger_write,
	.llseek	= default_llseek,
};

static const struct file_operations thread_fops = {
	.open	= simple_open,
	.read	= thread_read,
	.write	= thread_write,
	.llseek	= default_llseek,
};

static const struct file_operations thread_trigger_fops = {
	.open	= simple_open,
	.read	= thread_trigger_read,
	.write	= thread_trigger_write,
	.llseek	= default_llseek,
};

static int __init send_ipi_init(void)
{
	int ret = 0;

	if (!proc_create("ipi_cpu", 0660, NULL, &ipi_fops)) {
		ret = -ENOMEM;
		goto err;
	}

	if (!proc_create("ipi_trigger", 0660, NULL, &ipi_trigger_fops)) {
		ret = -ENOMEM;
		goto err_ipi;
	}

	if (!proc_create("thread_cpu", 0660, NULL, &thread_fops)) {
		ret = -ENOMEM;
		goto err_thread;
	}

	if (!proc_create("thread_trigger", 0660, NULL, &thread_trigger_fops)) {
		ret = -ENOMEM;
		goto err_trigger;
	}

	init_waitqueue_head(&waitq);

	return ret;

err_trigger:
	remove_proc_entry("thread_cpu", NULL);
err_thread:
	remove_proc_entry("ipi_trigger", NULL);
err_ipi:
	remove_proc_entry("ipi_cpu", NULL);
err:
	return -ENOMEM;
}

static void __exit send_ipi_exit(void)
{
	remove_proc_entry("ipi_cpu", NULL);
	remove_proc_entry("ipi_trigger", NULL);
	remove_proc_entry("thread_cpu", NULL);
	remove_proc_entry("thread_trigger", NULL);
	if (pthread)
		kthread_stop(pthread);
}

module_init(send_ipi_init);
module_exit(send_ipi_exit);
MODULE_LICENSE("GPL");
