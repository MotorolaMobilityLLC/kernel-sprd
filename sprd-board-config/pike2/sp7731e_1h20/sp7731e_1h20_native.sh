source sprd-board-config/pike2/sp7731e_1h20/sp7731e_1h20_base.sh

export BSP_BOARD_TEE_CONFIG="trusty"

#DEFCONFIG
export BSP_KERNEL_DEFCONFIG="sprd_pike2_defconfig"

#DTS
export BSP_DTB="sp7731e-1h20-native"
export BSP_DTBO="sp7731e-1h20-overlay"
