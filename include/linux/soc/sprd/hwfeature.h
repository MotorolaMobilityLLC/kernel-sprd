#ifndef __HWFEATURE__
#define __HWFEATURE__

#define HWFEATURE_KPROPERTY_DEFAULT_VALUE     "-1"
#define HWFEATURE_STR_SIZE_LIMIT              64
#define HWFEATURE_STR_SIZE_LIMIT_KEY          1024

#ifdef CONFIG_SPRD_HWFEATURE
void sprd_kproperty_get(const char *key, char *value, const char *default_value);
#else
static inline void sprd_kproperty_get(const char *key, char *value, const char *default_value)
{
	if (default_value == NULL)
			default_value = HWFEATURE_KPROPERTY_DEFAULT_VALUE;

	strlcpy(value, default_value, HWFEATURE_STR_SIZE_LIMIT);
}
#endif

#endif /*__HWFEATURE__*/
