static u8 icnl9911_driver_builtin_firmware_easyquick_608[] = {
    #include"firmware/easyquick_608/Moto-Fiji-Lite-Easyquick-ICNL9911C_Long-V_V020b_20200924.h"
};

static u8 icnl9911_driver_builtin_firmware_hlt[] = {
    #include"firmware/hlt/Moto_MaltaSC_ChinoE_S6302_Holitech_MDT6.517_ICNL9911C_V0211_20201231.h"
};

const static struct cts_firmware cts_driver_builtin_firmwares_easyquick_608[] = {
    {
        .name = "Ontim-Moto FIJI easyquick 608",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911,
        .fwid = CTS_DEV_FWID_ICNL9911,
        .data = icnl9911_driver_builtin_firmware_easyquick_608,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware_easyquick_608),
    },
};

const static struct cts_firmware cts_driver_builtin_firmwares_hlt[] = {
    {
        .name = "FIJISC HLT 9911C ",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .data = icnl9911_driver_builtin_firmware_hlt,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware_hlt),
    },
};
