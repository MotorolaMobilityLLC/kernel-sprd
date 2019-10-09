export BSP_KERNEL_ARCH="arm"
export BSP_KERNEL_DIFF_CONFIG_ARCH="sprd-diffconfig/android/sharkle/$BSP_KERNEL_ARCH"
export BSP_KERNEL_DIFF_CONFIG_COMMON="sprd-diffconfig/android/sharkle/common"
if [ -z $BSP_KERNEL_PATH ]; then
	BSP_KERNEL_PATH="."
fi

export BSP_KERNEL_CROSS_COMPILE="$BSP_KERNEL_PATH/../../toolchain/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-"
export BSP_BOARD_NAME="sp9832e_1h10_go"

export BSP_BOARD_WCN_CONFIG=""
export BSP_BOARD_EXT_PMIC_CONFIG=""
export BSP_PRODUCT_GO_DEVICE="true"
export BSP_BOARD_FEATUREPHONE_CONFIG=""
export BSP_BOARD_TEE_CONFIG=""

export BSP_BOARD_TEE_64BIT="false"

