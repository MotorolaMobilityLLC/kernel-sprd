/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_DVFS_DRV_H__
#define __SPRD_DVFS_DRV_H__

#include <linux/platform_device.h>
#include "../governor.h"

struct dvfs_hw_callback {
	int (*hw_dvfs_vote)(const char *name);
	int (*hw_dvfs_unvote)(const char *name);
	int (*hw_dvfs_set_point)(const char *name, unsigned int freq);
	int (*hw_dvfs_get_point_info)(char **name, unsigned int *freq,
				      unsigned int *flag, int index);
	int (*dvfs_freq_request)(unsigned int freq);
};

enum DDR_DFS_STATE_STEP {
	scenario_dfs_enter = 1,
	exit_scene,
	auto_dfs_on_off,
	scaling_force_ddr_freq,
	scene_boost_enter,
	set_backdoor,
	dfs_on_off,
	change_point,
	scene_freq_set,
	get_overflow_t,
	set_overflow_t,
	get_underflow_t,
	set_underflow_t,
	get_dvfs_status_t,
	get_dvfs_auto_status_t,
	get_cur_freq_t,
	get_freq_table_t,
	send_freq_request_t,
};

#define DDR_DB_NODE_NUM 32
#define SCENE_MAX 25
struct DDR_DFS_STEP_T {
	enum DDR_DFS_STATE_STEP step;
	int status;
	u32 buff;
	char scene[SCENE_MAX];
	int pid;
	ktime_t time;
};

struct ddr_dfs_step_list_t {
	struct ddr_dfs_step_list_t *next;
	struct DDR_DFS_STEP_T data;
};

struct governor_callback {
	int (*governor_vote)(const char *name);
	int (*governor_unvote)(const char *name);
	int (*governor_change_point)(const char *name, unsigned int freq);
	int (*get_point_info)(char **name, unsigned int *freq, unsigned int *flag, int index);
	int (*get_freq_num)(unsigned int *data);
	int (*get_overflow)(unsigned int *data, unsigned int sel);
	int (*set_overflow)(unsigned int value, unsigned int sel);
	int (*get_underflow)(unsigned int *data, unsigned int sel);
	int (*set_underflow)(unsigned int value, unsigned int sel);
	int (*get_dvfs_status)(unsigned int *data);
	int (*dvfs_enable)(void);
	int (*dvfs_disable)(void);
	int (*get_dvfs_auto_status)(unsigned int *data);
	int (*dvfs_auto_enable)(void);
	int (*dvfs_auto_disable)(void);
	int (*get_cur_freq)(unsigned int *data);
	int (*get_freq_table)(unsigned long *data, unsigned int sel);
	int (*ddrinfo_dfs_step_show)(char **arg, char **step_status,
				     char **scene, u32 *buff, int *pid, ktime_t *time, u32 i);
	void (*ddr_dfs_step_add)(enum DDR_DFS_STATE_STEP cur_step, int status,
				 char *scene, u32 buff, int pid, ktime_t time);
};

/*functions supportd by dvfs core to specific drivers*/
int dvfs_core_init(struct platform_device *pdev);
int dvfs_core_clear(struct platform_device *pdev);
void dvfs_core_hw_callback_register(struct dvfs_hw_callback *hw_callback);
void dvfs_core_hw_callback_clear(struct dvfs_hw_callback *hw_callback);
unsigned long get_max_freq(void);
int send_freq_request(unsigned int freq);
int get_request_freq(unsigned int *data);
int send_vote_request(unsigned int freq);

/*EXPORT_SYMBOLs supoorted by governor for other kernel drivers*/
int scene_dfs_request(char *scenario);
int scene_exit(char *scenario);
int change_scene_freq(char *scenario, unsigned int freq);

#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
extern struct devfreq_governor sprd_vote;
#endif

static inline int sprd_dvfs_add_governor(void)
{
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
	return devfreq_add_governor(&sprd_vote);
#endif
}

static inline void sprd_dvfs_del_governor(void)
{
#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SPRD_VOTE)
	devfreq_remove_governor(&sprd_vote);
#endif
}
#endif
