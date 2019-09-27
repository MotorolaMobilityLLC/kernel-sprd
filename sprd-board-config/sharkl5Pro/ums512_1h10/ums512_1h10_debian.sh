source sprd-board-config/sharkl5Pro/ums512_1h10/ums512_1h10_base.sh

export BSP_BOARD_TEE_CONFIG="trusty"
export BSP_BOARD_WCN_CONFIG="built-in"

#DEFCONFIG
export BSP_KERNEL_DEFCONFIG="sprd_sharkl5Pro_defconfig"

#DTS
export BSP_DTB="ums512-1h10"
export BSP_DTBO="ums512-1h10-overlay"

export BSP_BOARD_DEBIAN_CONFIG="true"
