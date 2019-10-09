export BSP_KERNEL_ARCH="arm64"
export BSP_KERNEL_DIFF_CONFIG_ARCH="sprd-diffconfig/android/sharkl3/$BSP_KERNEL_ARCH"
export BSP_KERNEL_DIFF_CONFIG_COMMON="sprd-diffconfig/android/sharkl3/common"
if [ -z $BSP_KERNEL_PATH ]; then
	BSP_KERNEL_PATH="."
fi
export BSP_KERNEL_CROSS_COMPILE=$(readlink -f "$BSP_KERNEL_PATH/../../toolchain/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-")

# BSP_MAKE_EXTRA_ARGS can't include null-value-fields "CC=" "LD="(just remove the null-value-field)
# Sample: export BSP_MAKE_EXTRA_ARGS="CC=clang"
export BSP_MAKE_EXTRA_ARGS="CC=clang"
BSP_CLANG_PREBUILT_BIN=$(readlink -f "$BSP_KERNEL_PATH/../../toolchain/prebuilts/clang/host/linux-x86/clang-r353983c/bin")
export CLANG_TRIPLE=aarch64-linux-gnu-
export BSP_TOOL_PATH=$BSP_CLANG_PREBUILT_BIN
export PATH=${BSP_TOOL_PATH}:${PATH//"${BSP_TOOL_PATH}:"}

export BSP_BOARD_NAME="s9863a2h10"

export BSP_BOARD_WCN_CONFIG=""
export BSP_BOARD_EXT_PMIC_CONFIG=""
export BSP_PRODUCT_GO_DEVICE=""
export BSP_BOARD_FEATUREPHONE_CONFIG=""
export BSP_BOARD_TEE_CONFIG=""

if [ "$BSP_BOARD_FEATUREPHONE_CONFIG" == "true" ]; then
	export BSP_BOARD_TEE_64BIT="false"
else
	export BSP_BOARD_TEE_64BIT="true"
fi

