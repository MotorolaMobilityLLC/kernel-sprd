obj-$(CONFIG_EXFAT_FS) += exfat_core.o exfat_fs.o

exfat_fs-y := exfat_super.o
exfat_core-y := exfat.o exfat_api.o exfat_blkdev.o exfat_cache.o exfat_xattr.o \
		exfat_data.o exfat_global.o exfat_nls.o exfat_oal.o exfat_upcase.o
GCOV_PROFILE_exfat_api.o := $(CONFIG_SPRD_FS_GCOV)
