/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef __PROCESS_MM_RECLAIM_H__
#define __PROCESS_MM_RECLAIM_H__

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagewalk.h>

extern int is_reclaim_should_cancel(struct mm_walk *walk);
extern int __weak  create_process_reclaim_enable_proc(struct proc_dir_entry *parent);
#endif /* __PROCESS_MM_RECLAIM_H__ */
