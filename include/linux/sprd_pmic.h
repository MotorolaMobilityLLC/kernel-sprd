#ifndef __SPRD_PMIC_H__
#define __SPRD_PMIC_H__

#ifdef CONFIG_SPRD_PMIC_REFOUT
int pmic_refout_update(unsigned int refout_num, int refout_state);
#else
static inline int pmic_refout_update(unsigned int refout_num, int refout_state)
{
	return 0;
}
#endif

#endif /* __SPRD_PMIC_H__ */
