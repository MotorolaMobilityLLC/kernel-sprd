#ifndef __VENDOR_CFG_H__
#define __VENDOR_CFG_H__
#include "adaptive_ts.h"
//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
extern int nt36xxx_init(void);
extern void nt36xxx_exit(void);
extern int nvt_ts_check_chip_ver_trim(void);
//#endif
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
//extern int DummyController_init(void);
//extern void DummyController_exit(void);

struct lcd_match_info fts_match_info[] = {
    {"lcd872201@872201" , 0xf0},
    {"lcd800601@800601" , 0xe0},
    {"lcd8006S-AB@8006S-AB" , 0xe0},
};


static struct verdor_cfg supported_verdor[] ={
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_TLSC6X)
    {
        .chipid = 0x36,
        .vendor = "tlsc6x",
        .name = "TLSC6X",
        .match_info = NULL ,
        .match_info_count = 0,
        .read_chipid = tlsc6x_read_chipid,
        .verdor_init = tlsc6x_init,
        .verdor_exit = tlsc6x_exit,
        .force_match = 1,
    },
#endif
//#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX)||defined(CONFIG_TOUCHSCREEN_ADAPTIVE_NT36XXX_SPI)
    {
        .chipid = 1,
        .vendor = "nt36xxx",
        .name = "NT36XXX",
        .match_info = NULL ,
        .match_info_count = 0,
        .read_chipid = nvt_ts_check_chip_ver_trim,
        .verdor_init = nt36xxx_init,
        .verdor_exit = nt36xxx_exit,
        .force_match = 0,
    },
//#endif
#if defined(CONFIG_TOUCHSCREEN_ADAPTIVE_ILI9882H)
    {
        .chipid = 1,
        .vendor = "ili9882h",
        .name = "ILI9882H",
        .match_info = NULL ,
        .match_info_count = 0,
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
        .match_info = NULL ,
        .match_info_count = 0,
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
        .match_info = fts_match_info ,
        .match_info_count = ARRAY_SIZE(fts_match_info),
        .read_chipid = ft8722_read_id,
        .verdor_init = ft8722_init,
        .verdor_exit = ft8722_exit,
        .force_match = 0,
    },
#endif

/* warning: the dummy must be the last item in this array */
   /* {
        .chipid = 0xff,
        .vendor = "sprd",
        .name = "dummy_ts",
        .read_chipid = NULL,
        .match_info = NULL ,
        .match_info_count = 0,
        .verdor_init = DummyController_init,
        .verdor_exit = DummyController_exit,
        .force_match = 0,
    },*/
};

#endif
