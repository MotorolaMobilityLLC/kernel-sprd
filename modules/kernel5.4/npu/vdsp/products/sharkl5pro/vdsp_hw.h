/*
* SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef _VDSP_HW_H
#define _VDSP_HW_H

#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include "vdsp_ipi_drv.h"

#define VDSP_FIRMWIRE_SIZE			(1024*1024*6)
#define VDSP_DRAM_ADDR				(0x7f000000)
#define VDSP_DRAM_SIZE				(256*1024)

#define DRIVER_NAME             "vdsp"
#define REG_RESET               (0x04)
#define REG_RUNSTALL            (0x3084)	//not support set/clr
#define REG_LP_CTL              (0x3090)	//not support set/clr
#define REG_VDSP_INT_CTL        (0x3094)	//not support set/clr
#define REG_QOS_THRESHOLD       (0xBC)
#define REG_QOS_3               (0xD4)
#define REG_QOS_SEL3            (0xD8)

#define REG_PD_AP_VDSP_CFG                (0x7e4)
#define REG_PD_AP_VDSP_CORE_INT_DISABLE   (0x7f8)
#define REG_PD_AP_VDSP_DLSP_ENA           (0x804)
#define REG_ISPPLL_REL_CFG                (0x8c)

#define REG_AP_DFS_EN_CTL        (0x8)


#define APAHB_HREG_MWR(reg, msk, val) \
		(REG_WR((reg), \
		((val) & (msk)) | \
		(REG_RD((reg)) & \
		(~(msk)))))

#define APAHB_HREG_OWR(reg, val) \
		(REG_WR((reg), \
		(REG_RD(reg) | (val))))

enum {
	XRP_DSP_SYNC_IRQ_MODE_NONE = 0x0,
	XRP_DSP_SYNC_IRQ_MODE_LEVEL = 0x1,
	XRP_DSP_SYNC_IRQ_MODE_EDGE = 0x2,
};

enum vdsp_init_flags {
	/*! Use interrupts in DSP->host communication */
	XRP_INIT_USE_HOST_IRQ = 0x1,
};

enum reg_type{
	RT_PMU = 0,
	RT_APAHB,
	RT_NO_SET_CLR,
};

struct xvp;

struct qos_info {
	uint8_t ar_qos_vdsp_msti;
	uint8_t ar_qos_vdsp_mstd;
	uint8_t aw_qos_vdsp_mstd;
	uint8_t ar_qos_vdsp_idma;
	uint8_t aw_qos_vdsp_idma;
	uint8_t ar_qos_vdma;
	uint8_t aw_qos_vdma;
	uint8_t ar_qos_threshold;
	uint8_t aw_qos_threshold;
};

struct vdsp_hw {
	struct xvp *xrp;
	struct regmap *ahb_regmap;
	struct regmap *pmu_regmap;
	phys_addr_t ipi_phys;
	void __iomem *ipi;
	phys_addr_t clk_phys;
	void __iomem *clk;

	/* how IRQ is used to notify the device of incoming data */
	enum xrp_irq_mode device_irq_mode;
	/*
	 * offset of device IRQ register in MMIO region (device side)
	 * bit number
	 * device IRQ#
	 */
	u32 device_irq[3];
	/* offset of devuce IRQ register in MMIO region (host side) */
	u32 device_irq_host_offset;
	/* how IRQ is used to notify the host of incoming data */
	enum xrp_irq_mode host_irq_mode;
	/*
	 * offset of IRQ register (device side)
	 * bit number
	 */
	u32 host_irq[2];

	s32 client_irq;

	struct vdsp_ipi_ctx_desc *vdsp_ipi_desc;
	struct qos_info qos;
};

struct vdsp_side_sync_data {
	__u32 device_mmio_base;
	__u32 host_irq_mode;
	__u32 host_irq_offset;
	__u32 host_irq_bit;
	__u32 device_irq_mode;
	__u32 device_irq_offset;
	__u32 device_irq_bit;
	__u32 device_irq;

	__u32 vdsp_smsg_addr;
	__u32 vdsp_log_addr;
};

/*!
 * Hardware-specific operation entry points.
 * Hardware-specific driver passes a pointer to this structure to xrp_init
 * at initialization time.
 */
struct xrp_hw_ops {
	/*!
	 * Enable power/clock, but keep the core stalled.
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	int (*enable) (void *hw_arg);
	/*!
	 * Diable power/clock.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	void (*disable) (void *hw_arg);
	/*!
	 * Reset the core.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	void (*reset) (void *hw_arg);
	/*!
	 * Unstall the core.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	void (*release) (void *hw_arg);
	/*!
	 * Stall the core.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	void (*halt) (void *hw_arg);

	/*! Get HW-specific data to pass to the DSP on synchronization
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 * \param sz: return size of sync data here
	 * \return a buffer allocated with kmalloc that the caller will free
	 */
	void *(*get_hw_sync_data) (void *hw_arg, size_t *sz, uint32_t log_addr);

	/*!
	 * Send IRQ to the core.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	void (*send_irq) (void *hw_arg);

	/*!
	 * Check whether region of physical memory may be handled by
	 * dma_sync_* operations
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 */
	bool(*cacheable) (void *hw_arg, unsigned long pfn, unsigned long n_pages);
	/*!
	 * Synchronize region of memory for DSP access.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 * \param flags: XRP_FLAG_{READ,WRITE,READWRITE}
	 */
	void (*dma_sync_for_device) (void *hw_arg, void *vaddr, phys_addr_t paddr,
		unsigned long sz, unsigned flags);
	/*!
	 * Synchronize region of memory for host access.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 * \param flags: XRP_FLAG_{READ,WRITE,READWRITE}
	 */
	void (*dma_sync_for_cpu) (void *hw_arg, void *vaddr, phys_addr_t paddr,
		unsigned long sz, unsigned flags);

	/*!
	 * memcpy data/code to device-specific memory.
	 */
	void (*memcpy_tohw) (void __iomem *dst, const void *src, size_t sz);
	/*!
	 * memset device-specific memory.
	 */
	void (*memset_hw) (void __iomem *dst, int c, size_t sz);
	/*!
	 * Check DSP status.
	 *
	 * \param hw_arg: opaque parameter passed to xrp_init at initialization
	 *                time
	 * \return whether the core has crashed and needs to be restarted
	 */
	bool(*panic_check) (void *hw_arg);

	/*set qos */
	int (*set_qos) (void *hw_arg);

	/*request irq */
	int (*vdsp_request_irq) (void *xvp_arg, void *hw_arg);

	/*free irq */
	void (*vdsp_free_irq) (void *xvp_arg, void *hw_arg);
};

long sprd_vdsp_init(struct platform_device *pdev, enum vdsp_init_flags flags,
	const struct xrp_hw_ops *hw, void *hw_arg);
int sprd_vdsp_deinit(struct platform_device *pdev);

/*!
 * Notify generic XRP driver of possible IRQ from the DSP.
 *
 * \param irq: IRQ number
 * \param xvp: pointer to struct xvp returned from xrp_init* call
 * \return whether IRQ was recognized and handled
 */
irqreturn_t xrp_irq_handler(int irq, struct xvp *xvp);
irqreturn_t vdsp_log_irq_handler(int irq, void *private);

int vdsp_runtime_resume(struct device *dev);
int vdsp_runtime_suspend(struct device *dev);

int vdsp_regmap_update_bits(struct regmap *regmap, uint32_t offset,
	uint32_t mask, uint32_t val, enum reg_type rt);

#endif
