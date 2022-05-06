#!/bin/bash

update_flag=0

KERNEL_PATH=$($(dirname $(readlink -f $0))/get_kernel_path.sh)

ROOT_DIR=$(dirname ${KERNEL_PATH})
KERNEL_DIR=$(echo ${KERNEL_PATH}|awk -F'/' '{print $NF}')
source ${KERNEL_PATH}/build.config.common

export PATH="${ROOT_DIR}/${CLANG_PREBUILT_BIN}:${ROOT_DIR}/${BUILDTOOLS_PREBUILT_BIN}:${PATH}"
if [ -z "${SOURCE_DATE_EPOCH}" ]; then
	export SOURCE_DATE_EPOCH=$(git -C ${ROOT_DIR}/${KERNEL_DIR} log -1 --pretty=%ct)
fi
export KBUILD_BUILD_TIMESTAMP="$(date -d @${SOURCE_DATE_EPOCH})"
export KBUILD_BUILD_HOST=build-host
export KBUILD_BUILD_USER=build-user
export KBUILD_BUILD_VERSION=1
export ARCH=arm64
JOBS=$(cat /proc/cpuinfo |grep processor|wc -l)

# use relative paths for file name references in the binaries
# (e.g. debug info)
export KCPPFLAGS="-ffile-prefix-map=${ROOT_DIR}/${KERNEL_DIR}/= -ffile-prefix-map=${ROOT_DIR}/="

# set the common sysroot
sysroot_flags+="--sysroot=${ROOT_DIR}/build/kernel/build-tools/sysroot "

# add openssl (via boringssl) and other prebuilts into the lookup path
cflags+="-I${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/include "

# add openssl and further prebuilt libraries into the lookup path
ldflags+="-Wl,-rpath,${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/lib64 "
ldflags+="-L ${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/lib64 "

# Have host compiler use LLD and compiler-rt.
LLD_COMPILER_RT="-fuse-ld=lld --rtlib=compiler-rt"
ldflags+=${LLD_COMPILER_RT}

export HOSTCFLAGS="$sysroot_flags $cflags"
export HOSTLDFLAGS="$sysroot_flags $ldflags"

HOSTCC=clang
HOSTCXX=clang++
CC=clang
LD=ld.lld
AR=llvm-ar
NM=llvm-nm
OBJCOPY=llvm-objcopy
OBJDUMP=llvm-objdump
OBJSIZE=llvm-size
READELF=llvm-readelf
STRIP=llvm-strip
export HOSTCC HOSTCXX CC LD AR NM OBJCOPY OBJDUMP OBJSIZE READELF STRIP AS
NDK_TRIPLE=$(grep NDK_TRIPLE ${KERNEL_PATH}/build.config.aarch64 |awk -F'=' '{print $NF}')
NDK_DIR=${ROOT_DIR}/prebuilts/ndk-r23
USERCFLAGS="--target=${NDK_TRIPLE} "
USERCFLAGS+="--sysroot=${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot "
USERCFLAGS+="-Wno-unused-function "
USERLDFLAGS="${LLD_COMPILER_RT} "
USERLDFLAGS+="--target=${NDK_TRIPLE} "
export USERCFLAGS USERLDFLAGS

