#ifndef __SPRD_PMIC_H__
#define __SPRD_PMIC_H__

int pmic_refout_update(unsigned int refout_num, int refout_state);
#if 0
#ifdef CONFIG_SPRD_PMIC_REFOUT
int pmic_refout_update(unsigned int refout_num, int refout_state);
#else
static inline int pmic_refout_update(unsigned int refout_num, int refout_state)
{
	pr_err("%s- not define SPRD_PMIC_REFOUT\n", __func__);
	return 0;
}
#endif
#endif
#endif /* __SPRD_PMIC_H__ */
