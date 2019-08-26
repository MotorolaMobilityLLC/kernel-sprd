source sprd-board-config/sharkl3/s9863a2h10/s9863a2h10_base.sh

export BSP_BOARD_TEE_CONFIG="trusty"
export BSP_BOARD_WCN_CONFIG="built-in"

#DEFCONFIG
export BSP_KERNEL_DEFCONFIG="sprd_sharkl3_defconfig"

#DTS
export BSP_DTB="sp9863a-2h10"
export BSP_DTBO="sp9863a-2h10-overlay"
