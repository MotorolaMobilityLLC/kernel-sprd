/*
 * This file is part of wcn platform
 */

#ifndef __SYSFS_H__
#define __SYSFS_H__

#include <misc/wcn_bus.h>

#define WCN_UEVENT_SOURCE	"SOURCE=wcnmarlin"
#define WCN_UEVENT_FW_ERRO	"EVENT=FW_ERROR"
#define WCN_UEVENT_REASON	"REASON="

int notify_at_cmd_finish(void *buf, unsigned char len);
void wcn_notify_fw_error(enum wcn_source_type type, char *buf);
int wcn_sysfs_get_reset_prop(void);
int wcn_firmware_init(void);

#endif

