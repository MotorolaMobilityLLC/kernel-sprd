static u8 icnl9911_driver_builtin_firmware[] = {
	#include "firmware/cts_builtin_firmware_trd.h"
};

static u8 icnl9911cac_driver_builtin_firmware[] = {
	#include "firmware/cts_builtin_firmware_cac.h"
};
const static struct cts_firmware cts_driver_builtin_firmwares[] = {
    {
        .name = "OEM-Project",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .data = icnl9911_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware),
    },

};

const static struct cts_firmware cts_driver_builtin_firmwares_cac[] = {
    {
        .name = "OEM-Project cac",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .data = icnl9911cac_driver_builtin_firmware,
        .size = ARRAY_SIZE(icnl9911cac_driver_builtin_firmware),
    },
};
