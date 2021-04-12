#!/bin/bash

OLD_PATH=$OLDPWD

tmp_path_def="./tmp_config_check/"

if [[ ! -f Makefile ]] || [[ `cat Makefile | head -n 2 | tail -n 1 | awk -F" " {'print $1'}` != "VERSION" ]]; then
	echo "Please use it in kernel source directroy."
	exit 1
fi

KERNEL_ROOT_DIR=`pwd`
BSP_ROOT_DIR=$(dirname $(dirname "$KERNEL_ROOT_DIR"))
echo "$BSP_ROOT_DIR"

if [[ $# -eq 0 ]]; then
	ARM32_TOOLCHAIN_PATH="$BSP_ROOT_DIR/toolchain/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin"
	PATH=$ARM32_TOOLCHAIN_PATH:$PATH

	# make sure to use the same version of clang as in our build system
	has_clang=$(echo $PATH | grep "clang-r")
	if [ -z $has_clang ];then
		echo "ERROR: clang not in env, please \"source/lunch\" at bsp first."
		exit 1
	fi
elif [[ $# -eq 2 ]] && [[ $1 == "--env" ]] && [[ $2 == env ]]; then
	echo "Using the TOOLCHAIN in env."
else
	echo "ERROR: Parameters Error."
	echo "Usage:"
	echo -e "\t./scripts/sprd/defconfig_update.sh"
	exit 1
fi

# compile the sprd_*_defconfig, named sprd_<SoC>_defconfig, same to arch/arm64/config
sprd_defconfig_list=`find . -name "sprd_*_defconfig"  | grep -v debian`
for sprd_defconfig in $sprd_defconfig_list;
do
	defconfig_name=`echo $sprd_defconfig | awk -F"/" {'print $NF'}`
	arch=`echo $sprd_defconfig | awk -F"/" {'print $3'}`

	if [[ $arch == "arm64" ]]; then
		# should be consistent with make_config() in envsetup.sh!
		make LLVM=1 LLVM_IAS=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-androidkernel- $defconfig_name O=$tmp_path_def
	elif [[ $arch == "arm" ]]; then
		make LLVM=1 ARCH=arm CROSS_COMPILE=arm-linux-androidkernel- CLANG_TRIPLE=arm-linux-gnueabi- $defconfig_name O=$tmp_path_def
	fi

	cp $tmp_path_def/.config $sprd_defconfig

	# report error if the sprd defconfig has any change.
	if [[ `git diff $sprd_defconfig | wc -l` -ne 0 ]]; then
		echo ====================================================================
		echo ERROR: $sprd_defconfig
		git --no-pager diff $sprd_defconfig
	fi

	rm -rf $tmp_path_def
done

cd $OLD_PATH
