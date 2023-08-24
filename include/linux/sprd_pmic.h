#ifndef __SPRD_PMIC_H__
#define __SPRD_PMIC_H__

#ifndef CONFIG_SPRD_PMIC_REFOUT
int pmic_refout_update(unsigned int refout_num, int refout_state);
#else
static inline int pmic_refout_update(unsigned int refout_num, int refout_state)
{	
	printk("lion pmic_refout_update error\n");
	return 0;
}
#endif

#endif /* __SPRD_PMIC_H__ */
