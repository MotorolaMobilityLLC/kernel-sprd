source sprd-board-config/sharkl5Pro/ums512_20c10/ums512_20c10_base.sh

export BSP_BOARD_TEE_CONFIG="trusty"
export BSP_BOARD_WCN_CONFIG="built-in"

#DEFCONFIG
export BSP_KERNEL_DEFCONFIG="sprd_sharkl5Pro_defconfig"

#DTS
export BSP_DTB="ums512-20c10"
export BSP_DTBO="ums512-20c10-overlay"
