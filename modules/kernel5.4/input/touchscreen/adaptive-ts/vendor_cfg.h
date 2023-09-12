#ifndef __VENDOR_CFG_H__
#define __VENDOR_CFG_H__
#include "adaptive_ts.h"
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
extern int nt36xxx_init(void);
extern void nt36xxx_exit(void);
extern int nvt_ts_check_chip_ver_trim(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
extern int ili9882h_init(void);
extern void ili9882h_exit(void);
extern int ili9882h_read_id(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
extern int tlsc6x_init(void);
extern void tlsc6x_exit(void);
extern int tlsc6x_read_chipid(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X)
extern int icnl9911x_init(void);
extern void icnl9911x_exit(void);
extern int icnl9911x_driver_get_hwid(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FT8722)
extern int ft8722_read_id(void);
extern int ft8722_init(void);
extern void ft8722_exit(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TD4160)
extern int ovt_tcm_read_id(void);
extern int ovt_tcm_init(void);
extern void ovt_tcm_exit(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_BL6XX1)
extern int betterlife_read_id(void);
extern int betterlife_init(void);
extern void betterlife_exit(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FT5X46)
extern int focaltech_read_id(void);
extern int focaltech_init(void);
extern void focaltech_exit(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FTS6436U)
extern int ft6436u_read_id(void);
extern int ft6436u_init(void);
extern void ft6436u_exit(void);
#endif

extern int DummyController_init(void);
extern void DummyController_exit(void);

static struct verdor_cfg supported_verdor[] ={
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
    {
        .chipid = 0x36,
        .vendor = "tlsc6x",
        .name = "TLSC6X",
        .read_chipid = tlsc6x_read_chipid,
        .verdor_init = tlsc6x_init,
        .verdor_exit = tlsc6x_exit,
        .force_match = 1,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FT5X46)
    {
        .chipid = 0x54,
        .vendor = "focaltech",
        .name = "FT5x46",
        .read_chipid = focaltech_read_id,
        .verdor_init = focaltech_init,
        .verdor_exit = focaltech_exit,
        .force_match = 1,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
    {
        .chipid = 1,
        .vendor = "nt36xxx",
        .name = "NT36XXX",
        .read_chipid = nvt_ts_check_chip_ver_trim,
        .verdor_init = nt36xxx_init,
        .verdor_exit = nt36xxx_exit,
        .force_match = 0,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
    {
        .chipid = 1,
        .vendor = "ili9882h",
        .name = "ILI9882H",
        .read_chipid = ili9882h_read_id,
        .verdor_init = ili9882h_init,
        .verdor_exit = ili9882h_exit,
        .force_match = 0,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X)
    {
        .chipid = 1,
        .vendor = "icnl9911x",
        .name = "ICNL991X",
        .read_chipid = icnl9911x_driver_get_hwid,
        .verdor_init = icnl9911x_init,
        .verdor_exit = icnl9911x_exit,
        .force_match = 0,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FT8722)
    {
        .chipid = 1,
        .vendor = "focaltech",
        .name = "FT8722",
        .read_chipid = ft8722_read_id,
        .verdor_init = ft8722_init,
        .verdor_exit = ft8722_exit,
        .force_match = 0,
    },
#endif

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TD4160)
    {
        .chipid = 1,
        .vendor = "omnivision_tcm",
        .name = "TD4160",
        .read_chipid = ovt_tcm_read_id,
        .verdor_init = ovt_tcm_init,
        .verdor_exit = ovt_tcm_exit,
        .force_match = 0,
    },
#endif

#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_BL6XX1)
    {
        .chipid = 1,
        .vendor = "betterlife",
        .name = "BL6XX1",
        .read_chipid = betterlife_read_id,
        .verdor_init = betterlife_init,
        .verdor_exit = betterlife_exit,
        .force_match = 0,
    },
#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_FTS6436U)
    {
        .chipid = 1,
        .vendor = "focaltech",
        .name = "FT6436U",
        .read_chipid = ft6436u_read_id,
        .verdor_init = ft6436u_init,
        .verdor_exit = ft6436u_exit,
        .force_match = 0,
    },
#endif
/* warning: the dummy must be the last item in this array */
    {
        .chipid = 0xff,
        .vendor = "sprd",
        .name = "dummy_ts",
        .read_chipid = NULL,
        .verdor_init = DummyController_init,
        .verdor_exit = DummyController_exit,
        .force_match = 0,
    },
};

#endif
