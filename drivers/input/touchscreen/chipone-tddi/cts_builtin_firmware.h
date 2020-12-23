static u8 icnl9911_driver_builtin_firmware[] = {
    #include"firmware/easyquick_608/builtin_firmware.h"
};

const static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "Ontim-Moto FIJI easyquick 608",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .data = icnl9911_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware),
    },
};

