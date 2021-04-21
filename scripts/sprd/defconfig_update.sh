#!/bin/bash

OLD_PATH=$OLDPWD

tmp_path_def="./tmp_config_check"

if [[ ! -f Makefile ]] || [[ `cat Makefile | head -n 2 | tail -n 1 | awk -F" " {'print $1'}` != "VERSION" ]]; then
	echo "Please use it in kernel source directroy."
	exit 1
fi

BSP_EXTRA_ARGS="CC LD LLVM LLVM_IAS"
KERNEL_ROOT_DIR=`pwd`
BSP_ROOT_DIR=$(dirname $(dirname "$KERNEL_ROOT_DIR"))
echo "$BSP_ROOT_DIR"

source $KERNEL_ROOT_DIR/build.config.common
#Use kernel clang_version by default.While get CLANG_PREBUILT_BIN from build.config.common
BSP_CLANG_VERSION=$(echo $CLANG_PREBUILT_BIN| awk -F "linux-x86/|/bin" '{print $(NF-1)}')

if [[ $# -eq 0 ]]; then
	UPDATE_DEFCONFIG="true"
	ARM32_TOOLCHAIN_PATH="$BSP_ROOT_DIR/toolchain/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin"
	PATH=$ARM32_TOOLCHAIN_PATH:$PATH
	BSP_CLANG_PREBUILT_BIN_ABS=$(readlink -f "$BSP_ROOT_DIR/toolchain/prebuilts/clang/host/linux-x86/$BSP_CLANG_VERSION/bin")
	BSP_KERNEL_TOOL_PATH=$BSP_CLANG_PREBUILT_BIN_ABS
	if [[ -n $BSP_KERNEL_TOOL_PATH ]]
	then
		PATH=${BSP_KERNEL_TOOL_PATH}:${PATH//"${BSP_KERNEL_TOOL_PATH}:"}
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

	unset $BSP_EXTRA_ARGS BSP_MAKE_EXTRA_ARGS

	source $KERNEL_ROOT_DIR/build.config.common
	if [[ $arch == "arm64" ]];then
		source $KERNEL_ROOT_DIR/build.config.aarch64
	elif [[ $arch == "arm" ]];then
		source $KERNEL_ROOT_DIR/build.config.arm
	fi

	for key in $BSP_EXTRA_ARGS
	do
		val=$(eval echo \$$key)
		#Eliminate empty [value] items, e.g."CC=".
		if [[ ! -z $val ]];then
			BSP_MAKE_EXTRA_ARGS+="$key=$val "
		fi
	done

	echo ====================================================================
	if [[ $arch == "arm64" ]]; then
		# should be consistent with make_config() in envsetup.sh!
		make $BSP_MAKE_EXTRA_ARGS ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-androidkernel- $defconfig_name O=$tmp_path_def
	elif [[ $arch == "arm" ]]; then
		make $BSP_MAKE_EXTRA_ARGS ARCH=arm CROSS_COMPILE=arm-linux-androidkernel- CLANG_TRIPLE=arm-linux-gnueabi- $defconfig_name O=$tmp_path_def
	fi

	# report error if the sprd defconfig has any change.
	if ! diff $sprd_defconfig $tmp_path_def/.config > /dev/null; then
		echo -e "ERROR: \033[31m #### $sprd_defconfig is not equal with .config #### \033[0m"
		echo "[<] represent defconfig, [>] represent .config"
		diff $sprd_defconfig $tmp_path_def/.config
		if [[ $UPDATE_DEFCONFIG == "true" ]];then
			cp $tmp_path_def/.config $sprd_defconfig
			echo -e "\033[32m #### $sprd_defconfig has been updated now. #### \033[0m"
		fi
	else
		echo -e "\033[32m #### $sprd_defconfig is equal with .config #### \033[0m"
	fi

	rm -rf $tmp_path_def
done

cd $OLD_PATH
