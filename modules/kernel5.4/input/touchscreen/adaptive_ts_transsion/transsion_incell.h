#ifndef _TRANSSION_INCELL_H
#define _TRANSSION_INCELL_H

struct transsion_incell_interface {
	void (*prepare_tp_reset) (int);
	/* LCD执行复位之前会调用prepare_lcd_reset，它的返回值要传递给post_lcd_reset的handle */
	int (*prepare_lcd_reset) (void);
	/* LCD执行复位之后会调用post_lcd_reset，传入参数必须是prepare_lcd_reset的返回值!!! */
	void (*post_lcd_reset) (int);
	/* 如果TP的距离感应、手势唤醒功能打开，部分INCELL屏可能不允许LCD断电 */
	int (*need_keep_power) (void);
	/*TP usb plug in/out switch*/
	void (*usb_plug_switch) (void);
};

extern int register_incell_interface(struct transsion_incell_interface *incell);

#endif
