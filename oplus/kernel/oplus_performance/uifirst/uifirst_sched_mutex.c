// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#include <linux/version.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include "uifirst_sched_common.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define MUTEX_FLAGS		0x07
static inline struct task_struct *__mutex_owner(struct mutex *lock)
{
	return (struct task_struct *)(atomic_long_read(&lock->owner) & ~MUTEX_FLAGS);
}
#endif

static void mutex_list_add_ux(struct list_head *entry, struct list_head *head)
{
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct mutex_waiter *waiter = NULL;
	list_for_each_safe(pos, n, head) {
		waiter = list_entry(pos, struct mutex_waiter, list);
		if (!test_task_ux(waiter->task)) {
			list_add(entry, waiter->list.prev);
			return;
		}
	}
	if (pos == head) {
		list_add_tail(entry, head);
	}
}

void mutex_list_add(struct task_struct *task, struct list_head *entry, struct list_head *head, struct mutex *lock)
{
	bool is_ux = test_task_ux(task);
	if (!entry || !head || !lock) {
		return;
	}
	if (is_ux && !lock->ux_dep_task) {
		mutex_list_add_ux(entry, head);
	} else {
		list_add_tail(entry, head);
	}
}

void mutex_set_inherit_ux(struct mutex *lock, struct task_struct *task)
{
	bool is_ux = false;
	struct task_struct *owner = NULL;
	if (!lock) {
		return;
	}
	is_ux = test_set_dynamic_ux(task);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	owner = __mutex_owner(lock);
#else
	owner = lock->owner;
#endif
	if (is_ux && !lock->ux_dep_task && owner && !test_task_ux(owner)) {
		dynamic_ux_enqueue(owner, DYNAMIC_UX_MUTEX, task->ux_depth);
		lock->ux_dep_task = owner;
	}
}

void mutex_unset_inherit_ux(struct mutex *lock, struct task_struct *task)
{
	if (lock && lock->ux_dep_task == task) {
		dynamic_ux_dequeue(task, DYNAMIC_UX_MUTEX);
		lock->ux_dep_task = NULL;
	}
}
