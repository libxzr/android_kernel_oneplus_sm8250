// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include "uifirst_sched_common.h"

#define CREATE_TRACE_POINTS
#include "uifirst_sched_trace.h"

struct task_struct* get_futex_owner_by_pid(u32 owner_tid)
{
	struct task_struct* futex_owner = NULL;

	if (owner_tid > 0 && owner_tid <= PID_MAX_DEFAULT) {
		rcu_read_lock();
		futex_owner = find_task_by_vpid(owner_tid);
		rcu_read_unlock();
		if (futex_owner == NULL) {
			ux_warn("failed to find task by pid(curr:%-12s pid:%d)\n", current->comm, owner_tid);
		}
	}

    return futex_owner;
}

struct task_struct *get_futex_owner(u32 owner_tid)
{
	struct task_struct *futex_owner = NULL;

	if (owner_tid > 0 && owner_tid <= PID_MAX_DEFAULT) {
		rcu_read_lock();
		futex_owner = find_task_by_vpid(owner_tid);
		rcu_read_unlock();
		if (futex_owner == NULL) {
			ux_warn("failed to find task by pid(curr:%-12s pid:%d)\n", current->comm, owner_tid);
		}
	}

	return futex_owner;
}

void futex_set_inherit_ux(struct task_struct *owner, struct task_struct *task)
{
	bool is_ux = false;
	is_ux = test_set_dynamic_ux(task);

	if (is_ux && owner && !test_task_ux(owner)) {
		dynamic_ux_enqueue(owner, DYNAMIC_UX_FUTEX, task->ux_depth);
	}
}

void futex_unset_inherit_ux(struct task_struct *task)
{
	if (test_dynamic_ux(task, DYNAMIC_UX_FUTEX)) {
		dynamic_ux_dequeue(task, DYNAMIC_UX_FUTEX);
	}
}

void futex_set_inherit_ux_refs(struct task_struct *owner, struct task_struct *task)
{
	bool is_ux = test_set_dynamic_ux(task);

	if (is_ux && owner) {
		bool is_owner_ux = test_task_ux(owner);
		if (!is_owner_ux){
			dynamic_ux_enqueue(owner, DYNAMIC_UX_FUTEX, task->ux_depth);
		} else {
			dynamic_ux_inc(owner, DYNAMIC_UX_FUTEX);
		}
	}

	if (owner)
		trace_oplus_tp_sched_change_ux(test_task_ux(owner) ? 3 : 0, owner->pid);
}

void futex_unset_inherit_ux_refs(struct task_struct *task, int value)
{
	if (test_dynamic_ux(task, DYNAMIC_UX_FUTEX)) {
		dynamic_ux_dequeue_refs(task, DYNAMIC_UX_FUTEX, value);
	}

	if (task)
		trace_oplus_tp_sched_change_ux(test_task_ux(task) ? 3 : 0, task->pid);
}
