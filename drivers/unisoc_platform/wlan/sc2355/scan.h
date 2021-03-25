/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright © 2021 Unisoc Technologies Co., Ltd.
 * <https://www.unisoc.com>
 *
 * Abstract: Scan header.
 */

#ifndef __SCAN_H__
#define __SCAN_H__

#include <net/cfg80211.h>

#define SPRD_MAX_SCAN_REQ_IE_LEN	(255)
#define SPRD_MIN_RSSI_THOLD		(-127)

void sc2355_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev);
void sc2355_report_scan_result(struct sprd_vif *vif, u16 chan, s16 rssi,
			       u8 *frame, u16 len);

#endif
