/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  BSD Process Accounting for Linux - Definitions
 *
 *  Author: Marco van Wieringen (mvw@planets.elm.net)
 *
 *  This header file contains the definitions needed to implement
 *  BSD-style process accounting. The kernel accounting code and all
 *  user-level programs that try to do something useful with the
 *  process accounting log must include this file.
 *
 *  Copyright (C) 1995 - 1997 Marco van Wieringen - ELM Consultancy B.V.
 *
 */
#ifndef _ONTIM_CPU_DEVINFO_H
#define _ONTIM_CPU_DEVINFO_H

//Add cpu devide info
extern unsigned int get_devinfo_with_index(unsigned int index); 

#endif	/* _ONTIM_CPU_DEVINFO_H */
