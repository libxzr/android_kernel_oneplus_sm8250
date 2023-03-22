/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#ifndef _OPLUS_CFS_FORK_H_
#define _OPLUS_CFS_FORK_H_

static inline void init_task_ux_info(struct task_struct *p)
{
	p->static_ux = 0;
#ifdef CONFIG_CAMERA_OPT
	p->camera_opt = 0;
#endif
	atomic64_set(&(p->dynamic_ux), 0);
	INIT_LIST_HEAD(&p->ux_entry);
	p->ux_depth = 0;
	p->enqueue_time = 0;
	p->dynamic_ux_start = 0;
#ifdef CONFIG_MMAP_LOCK_OPT
	p->ux_once = 0;
	p->get_mmlock = 0;
	p->get_mmlock_ts = 0;
#endif
}
#endif
