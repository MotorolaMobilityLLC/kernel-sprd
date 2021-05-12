#ifndef _QOGIRN6PRO_ISE_PD_H_
#define _QOGIRN6PRO_ISE_PD_H_

long qogirn6pro_ise_pd_status_check(void *apdu_dev);
long qogirn6pro_ise_cold_power_on(void *apdu_dev);
long qogirn6pro_ise_full_power_down(void *apdu_dev);
long qogirn6pro_ise_hard_reset(void *apdu_dev);
long qogirn6pro_ise_soft_reset(void *apdu_dev);
long qogirn6pro_ise_hard_reset_set(void *apdu_dev);
long qogirn6pro_ise_hard_reset_clr(void *apdu_dev);

#endif

