#ifndef SPRD_USERLOG_H
#define SPRD_USERLOG_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct userlog_entry {
	__u16		len;
};

#define USERLOG_SYSTEM	"userlog_point"	/* system user point messages */
#define USERLOG_ENTRY_MAX_PAYLOAD	4076

#endif

