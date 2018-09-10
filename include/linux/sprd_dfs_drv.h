#ifndef __SPRD_DFS_DRV_H__
#define __SPRD_DFS_DRV_H__

struct scene_freq {
	char *scene_name;
	unsigned int scene_freq;
	int scene_count;
};

extern int dfs_enable(void);
extern int dfs_disable(void);
extern int dfs_auto_enable(void);
extern int dfs_auto_disable(void);
extern int scene_dfs_request(char *scenario);
extern int scene_exit(char *scenario);
extern int force_freq_request(unsigned int freq);
extern int get_dfs_status(unsigned int *data);
extern int get_dfs_auto_status(unsigned int *data);
extern int get_freq_num(unsigned int *data);
extern int get_freq_table(unsigned int *data, unsigned int sel);
extern int get_cur_freq(unsigned int *data);
extern int get_ap_freq(unsigned int *data);
extern int get_cp_freq(unsigned int *data);
extern int get_force_freq(unsigned int *data);
extern int get_overflow(unsigned int *data, unsigned int sel);
extern int get_underflow(unsigned int *data, unsigned int sel);
extern int get_timer(unsigned int *data);
extern int get_scene_num(unsigned int *data);
extern int set_overflow(unsigned int value, unsigned int sel);
extern int set_underflow(unsigned int value, unsigned int sel);
extern int get_scene_info(char **name, unsigned int *freq,
			unsigned int *count, int index);
#endif
