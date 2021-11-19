#ifndef SPRD_USERLOG_H
#define SPRD_USERLOG_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct userlog_entry {
	__u16		len;
	__u16		hdr_size;
	__s32		pid;
	__s32		tid;
	char		*comm;
	time64_t	sec;			/* seconds */
	long		nsec;		/* nanoseconds */
	ktime_t 	time;
	char		msg[0];
};

#define USERLOG_SYSTEM	"userlog_point"	/* system user point messages */
#define USERLOG_ENTRY_MAX_PAYLOAD	4076

#endif

