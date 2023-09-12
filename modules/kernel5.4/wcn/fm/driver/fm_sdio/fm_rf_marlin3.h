#ifndef __MARLIN3_RF_H__
#define __MARLIN3_RF_H__
struct nvm_cali_cmd {
	int8_t itm[64];
	int32_t par[256];
	int32_t num;
};

struct nvm_name_table {
	int8_t *itm;
	uint32_t mem_offset;
	int32_t type;
};

/*fm config struct*/
struct fm_config_t{
	uint16_t	fm_modem_work_freq;
	uint16_t    	stra_sel;
	uint16_t	seek_ch_th;
	uint16_t	mono_pwr_th1;
	uint16_t	mono_pwr_th2;
	uint16_t	mono_pwr_th3;
	uint16_t	seek_chan_mode;
	uint16_t	seek_vldch_fo_th1;
	uint16_t	seek_vldch_fo_th2;
	uint16_t	noise_pwr_th0;
	uint16_t	noise_pwr_th1;
	uint16_t	noise_pwr_th2;
	uint16_t	pdp_th;
	uint16_t	pdp_dev1;
	uint16_t	pdp_dev2;
	uint16_t	seek_sample_num_div;
	uint16_t	boundary_offset;
	uint16_t	fm_db_comp;
	uint16_t	fm_cali_reserved[46];
};

int get_fm_config_param(struct fm_config_t *p);
#endif
