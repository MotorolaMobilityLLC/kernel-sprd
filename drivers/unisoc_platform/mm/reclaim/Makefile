# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_UNISOC_MM_RECLAIM) += unisoc_mm_reclaim.o
unisoc_mm_reclaim-objs := reclaim.o

ifeq ($(CONFIG_UNISOC_MM_SHRINKLRU),y)
unisoc_mm_reclaim-objs += kshrink_lruvecd.o
endif