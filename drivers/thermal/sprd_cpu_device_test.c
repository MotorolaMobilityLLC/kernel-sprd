/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 * KUnit test for thermal.
 */
#include <test/test.h>
#include <test/mock.h>
#include <linux/thermal.h>
#include <linux/sprd_cpu_cooling.h>
#include <linux/sprd_cpu_device.h>
#include "sprd_cpu_device_test.h"

struct sprd_cpu_device_test {
	struct test *test;
	struct cpu_power_model_t *power_model;
};

static void get_core_dyn_power_test(struct test *test)
{
	u32 power = 0;

	//cluster_id=0
	power = get_core_dyn_power(0, 768, 918);
	EXPECT_EQ(test, 65, power);

	power = get_core_dyn_power(0, 884, 975);
	EXPECT_EQ(test, 84, power);

	power = get_core_dyn_power(0, 1000, 1028);
	EXPECT_EQ(test, 106, power);

	power = get_core_dyn_power(0, 1100, 1075);
	EXPECT_EQ(test, 128, power);

	power = get_core_dyn_power(0, 1200, 1121);
	EXPECT_EQ(test, 151, power);

	//cluster_id=1
	power = get_core_dyn_power(1, 768, 900);
	EXPECT_EQ(test, 95, power);

	power = get_core_dyn_power(1, 1050, 921);
	EXPECT_EQ(test, 136, power);

	power = get_core_dyn_power(1, 1225, 984);
	EXPECT_EQ(test, 182, power);

	power = get_core_dyn_power(1, 1400, 1050);
	EXPECT_EQ(test, 237, power);

	power = get_core_dyn_power(1, 1500, 1084);
	EXPECT_EQ(test, 270, power);

	power = get_core_dyn_power(1, 1600, 1121);
	EXPECT_EQ(test, 308, power);
}

static void get_cluster_dyn_power_test(struct test *test)
{
	u32 power = 0;

	power = get_cluster_dyn_power(0, 1200, 1121);
	EXPECT_EQ(test, 112, power);

}

static void get_static_power_test(struct test *test)
{
	cpumask_t cpumask_4 = CPU_MASK_CPU4;
	u32 power = 0;
	u32 power_4 = 0;

	power = get_static_power(&cpumask_4, 100,
						1121875, &power_4, 70866);
	EXPECT_EQ(test, 107, power_4);

	power = get_static_power(&cpumask_4, 100,
						1120000, &power_4, 70866);
	EXPECT_EQ(test, 107, power_4);

	power = get_static_power(&cpumask_4, 100,
						1000000, &power_4, 70866);
	EXPECT_EQ(test, 80, power_4);

	power = get_static_power(&cpumask_4, 100,
						2121875, &power_4, 70866);
	EXPECT_EQ(test, 949, power_4);

	power = get_static_power(&cpumask_4, 100,
						6121875, &power_4, 70866);
	EXPECT_EQ(test, 33110, power_4);
}

static void get_core_static_power_test(struct test *test)
{
	cpumask_t cpumask_4 = CPU_MASK_CPU4;
	u32 power_4 = 0;
	u32 power = 0;

	power = get_core_static_power(&cpumask_4, 100,
						1121875, &power_4, 70866);
	EXPECT_EQ(test, 76, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						1120000, &power_4, 70866);
	EXPECT_EQ(test, 76, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						1000000, &power_4, 70866);
	EXPECT_EQ(test, 57, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						2121875, &power_4, 70866);
	EXPECT_EQ(test, 672, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						6121875, &power_4, 70866);
	EXPECT_EQ(test, 23424, power_4);
}

static void  get_cluster_min_cpufreq_test(struct test *test)
{
	u32 min_cpufreq  = 0;
	u32 min_cpufreq1 = 0;

	min_cpufreq  = get_cluster_min_cpufreq(0);
	min_cpufreq1 = get_cluster_min_cpufreq(1);

	EXPECT_EQ(test, 768000, min_cpufreq);
	EXPECT_EQ(test, 768000, min_cpufreq1);
}

static void  get_cluster_min_cpunum_test(struct test *test)
{
	u32 min_cpunum = 0;

	min_cpunum = get_cluster_min_cpunum(0);
	EXPECT_EQ(test, 4, min_cpunum);

	min_cpunum = get_cluster_min_cpunum(1);
	EXPECT_EQ(test, 0, min_cpunum);
}
static void get_cluster_resistance_ja_test(struct test *test)
{
	int test_ja = 0;

	test_ja = get_cluster_resistance_ja(0);
	EXPECT_EQ(test, 0, test_ja);
}

static void  get_core_temp_test(struct test *test)
{
	u32 core_temp = 0;

	get_core_temp(0, 0, &core_temp);
	EXPECT_EQ(test, 0, core_temp);

	get_core_temp(1, 4, &core_temp);
	EXPECT_EQ(test, 0, core_temp);

}
static void  get_all_core_temp_test(struct test *test)
{
	u32 ret_test = 0;

	ret_test = get_all_core_temp(0, 4);
	EXPECT_EQ(test, 0, ret_test);
}
static void  get_core_cpuidle_tp_test(struct test *test)
{
	u32 temp = 0;
	u32 tp_id = 0;

	tp_id = get_core_cpuidle_tp(0, 0, 0, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 1, 0, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 2, 5, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 0, 3, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 1, 5, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 2, 6, &temp);
	EXPECT_EQ(test, 0, temp);

}

static void  get_cpuidle_temp_point_test(struct test *test)
{
	u32 temp_point = 0;

	temp_point = get_cpuidle_temp_point(0);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(1);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(2);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(3);
	EXPECT_EQ(test, 0, temp_point);
}

static int sprd_cpu_device_test_init(struct test *test)
{
	struct sprd_cpu_device_test *ctx;

	ctx = test_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	test->priv = ctx;

	ctx->test = test;

	create_cpu_cooling_device();

	return 0;
}

static void sprd_cpu_device_test_exit(struct test *test)
{
	destroy_cpu_cooling_device();

	test_cleanup(test);
}

static struct test_case sprd_cpu_device_test_cases[] = {
	TEST_CASE(get_core_dyn_power_test),

	TEST_CASE(get_cluster_dyn_power_test),

	TEST_CASE(get_static_power_test),

	TEST_CASE(get_core_static_power_test),

	TEST_CASE(get_cluster_min_cpufreq_test),

	TEST_CASE(get_cluster_min_cpunum_test),

	TEST_CASE(get_cluster_resistance_ja_test),

	TEST_CASE(get_core_temp_test),

	TEST_CASE(get_all_core_temp_test),

	TEST_CASE(get_core_cpuidle_tp_test),

	TEST_CASE(get_cpuidle_temp_point_test),

	{},
};

static struct test_module sprd_cpu_device_test_module = {
	.name = "sprd-cpu-device-test",
	.init = sprd_cpu_device_test_init,
	.exit = sprd_cpu_device_test_exit,
	.test_cases = sprd_cpu_device_test_cases,
};

module_test(sprd_cpu_device_test_module);
