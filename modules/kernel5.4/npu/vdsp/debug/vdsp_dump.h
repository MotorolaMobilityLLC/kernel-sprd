/*
 * SPDX-FileCopyrightText: 2020 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2020 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */


#ifndef __VDSP_DUMP_FILE__
#define __VDSP_DUMP_FILE__

int32_t xrp_save_file(const char *filename, const char *buffer, uint32_t size);
int32_t xrp_dump_libraries(struct xvp *xvp);
#endif
