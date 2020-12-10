static u8 icnl9911_driver_builtin_firmware[] = {
    #include"firmware/easyquick_608/Moto-Fiji-Lite-Easyquick-ICNL9911C_Long-V_V020b_20200924.h"
};

const static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "Ontim-Moto FIJI easyquick 608",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911,
        .fwid = CTS_DEV_FWID_ICNL9911,
        .data = icnl9911_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware),
    },
};

