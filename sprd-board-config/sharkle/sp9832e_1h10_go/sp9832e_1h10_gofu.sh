source sprd-board-config/sharkle/sp9832e_1h10_go/sp9832e_1h10_go_base.sh

export BSP_BOARD_TEE_CONFIG="trusty"

#DEFCONFIG
export BSP_KERNEL_DEFCONFIG="sprd_sharkle_defconfig"

#DTS
export BSP_DTB="sp9832e-1h10-gofu"
export BSP_DTBO="sp9832e-1h10-gofu-overlay"
