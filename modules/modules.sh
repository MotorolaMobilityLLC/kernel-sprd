
my_top_dir=$PWD

kernel_out_dir=$my_top_dir/out/target/product/sabahlite/obj/kernel5.4

clang_1=$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/bin/clang

make=$my_top_dir/prebuilts/build-tools/linux-x86/bin/make

aarch64_linux_android_=$my_top_dir/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

export ARCH=arm64 LLVM=1 LLVM_LAS=1 INSTALL_MOD_STRIP=1 CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-androidkernel- SUBARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- LD_LIBRARY_PATH=$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/lib64:$$LD_LIBRARY_PATH PATH=$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/bin/:$aarch64_linux_android_:$PATH BSP_MODULES_OUT=$kernel_out_dir BSP_KERNEL_PATH=$my_top_dir/kernel5.4 BSP_BOARD_BASE_PATH=$my_top_dir/kernel5.4/modules BSP_KERNEL_OUT=$kernel_out_dir KERNEL_MAKE_ARGS="ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabi- INSTALL_MOD_STRIP=1" BSP_NPROC=96 CONFIG_MALI_PLATFORM_NAME=qogirl6 CONFIG_TARGET_BOARD=sabahlite BSP_ROOT_DIR=$my_top_dir BSP_KERNEL_VERSION=kernel5.4

function source_configuration()
{
	# the max number of parameters is 1, one of "chipram uboot tos sml kernel"
	# if "$1".is null, only include the common.cfg

	if [[ -f $BSP_BOARD_BASE_PATH/sabahlite_base/$1.cfg ]]; then
		echo "source $BSP_BOARD_BASE_PATH/sabahlite_base/$1.cfg"
		source $BSP_BOARD_BASE_PATH/sabahlite_base/$1.cfg
	fi

}

function compile_modules()
{
	BSP_MOD_NAME="$BSP_MOD".ko

	if (cat $BSP_MOD_PATH | grep -w "$BSP_MOD\.o\|KO_MODULE_NAME\s*:=\s*$BSP_MOD" > /dev/null); then
		echo ======================================================================
		echo MAKE $BSP_MOD.o $BSP_MOD_PATH
		echo ======================================================================
		make -C ${BSP_MOD_PATH%/*} -f Makefile O=$BSP_KERNEL_OUT $KERNEL_MAKE_ARGS modules -j$BSP_NPROC

		make -C ${BSP_MOD_PATH%/*} -f Makefile O=$BSP_KERNEL_OUT $KERNEL_MAKE_ARGS INSTALL_MOD_PATH=$BSP_MODULES_OUT modules_install -j$BSP_NPROC

	fi
}

function do_make_modules()
{
	BSP_KERNEL_VERSION=kernel5.4
    
	source_configuration chipram
	source_configuration common
	source_configuration kernel
	source_configuration modules
	
	
	export specified_ko
	shift
	while getopts ":m:" opt
	do
		case "$opt" in
		m) specified_ko=$OPTARG ;;
		*) ;;
		esac
	done

	mkdir $BSP_MODULES_OUT -p

	cd $BSP_ROOT_DIR
	BSP_MOD_PATH_LIST=`find $BSP_ROOT_DIR/kernel5.4/modules -type f -name "Makefile" | grep -E "$BSP_KERNEL_VERSION|camera"`

	if [[ -z $specified_ko ]]; then
		local bsp_modules_list_result bsp_modules_del

		bsp_modules_list_result=$BSP_MODULES_LIST$BSP_MODULES_LIST_ADD

		for bsp_modules_del in $BSP_MODULES_LIST_DEL;
		do
			bsp_modules_list_result=`echo $bsp_modules_list_result | sed "s/$bsp_modules_del//g"`
		done

		echo ======================================================================
		echo COMPILE MODULES: $bsp_modules_list_result
		echo ======================================================================

		for BSP_MOD_NAME in $bsp_modules_list_result;
		do
			BSP_MOD=$(echo $BSP_MOD_NAME | awk -F".ko" {'print $1'})

			for BSP_MOD_PATH in $BSP_MOD_PATH_LIST;
			do
				BSP_MOD_PATH=$BSP_MOD_PATH
				compile_modules
			done
		done
	else
		echo "Only compile the specified ko named $specified_ko."
		BSP_MOD=$(echo $specified_ko | awk -F".ko" {'print $1'})
		for BSP_MOD_PATH in $BSP_MOD_PATH_LIST;
		do
			BSP_MOD_PATH=$BSP_MOD_PATH
			compile_modules
		done
	fi

	export make
}

do_make_modules
