#!/bin/bash

OLD_PATH=$OLDPWD

tmp_path_def="./tmp_config_check/"

if [[ ! -f Makefile ]] || [[ `cat Makefile | head -n 2 | tail -n 1 | awk -F" " {'print $1'}` != "VERSION" ]]; then
	echo "Please use it in kernel source directroy."
	exit 1
fi

if [[ $# -eq 0 ]]; then
	clang_version=`cat ../../build/set_toolchain.sh | grep clang-r | awk -F"/" {'print $(NF-1)'} | tail -n 1`
	clang_path=$(readlink -f "../../toolchain/prebuilts/clang/host/linux-x86/$clang_version/bin")
	gcc_path=$(readlink -f "../../toolchain/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-aarch64-linux-gnu-7.4/bin/aarch64-linux-gnu-")
	# Add clang and gcc absolute path to env
	PATH=$clang_path:$PATH
	PATH=$gcc_path:$PATH
elif [[ $# -eq 2 ]] && [[ $1 == "--env" ]] && [[ $2 == env ]]; then
	echo "Using the TOOLCHAIN in env."
else
	echo "Parameters Error."
	echo "Usage:"
	echo "    ./scripts/sprd/defconfig_update.sh"
	exit 1
fi

# compile the sprd_*_defconfig, named sprd_<SoC>_defconfig, same to arch/arm64/config
sprd_defconfig_list=`find . -name "sprd_*_defconfig"  | grep -v debian`
for sprd_defconfig in $sprd_defconfig_list;
do
	defconfig_name=`echo $sprd_defconfig | awk -F"/" {'print $NF'}`
	arch=`echo $sprd_defconfig | awk -F"/" {'print $3'}`

	if [[ $arch == "arm64" ]]; then
		clang_triple="aarch64-linux-gnu-"
	elif [[ $arch == "arm" ]]; then
		clang_triple="arm-linux-gnueabi-"
	fi

	make CC=clang LD=ld.lld CLANG_TRIPLE=$clang_triple ARCH=$arch CROSS_COMPILE="aarch64-linux-gnu-" $defconfig_name O=$tmp_path_def

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
