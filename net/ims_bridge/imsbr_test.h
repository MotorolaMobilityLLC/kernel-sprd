#ifndef _IMSBR_TEST_H
#define _IMSBR_TEST_H

int imsbr_test_init(void);
void imsbr_test_exit(void);

#define IMSBR_LTP_CASE_HO_WIFI2LTE	(1)
#define IMSBR_LTP_CASE_HO_LTE2WIFI	(2)
#define IMSBR_LTP_CASE_HO_FINISH	(3)
#define IMSBR_LTP_CASE_FRAGSIZE		(4)
#define IMSBR_LTP_CASE_PING			(5)
#define IMSBR_LTP_CASE_V4_OUTPUT	(6)
#define IMSBR_LTP_CASE_V6_OUTPUT	(7)
#define IMSBR_LTP_CASE_V4_INPUT		(8)
#define IMSBR_LTP_CASE_V6_INPUT		(9)
#define IMSBR_LTP_CASE_CP_TUPLE		(10)
#define IMSBR_LTP_CASE_AP_TUPLE		(11)
#define IMSBR_LTP_CASE_SIPC			(12)
#define IMSBR_LTP_CASE_PRESSURE		(13)

#endif
