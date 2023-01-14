/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#ifndef _OPLUS_CFS_BINDER_H_
#define _OPLUS_CFS_BINDER_H_
#include "uifirst_sched_common.h"
extern const struct sched_class rt_sched_class;
static inline void binder_set_inherit_ux(struct task_struct *thread_task, struct task_struct *from_task)
{
	if (from_task && test_set_dynamic_ux(from_task) && !test_task_ux(thread_task)) {
		dynamic_ux_enqueue(thread_task, DYNAMIC_UX_BINDER, from_task->ux_depth);
	}
#ifdef CONFIG_CAMERA_OPT
	else if (from_task && from_task->camera_opt && !thread_task->camera_opt) {
		thread_task->camera_opt = 2;
	}
#endif
	else if (from_task && (from_task->sched_class == &rt_sched_class)) {
		if (!test_task_ux(thread_task))
			dynamic_ux_enqueue(thread_task, DYNAMIC_UX_BINDER, from_task->ux_depth);
	}

	/* set heavyload task mark */
	if (from_task && is_heavy_ux_task(from_task) && !is_heavy_ux_task(thread_task))
		dynamic_ux_inc(thread_task, DYNAMIC_UX_HEAVY);
}

static inline void binder_set_inherit_ux_sf(struct task_struct *thread_task, struct task_struct *from_task)
{
	if (from_task && !test_task_ux(thread_task)) {
		dynamic_ux_enqueue(thread_task, DYNAMIC_UX_BINDER, from_task->ux_depth);
	}
}

static inline void binder_unset_inherit_ux(struct task_struct *thread_task)
{
	if (test_dynamic_ux(thread_task, DYNAMIC_UX_BINDER)) {
		dynamic_ux_dequeue(thread_task, DYNAMIC_UX_BINDER);
	}
#ifdef CONFIG_CAMERA_OPT
	else if (thread_task->camera_opt == 2) {
		thread_task->camera_opt = 0;
	}
#endif

	/* clear heavyload task mark*/
	if (thread_task && test_dynamic_ux(thread_task, DYNAMIC_UX_HEAVY))
		dynamic_ux_sub(thread_task, DYNAMIC_UX_HEAVY, 1);
}
#endif
