#ifndef _TRANSSION_GESTURE_H_
#define _TRANSSION_GESTURE_H_
#define GESTURE_NODE "gesture_function"

struct transsion_gesture_if {
	int support_gesture;	// 芯片是否支持手势唤醒，0x01为支持，0x00为不支持。
	int (*global_gesture_enable)(int onoff);
	int (*single_gesture_enable)(int onoff, unsigned char gesture);
    unsigned char (*get_gesture_data)(void);
};

enum GESTURE_TYPE
{
    GESTURE_DOUBLE = 0,
    GESTURE_MUSIC = 1,
    GESTURE_CHAR = 2,
    GESTURE_CHAR_ALL = 3,
    GESTURE_SPECIAL = 4,
    GESTURE_ERR,
};

struct hios_gestre_map_s
{
    char cmd[2];
    unsigned char code;
    enum GESTURE_TYPE gesture_type;
};

#define GESTURE_LF              0xBB
#define GESTURE_RT              0xAA
#define GESTURE_down            0xAB
#define GESTURE_up              0xBA
#define GESTURE_DC              0xCC
#define GESTURE_o               0x6F
#define GESTURE_w               0x77
#define GESTURE_m               0x6D
#define GESTURE_e               0x65
#define GESTURE_c               0x63
#define GESTURE_s               0x73
#define GESTURE_v               0x76
#define GESTURE_z               0x7A

#define GESTRE_ON '1'
#define GESTRE_OFF '2'

int transsion_gesture_register(struct transsion_gesture_if * tp_if);
#endif
