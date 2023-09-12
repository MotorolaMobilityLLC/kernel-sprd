/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Priority Dvfs_Queue for Dvfs.
 *
 * In order to ensure the highest freqency node is always the first node
 * and the freqency with same pid will overwrite by it's latest value.
 *
 */

#ifndef __DVFS_PRI_QUEUE_H__
#define __DVFS_PRI_QUEUE_H__

#include <linux/kernel.h>
#include <linux/slab.h>

/* *
 * struct Dvfs_Queue
 *
 * @pid pid of the process
 * @freq freqency for the device
 * @active wether the process enable the freqency point for the device.
 *
 */

typedef struct _Dvfs_Queue

    {
    long pid;
    unsigned long freq;
    int active;
} Dvfs_Queue;

/* *
 * struct PQueue
 *
 * @Dvfs_Queue struct Dvfs_Queu
 * @_PNode the node pointer of the queue
 */

typedef struct _PNode

    {
    Dvfs_Queue DvfsQ;
    struct _PNode *next;
} PNode, *PQueue;

void InitPQueue(PQueue pQueue);
PNode *CreateNode(Dvfs_Queue DvfsQ);
/* *
 * fun GetFrontNodeFreq.The freqency of first node is the highest.
 *
 * @pQueue the head of queue
 *
 */

unsigned long GetFrontNodeFreq(PQueue pQueue);
void Destroy(PQueue pQueue);
/* *
 * struct PQueue
 *
 * @DvfsQ Dvfs_Queue data
 * @pQueue the head of queue
 */
void AddNode(PQueue pQueue, Dvfs_Queue DvfsQ);
/* *
 * fun DelNode
 *
 * @pQueue  pQueue->next will be deleted.
 *
 */
void DelNode(PQueue pQueue);

/* *
 * fun MatchPid_RemoveInvalidNode. remove the invalid enode base on the pid
 * and active
 *
 * @pQueue  pQueue->next will be deleted.
 *
 */
int MatchPid_RemoveInvalidNode(PQueue pQueue, Dvfs_Queue DvfsQ);
void ShowQueue(PQueue pQueue);
bool IsEmpty(PQueue pQueue);

#endif
