/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * Maximum frequency GPU will be clocked at. Given in kHz.
 * This must be specified as there is no default value.
 *
 * Attached value: number in kHz
 * Default value: NA
 */
#define GPU_FREQ_KHZ_MAX 600000
/**
 * Minimum frequency GPU will be clocked at. Given in kHz.
 * This must be specified as there is no default value.
 *
 * Attached value: number in kHz
 * Default value: NA
 */
#define GPU_FREQ_KHZ_MIN 256000

/**
 * CPU_SPEED_FUNC - A pointer to a function that calculates the CPU clock
 *
 * CPU clock speed of the platform is in MHz - see kbase_cpu_clk_speed_func
 * for the function prototype.
 *
 * Attached value: A kbase_cpu_clk_speed_func.
 * Default Value:  NA
 */
#define CPU_SPEED_FUNC (&kbase_cpuprops_get_default_clock_speed)

/**
 * GPU_SPEED_FUNC - A pointer to a function that calculates the GPU clock
 *
 * GPU clock speed of the platform in MHz - see kbase_gpu_clk_speed_func
 * for the function prototype.
 *
 * Attached value: A kbase_gpu_clk_speed_func.
 * Default Value:  NA
 */
#define GPU_SPEED_FUNC (NULL)

/**
 * Power management configuration
 *
 * Attached value: pointer to @ref kbase_pm_callback_conf
 * Default value: See @ref kbase_pm_callback_conf
 */
#define POWER_MANAGEMENT_CALLBACKS (&pm_sharkle_callbacks)

/**
 * Platform specific configuration functions
 *
 * Attached value: pointer to @ref kbase_platform_funcs_conf
 * Default value: See @ref kbase_platform_funcs_conf
 */
#define PLATFORM_FUNCS (&platform_sharkle_funcs)

/** Power model for IPA
 *
 * Attached value: pointer to @ref mali_pa_model_ops
 */
#define POWER_MODEL_CALLBACKS (NULL)

/**
 * Secure mode switch
 *
 * Attached value: pointer to @ref kbase_secure_ops
 */
#define SECURE_CALLBACKS (NULL)

extern struct kbase_pm_callback_conf pm_sharkle_callbacks;
extern struct kbase_platform_funcs_conf platform_sharkle_funcs;
#ifdef CONFIG_MALI_DEVFREQ
int kbase_platform_get_init_freq(void);
int kbase_platform_set_freq(int freq);
#ifdef CONFIG_LIMIT_MAX_FREQ
void kbase_platform_limit_max_freq(struct device *dev);
#endif

#ifdef CONFIG_MALI_BOOST
void kbase_platform_modify_target_freq(struct device *dev, unsigned long *target_freq);
#endif
#endif
bool kbase_mali_is_powered(void);
#ifdef CONFIG_MALI_BOOST
void kbase_platform_set_boost(struct kbase_device *kbdev, int boost_level);
#endif