function get_board() {
	cd ${KERNEL_PATH}/arch/arm64/configs/
	board_fragment_list=$(find ./* -name "sprd_gki_*.fragment"|grep -v debug)

	for i in ${board_fragment_list}
	do
		board_list="${board_list} $(echo $i|awk -F'_' '{print $NF}'|awk -F'.' '{print $1}')"
	done
	cd ${ROOT_DIR}
	echo ${board_list}
}

function sort_config() {
  # Normal sort won't work because all the "# CONFIG_.. is not set" would come
  # before all the "CONFIG_..=m". Use sed to extract the CONFIG_ option and prefix
  # the line in front of the line to create a key (e.g. CONFIG_.. # CONFIG_.. is not set),
  # sort, then remove the key
  sed -E -e 's/.*(CONFIG_[^ =]+).*/\1 \0/' $1 | sort -k1 | cut -F2-
}

# verifies that defconfig matches the DEFCONFIG
function check_defconfig() {
	cd ${ROOT_DIR}
	local out=$(mktemp -d /tmp/tmpd.XXXXXX)
	make -j ${JOBS} -C ${KERNEL_DIR} LLVM=1 DEPMOD=depmod DTC=dtc ARCH=arm64 O=${out} $1 >/dev/null
	make -j ${JOBS} -C ${KERNEL_DIR} LLVM=1 DEPMOD=depmod DTC=dtc ARCH=arm64 O=${out} savedefconfig >/dev/null
	RES=0
	diff -u ${KERNEL_PATH}/arch/${ARCH}/configs/$1 ${out}/defconfig >&2 || RES=$?
	if [ ${RES} -ne 0 ]; then
		echo ERROR: savedefconfig does not match ${KERNEL_DIR}/arch/${ARCH}/configs/${DEFCONFIG} >&2
	fi
	rm -rf ${out}
	return ${RES}
}

function generate_config()
{
	baseconfig=$1
	fragment=$2
	defconfig=$3
	local out=$(mktemp -d /tmp/tmpd.XXXXXX)
	local orig_config=$(mktemp)
	local new_config=$(mktemp)
	local changed_config=$(mktemp)
	local new_fragment=$(mktemp)
	local new_fragment_sorted=$(mktemp)
	check_defconfig gki_defconfig
	if [[ $? -ne 0 ]];then
		exit 1
	fi
	cd ${ROOT_DIR}
	make -j ${JOBS} -C ${KERNEL_DIR} LLVM=1 DEPMOD=depmod DTC=dtc ARCH=arm64 O=${out} ${baseconfig} >/dev/null
	cp ${out}/.config ${orig_config}
	KCONFIG_CONFIG=${KERNEL_PATH}/arch/arm64/configs/${defconfig} ${KERNEL_PATH}/scripts/kconfig/merge_config.sh -m -r ${KERNEL_PATH}/arch/arm64/configs/${baseconfig} ${KERNEL_PATH}/arch/arm64/configs/${fragment} >/dev/null
	make -j ${JOBS} -C ${KERNEL_DIR} LLVM=1 DEPMOD=depmod DTC=dtc ARCH=arm64 O=${out} ${defconfig} >/dev/null
	cp ${out}/.config ${new_config}

	${KERNEL_PATH}/scripts/diffconfig -m ${orig_config} ${new_config} > ${new_fragment}
	#KCONFIG_CONFIG=${new_fragment} ${KERNEL_PATH}/scripts/kconfig/merge_config.sh -m \
	#	${KERNEL_PATH}/arch/${ARCH}/configs/${fragment} ${changed_config} >/dev/null

	sort_config ${new_fragment} > ${new_fragment_sorted}
	diff -u ${KERNEL_PATH}/arch/${ARCH}/configs/${fragment} ${new_fragment_sorted} >/dev/null || RES=$?
	if [ ${RES} -ne 0 ]; then
		echo "ERROR: the fragment ${fragment} order not correct" >&2
		if [ $update_flag -eq 1 ];then
			echo "update the ${fragment}"
			cp  ${new_fragment_sorted} ${KERNEL_PATH}/arch/${ARCH}/configs/${fragment}
		fi
	else
		echo "the fragment ${fragment} has a correct order" >&1
	fi

	rm -rf ${out}
	rm -rf $orig_config $new_config $changed_config $new_fragment $new_fragment_sorted

	return $RES
}
if [[ "$1" == "update" ]];then
	update_flag=1
fi
# sort the sprd_gki_SOC[_debug].fragment
for board in $(get_board)
do
	echo "check sprd_gki_${board}.fragment"
	generate_config gki_defconfig sprd_gki_${board}.fragment test_sprd_${board}_defconfig
	echo "check sprd_gki_${board}_debug.fragment"
	generate_config test_sprd_${board}_defconfig  sprd_gki_${board}_debug.fragment test_sprd_${board}_debug_defconfig
	rm -rf $KERNEL_PATH/arch/arm64/configs/test_sprd_${board}_defconfig
	rm -rf $KERNEL_PATH/arch/arm64/configs/test_sprd_${board}_debug_defconfig


done
#sort the sprd_gki.fragment
echo "check sprd_gki.fragment"
generate_config gki_defconfig sprd_gki.fragment test_gki_sprd_defconfig
rm -rf $KERNEL_PATH/arch/arm64/configs/test_gki_sprd_defconfig
exit $RES
