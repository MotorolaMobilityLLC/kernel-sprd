/**
 * Copyright (C) 2019-2022 UNISOC (Shanghai) Technologies Co.,Ltd.
 */

/*
 * XRP driver IOCTL codes and data structures
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */
/*
 * This file has been modified by UNISOC to add faceid, memory, dvfs related define
 * to realize real device driver.
 */
#ifndef _XRP_KERNEL_DEFS_H
#define _XRP_KERNEL_DEFS_H

#define XRP_IOCTL_MAGIC 'r'
#define XRP_IOCTL_ALLOC		_IO(XRP_IOCTL_MAGIC, 1)
#define XRP_IOCTL_FREE		_IO(XRP_IOCTL_MAGIC, 2)
#define XRP_IOCTL_QUEUE		_IO(XRP_IOCTL_MAGIC, 3)
#define XRP_IOCTL_QUEUE_NS	_IO(XRP_IOCTL_MAGIC, 4)
#define XRP_IOCTL_SET_DVFS      _IO(XRP_IOCTL_MAGIC,5)
#define XRP_IOCTL_FACEID_CMD    _IO(XRP_IOCTL_MAGIC,6)
#define XRP_IOCTL_SET_POWERHINT _IO(XRP_IOCTL_MAGIC,7)
#define XRP_IOCTL_MEM_QUERY  _IO(XRP_IOCTL_MAGIC,8)
#define XRP_IOCTL_MEM_IMPORT _IO(XRP_IOCTL_MAGIC,9)
#define XRP_IOCTL_MEM_EXPORT _IO(XRP_IOCTL_MAGIC,10)
#define XRP_IOCTL_MEM_ALLOC  _IO(XRP_IOCTL_MAGIC,11)
#define XRP_IOCTL_MEM_FREE   _IO(XRP_IOCTL_MAGIC,12)
#define XRP_IOCTL_MEM_IOMMU_MAP    _IO(XRP_IOCTL_MAGIC,13)
#define XRP_IOCTL_MEM_IOMMU_UNMAP  _IO(XRP_IOCTL_MAGIC,14)
#define XRP_IOCTL_MEM_CPU_TO_DEVICE  _IO(XRP_IOCTL_MAGIC,15)
#define XRP_IOCTL_MEM_DEVICE_TO_CPU  _IO(XRP_IOCTL_MAGIC,16)

#define XRP_NAMESPACE_ID_SIZE   32

struct xrp_ioctl_alloc {
	__u32 size;
	__u32 align;
	__u64 addr;
};

enum {
	XRP_FLAG_READ = 0x1,
	XRP_FLAG_WRITE = 0x2,
	XRP_FLAG_READ_WRITE = 0x3,
};

struct xrp_ioctl_buffer {
	__u32 flags;
	__u32 size;
	__u64 addr;
	int fd;			/*add ion fd */
};

enum {
	XRP_QUEUE_FLAG_NSID = 0x4,
	XRP_QUEUE_FLAG_PRIO = 0xff00,
	XRP_QUEUE_FLAG_PRIO_SHIFT = 8,

	XRP_QUEUE_VALID_FLAGS = XRP_QUEUE_FLAG_NSID | XRP_QUEUE_FLAG_PRIO,
};

struct xrp_ioctl_queue {
	__u32 flags;
	__u32 in_data_size;
	__u32 out_data_size;
	__u32 buffer_size;
	__u64 in_data_addr;
	int in_data_fd;
	__u64 out_data_addr;
	int out_data_fd;
	__u64 buffer_addr;
	__u64 nsid_addr;
};

struct xrp_dvfs_ctrl {
	__u32 en_ctl_flag;
	union {
		__u32 enable;
		__u32 level;
	};
};

struct xrp_powerhint_ctrl {
	int level;
	__u32 flag;
};

struct xrp_faceid_ctrl {
	__u32 inout_fd;
	__u32 img_fd;
};

/* response returned when querying for heaps */
struct xrp_heap_data {
	__u32 id;		/* Heap ID   */
	__u32 type;		/* Heap type */
	__u32 attributes;	/* Heap attributes
				   defining capabilities that user may treat as hint
				   when selecting the heap id during allocation/importing */
};

#define SPRD_MAX_HEAPS 16

struct xrp_heaps_ctrl {
	struct xrp_heap_data heaps[SPRD_MAX_HEAPS];	/* [OUT] Heap data */
} __attribute__ ((aligned(8)));

/* parameters to allocate a device buffer */
struct xrp_alloc_ctrl {
	__u64 size;		/* [IN] Size of device memory (in bytes) */
	__u32 heap_id;	/* [IN] Heap ID of allocator */
	__u32 attributes;	/* [IN] Attributes of buffer */
	char name[8];		/* [IN] short name for buffer */
	__u32 buf_id;		/* [OUT] Generated buffer ID */
} __attribute__ ((aligned(8)));

/* parameters to import a device buffer */
struct xrp_import_ctrl {
	__u64 size;		/* [IN] Size of device memory (in bytes)    */
	//__u64 buf_hnd;	/* [IN] File descriptor/cpu pointer of buffer to import */
	__u64 buf_fd;		/* [IN] File descriptor */
	__u64 cpu_ptr;	/* [IN] Cpu pointer of buffer to import */
	__u32 heap_id;	/* [IN] Heap ID of allocator  */
	__u32 attributes;	/* [IN] Attributes of buffer */
	char name[8];		/* [IN] short name for buffer */
	__u32 buf_id;		/* [OUT] Generated buffer ID */
} __attribute__ ((aligned(8)));

/* parameters to export a device buffer */
struct xrp_export_ctrl {
	__u32 buf_id;		/* [IN] Buffer ID to be exported */
	__u64 size;		/* [IN] Size to be exported */
	__u32 attributes;	/* [IN] Attributes of buffer */
	__u64 buf_hnd;	/* [OUT] Buffer handle (file descriptor) */
} __attribute__ ((aligned(8)));

struct xrp_free_ctrl {
	__u32 buf_id;		/* [IN] ID of device buffer to free */
};

/* parameters to map a buffer into device */
struct xrp_map_ctrl {
	__u32 buf_id;		/* [IN] ID of device buffer to map */
	__u32 flags;		/* [IN] Mapping flags */
} __attribute__ ((aligned(8)));

struct xrp_unmap_ctrl {
	__u32 buf_id;		/* [IN] ID of device buffer to unmap */
} __attribute__ ((aligned(8)));

struct xrp_sync_cpu_to_device_ctrl {
	__u32 buf_id;		/* [IN] ID of device buffer to sync */
} __attribute__ ((aligned(8)));

struct xrp_sync_device_to_cpu_ctrl {
	__u32 buf_id;		/* [IN] ID of device buffer to sync */
} __attribute__ ((aligned(8)));

#if 0
struct xrp_faceid_ctrl {
	__u32 in_fd;
	__u32 out_fd;
};
#endif
#endif
