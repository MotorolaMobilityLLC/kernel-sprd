#ifndef _UAPI_LINUX_AUTOTEST_H_
#define _UAPI_LINUX_AUTOTEST_H_

#define AUTOTEST_MAGIC  'A'

#define AT_PINCTRL	_IOWR(AUTOTEST_MAGIC, 0, int)/* Set at_pinctrl cmd*/
#define AT_GPIO		_IOWR(AUTOTEST_MAGIC, 1, int)/* Set at_gpio cmd */

#endif /* _UAPI_LINUX_AUTOTEST_H_ */
