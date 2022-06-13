# SPDX-License-Identifier: GPL-2.0-only

KCOV_INSTRUMENT := n
KCSAN_SANITIZE := n

obj-$(CONFIG_UNISOC_SCHED) += unisoc-sched.o
unisoc-sched-$(CONFIG_SCHED_WALT) := walt.o sysctl.o walt_freq.o fair.o \
				rt.o boost.o
obj-$(CONFIG_UNISOC_SCHEDUTIL_GOV) += unisoc_schedutil.o
