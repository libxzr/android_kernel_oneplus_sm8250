/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#ifndef _OPLUS_CFS_COMMON_H_
#define _OPLUS_CFS_COMMON_H_

#define ux_err(fmt, ...) \
		printk_deferred(KERN_ERR "[UIFIRST_ERR][%s]"fmt, __func__, ##__VA_ARGS__)
#define ux_warn(fmt, ...) \
		printk_deferred(KERN_WARNING "[UIFIRST_WARN][%s]"fmt, __func__, ##__VA_ARGS__)
#define ux_debug(fmt, ...) \
		printk_deferred(KERN_INFO "[UIFIRST_INFO][%s]"fmt, __func__, ##__VA_ARGS__)

#define UX_MSG_LEN 64
#define UX_DEPTH_MAX 5

enum DYNAMIC_UX_TYPE
{
	DYNAMIC_UX_BINDER = 0,
	DYNAMIC_UX_RWSEM,
	DYNAMIC_UX_MUTEX,
	DYNAMIC_UX_SEM,
	DYNAMIC_UX_FUTEX,
	DYNAMIC_UX_HEAVY,
	DYNAMIC_UX_MAX,
};

#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
extern unsigned int walt_scale_demand_divisor;
#define scale_demand(d) ((d)/walt_scale_demand_divisor)
#endif

struct rq;
extern int ux_prefer_cpu[];

extern void ux_init_rq_data(struct rq *rq);
extern void ux_init_cpu_data(void);

extern void enqueue_ux_thread(struct rq *rq, struct task_struct *p);
extern void dequeue_ux_thread(struct rq *rq, struct task_struct *p);
extern void pick_ux_thread(struct rq *rq, struct task_struct **p, struct sched_entity **se);

extern void dynamic_ux_dequeue(struct task_struct *task, int type);
extern void dynamic_ux_dequeue_refs(struct task_struct *task, int type, int value);
extern void dynamic_ux_enqueue(struct task_struct *task, int type, int depth);
extern void dynamic_ux_inc(struct task_struct *task, int type);
extern void dynamic_ux_sub(struct task_struct *task, int type, int value);

extern bool test_task_ux(struct task_struct *task);
extern bool test_task_ux_depth(int ux_depth);
extern bool test_dynamic_ux(struct task_struct *task, int type);
extern bool test_set_dynamic_ux(struct task_struct *tsk);

extern bool test_ux_task_cpu(int cpu);
extern bool test_ux_prefer_cpu(struct task_struct *tsk, int cpu);
extern void find_ux_task_cpu(struct task_struct *tsk, int *target_cpu);
extern int set_ux_task_cpu_common_by_prio(struct task_struct *task, int *target_cpu, bool boost, bool prefer_idle);
extern int set_ux_task_cpu_common(struct task_struct *task, int prev_cpu, int *target_cpu);
static inline void find_slide_boost_task_cpu(struct task_struct *tsk, int *target_cpu) {}

static inline is_animator_ux_task(struct task_struct *task)
{
	return task->static_ux == 1;
}

static inline bool is_heavy_ux_task(struct task_struct *task)
{
	return task->static_ux == 2 || test_dynamic_ux(task, DYNAMIC_UX_HEAVY);
}

extern void place_entity_adjust_ux_task(struct cfs_rq *cfs_rq, struct sched_entity *se, int initial);
extern bool should_ux_task_skip_further_check(struct sched_entity *se);
extern bool should_ux_preempt_wakeup(struct task_struct *wake_task, struct task_struct *curr_task);
extern bool ux_skip_sync_wakeup(struct task_struct *task, int *sync);
extern void select_cpu_dur_anim(struct task_struct *task, int *target_cpu);
static inline bool is_anim_related(struct task_struct *task)
{
	return task->static_ux == 1 || test_dynamic_ux(task, DYNAMIC_UX_BINDER);
}
#endif
