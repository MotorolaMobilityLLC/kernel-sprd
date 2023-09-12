/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/

#ifndef AMS_TCS343X_H
#define AMS_TCS343X_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#endif

#include <linux/types.h>
#include <linux/mutex.h>

#define TCS3430_DEFAULT_AGAIN 64
#define TCS3430_DEFAULT_PERSIST 0
#define TCS3430_DEFAULT_ATIME 100

enum tcs3430_reg_index {
	TCS3430_REG_ENABLE,     /* = 0x80 */
	TCS3430_REG_ATIME,      /* = 0x81 */
	TCS3430_REG_WTIME,      /* = 0x83 */
	TCS3430_REG_AILTL,      /* = 0x84 */
	TCS3430_REG_AILTH,      /* = 0x85 */
	TCS3430_REG_AIHTL,      /* = 0x86 */
	TCS3430_REG_AIHTH,      /* = 0x87 */
	TCS3430_REG_PERS,       /* = 0x8C */
	TCS3430_REG_CFG1,       /* = 0x90 */
	TCS3430_REG_REVID,      /* = 0x91 */
	TCS3430_REG_ID,         /* = 0x92 */
	TCS3430_REG_STATUS,     /* = 0x93 */
	TCS3430_REG_CH0DATAL,   /* = 0x94  Z channel */
	TCS3430_REG_CH0DATAH,   /* = 0x95  Z channel */
	TCS3430_REG_CH1DATAL,   /* = 0x96  Y channel */
	TCS3430_REG_CH1DATAH,   /* = 0x97  Y channel */
	TCS3430_REG_CH2DATAL,   /* = 0x98  IR1 channel */
	TCS3430_REG_CH2DATAH,   /* = 0x99  IR1 channel */
	TCS3430_REG_CH3DATAL,   /* = 0x9A  X or IR2 channel */
	TCS3430_REG_CH3DATAH,   /* = 0x9B  X or IR2 channel */
	TCS3430_REG_CFG2,       /* = 0x9F */
	TCS3430_REG_CFG3,       /* = 0xAB */
	TCS3430_REG_AZ_CONFIG,  /* = 0xD6 */
	TCS3430_REG_INTENAB,    /* = 0xDD */

	TCS3430_REG_MAX,
};

enum tcs3430_pwr_state {
	POWER_ON,
	POWER_OFF,
	POWER_STANDBY,
};

struct device;

struct tcs3430_parameters {
	u16 als_time_ms;
	u8 als_gain;      /* the gain  (1,4,16,64) */
	u8 persist;       /* APERS bits 3:0 */
};

struct tcs3430_als_info {
	u32 saturation;
	u16 x_raw;
	u16 y_raw;
	u16 z_raw;
	u16 ir1_raw;
	u16 ir2_raw;
	s32 x_raw_q16_16;
	s32 y_raw_q16_16;
	s32 z_raw_q16_16;
	s32 ir1_raw_q16_16;
	s32 ir2_raw_q16_16;
	s32 xp1_q16_16;
	s32 yp1_q16_16;
	s32 zp1_q16_16;
	s32 irp1_q16_16;
	s32 xp1n_q16_16;
	s32 yp1n_q16_16;
	s32 zp1n_q16_16;
	s32 irp1n_q16_16;
	s32 xy_q16_16;
	s32 xz_q16_16;
	s32 xir_q16_16;
	s32 yz_q16_16;
	s32 yir_q16_16;
	s32 zir_q16_16;
	s32 xp2_q16_16;
	s32 yp2_q16_16;
	s32 zp2_q16_16;
	s32 x_q16_16;
	s32 y_q16_16;
	u16 lux;
	u16 cct;
};

struct tcs3430_chip {
	struct mutex lock;
	struct i2c_client *client;
	struct tcs3430_als_info als_inf;
	struct tcs3430_parameters params;
	struct ams_tcs3430_platform_data *pdata;
	u8 shadow[TCS3430_REG_MAX];
	struct input_dev *a_idev;
	int in_suspend;
	int wake_irq;
	int irq_pending;
	bool unpowered;
	bool als_enabled;
	u8 device_index;
	u8 amux_state;
	u8 amux;
	u8 rev;
};

/**
 * struct ams_platform_data - Platform data for the mpu driver
 * @int_config:		interrupt information, not zero if interrupt available
 */
struct ams_tcs3430_platform_data {
	__u8 int_config;
	int (*interrupt_polarity)(void);

/* The following callback for power events are received and handled by
   the driver.  Currently only for SUSPEND and RESUME */
	int (*platform_power)(struct device *dev, enum tcs3430_pwr_state state);
	int (*platform_init)(void);
	void (*platform_teardown)(struct device *dev);
	char const *als_name;
	unsigned int int_gpio;
	struct tcs3430_parameters parameters;
	bool als_can_wake;
#ifdef CONFIG_OF
	struct device_node *of_node;
#endif    
};

#endif /* AMS_TCS343X_H */

