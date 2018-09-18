#include <linux/init.h>
#include <linux/printk.h>
#include <test/test.h>

extern struct test_module *__test_modules_start[];
extern struct test_module *__test_modules_end[];

static bool test_run_all_tests(void)
{
	struct test_module** module;
	bool has_test_failed = false;

	for (module = __test_modules_start; module < __test_modules_end; ++module) {
		if (test_run_tests(*module))
			has_test_failed = true;
	}

	return !has_test_failed;
}

static int test_executor_init(void)
{
	if (test_run_all_tests())
		return 0;
	else
		return -EFAULT;
}

late_initcall(test_executor_init);
