// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) "osvelte: " fmt

#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/cred.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pagemap.h>
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/thread_info.h>

#include "common.h"
#include "memstat.h"
#include "proc-memstat.h"
#include "sys-memstat.h"
#include "logger.h"

static bool is_ashmem_file(struct file *file)
{
	return false;
}

static int match_file(const void *p, struct file *file, unsigned fd)
{
	struct dma_buf *dmabuf;
	struct ashmem_area *ashmem_data;
	struct proc_ms *ms = (struct proc_ms *)p;

	if (is_dma_buf_file(file)) {
		dmabuf = file->private_data;
		if (dmabuf)
			ms->dmabuf += dmabuf->size;
		return 0;
	}

#ifdef CONFIG_ASHMEM
	if (is_ashmem_file(file)) {
		ashmem_data = file->private_data;
		if (ashmem_data)
			ms->ashmem += ashmem_data->size;
		return 0;
	}
#endif /* CONFIG_ASHMEM */

	ms->nr_fds += 1;
	return 0;
}

/*
 * Must be called under rcu_read_lock() & increment task_struct counter.
 */
static int __proc_memstat(struct task_struct *p, struct proc_ms *ms, u32 flags)
{
	struct mm_struct *mm = NULL;
	struct task_struct *tsk;

	if (flags & PROC_MS_UID)
		ms->uid = from_kuid(&init_user_ns, task_uid(p));

	if (flags & PROC_MS_PID)
		ms->pid = p->pid;

	if (flags & PROC_MS_OOM_SCORE_ADJ)
		ms->oom_score_adj = p->signal->oom_score_adj;

	if (flags & PROC_MS_32BIT)
		ms->is_32bit = test_ti_thread_flag(task_thread_info(p),
						   TIF_32BIT);

	if (flags & PROC_MS_COMM)
		strncpy(ms->comm, p->comm, sizeof(ms->comm));

	tsk = find_lock_task_mm_dup(p);
	if (!tsk)
		return -EEXIST;

	if (flags & PROC_MS_ITERATE_FD) {
		iterate_fd(p->files, 0, match_file, ms);

		/* dma_buf size use byte */
		ms->dmabuf = ms->dmabuf >> PAGE_SHIFT;
		ms->ashmem = ms->ashmem >> PAGE_SHIFT;
	}

	if (flags & PROC_MS_ITERATE_MTRACK) {
		/* gpu in page_size, so it cannot overflow */
		ms->gpu = (u32)read_pid_mtrack_mem_usage(MTRACK_GPU,
							 MTRACK_GPU_PROC_KERNEL,
							 p->pid);
	}

	mm = tsk->mm;

	if (flags & PROC_MS_VSS)
		ms->vss = mm->total_vm;

	if (flags & PROC_MS_ANON)
		ms->anon = get_mm_counter(mm, MM_ANONPAGES);

	if (flags & PROC_MS_FILE)
		ms->file = get_mm_counter(mm, MM_FILEPAGES);

	if (flags & PROC_MS_SHMEM)
		ms->shmem = get_mm_counter(mm, MM_SHMEMPAGES);

	if (flags & PROC_MS_SWAP)
		ms->swap = get_mm_counter(mm, MM_SWAPENTS);

	task_unlock(tsk);
	return 0;
}

static int proc_pid_memstat(unsigned long arg)
{
	long ret = -EINVAL;
	struct proc_pid_ms ppm;
	struct task_struct *p;
	pid_t pid;
	void __user *argp = (void __user *) arg;

	if (copy_from_user(&ppm, argp, sizeof(ppm)))
		return -EFAULT;

	pid = ppm.pid;
	/* zeroed data */
	memset(&ppm.ms, 0, sizeof(ppm.ms));

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -EINVAL;
	}

	if ((ppm.flags & PROC_MS_PPID) && pid_alive(p))
		ppm.ms.ppid = task_pid_nr(rcu_dereference(p->real_parent));
	ret = __proc_memstat(p, &ppm.ms, ppm.flags);
	rcu_read_unlock();

	if (ret)
		return ret;

	if (copy_to_user(argp, &ppm, sizeof(ppm)))
		return -EFAULT;

	return 0;
}

static int proc_size_memstat(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_reader *reader = file->private_data;
	struct proc_size_ms psm;
	struct task_struct *p = NULL;
	int ret, cnt = 0;

	void __user *argp = (void __user *) arg;

	if (copy_from_user(&psm, argp, sizeof(psm)))
		return -EFAULT;

	if (psm.size > PROC_MS_MAX_SIZE)
		return -EINVAL;

	mutex_lock(&reader->mutex);
	if (unlikely(!reader->arr_ms)) {
		reader->arr_ms = vzalloc(PROC_MS_MAX_SIZE * sizeof(struct proc_ms));

		if (!reader->arr_ms) {
			mutex_unlock(&reader->mutex);
			return -ENOMEM;
		}
	}
	memset(reader->arr_ms, 0, PROC_MS_MAX_SIZE * sizeof(struct proc_ms));
	mutex_unlock(&reader->mutex);

	rcu_read_lock();
	for_each_process(p) {
		struct proc_ms *ms = reader->arr_ms + cnt;

		if (cnt >= psm.size)
			break;

		if (p->flags & PF_KTHREAD)
			continue;

		if (p->pid != p->tgid)
			continue;

		if (cmd == CMD_PROC_MS_SIZE_UID) {
			/* don't need fetch uid again */
			psm.flags &= ~PROC_MS_UID;
			ms->uid = from_kuid(&init_user_ns, task_uid(p));
			if (ms->uid != psm.uid)
				continue;
		}

		if ((psm.flags & PROC_MS_PPID) && pid_alive(p))
			ms->ppid = task_pid_nr(rcu_dereference(p->real_parent));

		ret = __proc_memstat(p, ms, psm.flags);
		if (likely(!ret))
			cnt++;
	}
	rcu_read_unlock();

	psm.size = cnt;
	if (copy_to_user(argp, &psm, sizeof(psm))) {
		ret = -EFAULT;
		goto err_buf;
	}

	/* if cnt is zero, copy nothin. */
	if (copy_to_user(argp + sizeof(psm), reader->arr_ms, cnt * sizeof(struct proc_ms))) {
		ret = -EFAULT;
		goto err_buf;
	}

err_buf:
	return ret;
}

long proc_memstat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;

	if (cmd < CMD_PROC_MS_MIN || cmd > CMD_PROC_MS_MAX) {
		osvelte_err("cmd invalid.\n");
		return CMD_PROC_MS_INVALID;
	}

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;

	switch (cmd) {
	case CMD_PROC_MS_PID:
		ret = proc_pid_memstat(arg);
		break;
	case CMD_PROC_MS_SIZE:
		ret = proc_size_memstat(file, cmd, arg);
		break;
	case CMD_PROC_MS_SIZE_UID:
		ret = proc_size_memstat(file, cmd, arg);
		break;
	}

	return ret;
}
