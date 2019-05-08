export BSP_KERNEL_PATH=$(readlink -f $(dirname $0)/../../)

function get_board()
{
	SEARCH_DIR="sprd-board-config"
	BOARD_PATH=`find $BSP_KERNEL_PATH/$SEARCH_DIR -type f | grep -v "base"`
	for board_path in $BOARD_PATH;
	do
		board_name=`echo $board_path | awk -F"$SEARCH_DIR/" {'print $2'} | awk -F"/" {'if (NF == 3) print $NF'} | awk -F"." {'print $1'}`
		if [ -n "$board_name" ]; then
			d_board+=([$board_name]="$board_path")
			l_board=("${l_board[@]}" "$board_name")
		fi
	done
}

function print_header()
{
	echo "Lunch menu... pick a combo:"
	echo
	echo "You're building on Linux"
	echo "Pick a number:"
	echo "choice a project"

	for i in $(seq 0 $((${#l_board[@]}-1)) );
	do
		printf "  %d. %s-%s\n" $((i+1)) ${l_board[$i]} 'userdebug'
	done

	echo "Which would you like?"
}

function chooseboard()
{
	if [ $# == 0 ]; then
		print_header
		read -a get_line
	elif [ $# == 1 ]; then
		get_line=$1
	else
		echo "The num of parameter is error."
		return -1
	fi

	if (echo -n $get_line | grep -q -e "^[0-9][0-9]*$"); then
		if [ "$get_line" -le "${#l_board[@]}" ] ; then
			input_board=${l_board[$((get_line-1))]}
			export_board=$input_board"-userdebug"
			BSP_BUILD_VARIANT="userdebug"
		else
			echo "The num you input is out of range, please check."
			return -1
		fi
	else
		input_board=`echo $get_line | awk -F"-" {'print $1'}`
		if [ -n $d_board[$input_board] ]; then
			BSP_BUILD_VARIANT=`echo $get_line | awk -F"-" {'print $2'}`
			if [ $BSP_BUILD_VARIANT != "userdebug" ] && [ $BSP_BUILD_VARIANT != "user" ]; then
				echo "The board name was error, please check."
				return -1
			else
				export_board=$input_board"-"$BSP_BUILD_VARIANT
			fi
		else
			echo "The board name was error, please check."
			return -1
		fi
	fi

	return 0
}

function clean()
{
	input_board=''
	export_board=''
	get_line=''
	unset l_board d_board
}

function print_setinfo()
{
	export BSP_BOARD_NAME=$export_board
	export BSP_BOARD_PATH=${d_board["$input_board"]}

	echo "BSP_BOARD_NAME  : " $BSP_BOARD_NAME
	echo "BSP_BOARD_PATH  : " $BSP_BOARD_PATH
}

function lunch()
{
	declare -A d_board

	get_board
	chooseboard $1
	if [ $? == 0 ]; then
		print_setinfo
	fi
	clean
}

function sprd_create_user_config()
{
	if [ $# -ne 2 ]; then
		echo "Parameters error! Please check."
	fi

	bash scripts/sprd_create_user_config.sh $1 $2
}

function add_diffconfig()
{
	if [ -z $BSP_KERNEL_OUT ]; then
		echo ++++++++++++++++++Ruifeng++++++++++++++++
		BSP_KERNEL_OUT="."
	fi
	KERNEL_CONFIG="$BSP_KERNEL_OUT/.config"
	BSP_BOARD_SPEC_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/$BSP_BOARD_NAME"_diff_config"
	sprd_create_user_config $KERNEL_CONFIG $BSP_BOARD_SPEC_CONFIG

	if [ "$BOARD_TEE_CONFIG" == "trusty" ]; then
		if [ -n $BOARD_TEE_64BIT ]; then
			if [ "$BOARD_TEE_64BIT" == "false" ]; then
				BSP_DEVICE_TRUSTY_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/trusty_aarch32_diff_config
			else
				BSP_DEVICE_TRUSTY_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/trusty_aarch64_diff_config
			fi
		else
			BSP_DEVICE_TRUSTY_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/trusty_diff_config
		fi

		sprd_create_user_config $KERNEL_CONFIG $BSP_DEVICE_TRUSTY_CONFIG
	fi

	if [ "$BSP_BOARD_WCN_CONFIG" == "ext" ]; then
		BSP_DEVICE_WCN_CONFIG=$BSP_KERNEL_DIFF_CONFIG_COMMON/wcn_ext_diff_config
	elif [ "$BSP_BOARD_WCN_CONFIG" == "built-in" ]; then
		BSP_DEVICE_WCN_CONFIG=$BSP_KERNEL_DIFF_CONFIG_COMMON/wcn_built_in_diff_config
	fi

	if [ -n $BSP_BSP_DEVICE_WCN_CONFIG ]; then
		sprd_create_user_config $KERNEL_CONFIG $BSP_DEVICE_WCN_CONFIG
	fi

	if [ "$BSP_PRODUCT_GO_DEVICE" == "true" ]; then
		BSP_GO_DEVICE_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/go_google_diff_config
		sprd_create_user_config $KERNEL_CONFIG $BSP_GO_DEVICE_CONFIG
	fi

	if [ "$BSP_BUILD_VARIANT" == "user" ]; then
		if [ "$BSP_BUILD_VARIANT" == "user" ]; then
			if [ "$BSP_KERNEL_ARCH" == "arm" ]; then
				BSP_DEVICE_USER_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/aarch32_user_diff_config
			elif [ "$BSP_KERNEL_ARCH" == "arm64" ]; then
				BSP_DEVICE_USER_CONFIG=$BSP_KERNEL_DIFF_CONFIG_ARCH/aarch64_user_diff_config
			fi
		fi

		sprd_create_user_config $KERNEL_CONFIG $BSP_DEVICE_USER_CONFIG
	fi
}

function pre_build()
{
	BSP_OBJ=`cat /proc/cpuinfo | grep processor | wc -l`

	source $BSP_BOARD_PATH
}

function make_all()
{
	make_config
	make_kernel
	make_dtbs
}

function make_kernel()
{
	make_config
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE -j$BSP_OBJ
	if [ -n $BSP_KERNEL_DIST ]; then
		mkdir $BSP_KERNEL_DIST -p
		find $BSP_KERNEL_OUT -name $BSP_DTB.dtb | xargs -i cp {} $BSP_KERNEL_DIST
		find $BSP_KERNEL_OUT -name $BSP_DTBO.dtbo | xargs -i cp {} $BSP_KERNEL_DIST
	fi
}

function make_config()
{
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE $BSP_KERNEL_DEFCONFIG -j$BSP_OBJ
	add_diffconfig
}

function make_kuconfig()
{
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE $BSP_KERNEL_DEFCONFIG menuconfig -j$BSP_OBJ
	cp $BSP_KERNEL_OUT/.config $BSP_KERNEL_PATH/arch/$BSP_KERNEL_ARCH/configs/$BSP_KERNEL_DEFCONFIG
}

function make_dtb()
{
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE dtbs -j$BSP_OBJ
	if [ -n $BSP_KERNEL_DIST ]; then
		mkdir $BSP_KERNEL_DIST -p
		find $BSP_KERNEL_OUT -name $BSP_DTB.dtb | xargs -i cp {} $BSP_KERNEL_DIST
	fi
}

function make_dtbo()
{
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE dtbs -j$BSP_OBJ
	if [ -n $BSP_KERNEL_DIST ]; then
		mkdir $BSP_KERNEL_DIST -p
		find $BSP_KERNEL_OUT -name $BSP_DTBO.dtbo | xargs -i cp {} $BSP_KERNEL_DIST
	fi
}

function make_modules()
{
	if [ -n $BSP_KERNEL_DIST ]; then
		mkdir $BSP_KERNEL_DIST -p
		make -C $BSP_KERNEL_OUT O=$BSP_KERNEL_OUT $BSP_CC_LD_ARG INSTALL_MOD_PATH=$BSP_KERNEL_DIST modules_install -j$BSP_OBJ
	fi
}

function make_headers()
{
	if [ -n $BSP_KERNEL_DIST ]; then
		make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE \
			$BSP_CC_LD_ARG INSTALL_HDR_PATH="$BSP_KERNEL_HEADERS_DIR/usr" headers_install -j$BSP_OBJ
		find $BSP_KERNEL_HEADERS_DIR \( -name ..install.cmd -o -name .install \) -exec rm '{}' +
		BSP_KERNEL_HEADER_TAR=$BSP_KERNEL_DIST/kernel-uapi-headers.tar.gz
		tar -czPf $BSP_KERNEL_HEADER_TAR --directory=$BSP_KERNEL_HEADERS_DIR usr/
		tar -xf $BSP_KERNEL_HEADER_TAR -C $BSP_KERNEL_DIST
	fi
}

function make_clean()
{
	make -C $BSP_KERNEL_PATH O=$BSP_KERNEL_OUT ARCH=$BSP_KERNEL_ARCH CROSS_COMPILE=$BSP_KERNEL_CROSS_COMPILE mrproper -j$BSP_OBJ
}

lunch $1

pre_build

if [ "$2" == "all" ]; then
	make_all
elif [ "$2" == "config" ]; then
	make_config
elif [ "$2" == "kernel" ]; then
	make_kernel
elif [ "$2" == "dtb" ]; then
	make_dtb
elif [ "$2" == "dtbo" ]; then
	make_dtbo
elif [ "$2" == "modules" ]; then
	make_modules
elif [ "$2" == "headers" ]; then
	make_headers
elif [ "$2" == "kuconfig" ]; then
	make_kuconfig
elif [ "$2" == "clean" ]; then
	make_clean
fi

