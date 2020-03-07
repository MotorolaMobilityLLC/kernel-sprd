/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * KUnit test for thermal.
 */
#ifndef __SPRD_CPU_DEVICE_TEST_H__
#define __SPRD_CPU_DEVICE_TEST_H__

#include <test/test.h>
#include <test/mock.h>
#include <linux/cpumask.h>

#define CPU_MASK_CPU4							\
(cpumask_t) { {								\
	[0] =  0xfUL							\
} }

u64 get_core_dyn_power(int cluster_id,
		unsigned int freq_mhz, unsigned int voltage_mv);

u64 get_cluster_dyn_power(int cluster_id,
		unsigned int freq_mhz, unsigned int voltage_mv);

u32 get_cluster_min_cpufreq(int cluster_id);

u32 get_cluster_min_cpunum(int cluster_id);

u32 get_cluster_resistance_ja(int cluster_id);

__visible_for_testing int get_static_power(cpumask_t *cpumask,
	int interval, unsigned long u_volt, u32 *power, int temperature);

__visible_for_testing int get_core_static_power(cpumask_t *cpumask,
	int interval, unsigned long u_volt, u32 *power, int temperature);

__visible_for_testing int get_all_core_temp(int cluster_id, int cpu);

__visible_for_testing u32 get_core_cpuidle_tp(int cluster_id,
	int first_cpu, int cpu, int *temp);

__visible_for_testing u32 get_cpuidle_temp_point(int cluster_id);

__visible_for_testing void get_core_temp(int cluster_id,
	int cpu, int *temp);

int create_cpu_cooling_device(void);

int destroy_cpu_cooling_device(void);

#endif /* __SPRD_CPU_DEVICE_TEST_H__ */
