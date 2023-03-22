/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef _OSVELTE_PROC_MEMSTAT_H
#define _OSVELTE_PROC_MEMSTAT_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/ioctls.h>
#else
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

#define PROC_MS_UID		0x1
#define PROC_MS_PID		0x2
#define PROC_MS_PPID		0x4
#define PROC_MS_OOM_SCORE_ADJ	0x8
#define PROC_MS_32BIT		0x10
#define PROC_MS_COMM		0x20

#define PROC_MS_VSS		0x40
#define PROC_MS_ANON		0x80
#define PROC_MS_FILE		0x100
#define PROC_MS_SHMEM		0x200
#define PROC_MS_SWAP		0x400

#define PROC_MS_NR_FDS		0x800
#define PROC_MS_ASHMEM		0x1000
#define PROC_MS_DMABUF		0x2000

#define PROC_MS_PSS		0x4000
#define PROC_MS_NATIVEHEAP	0x8000
#define PROC_MS_JAVAHEAP	0x10000

#define PROC_MS_GPU		0x20000

#define PROC_MS_INFO		 (PROC_MS_UID | PROC_MS_PID | PROC_MS_PPID | \
				  PROC_MS_OOM_SCORE_ADJ | PROC_MS_32BIT | \
				  PROC_MS_COMM)
#define PROC_MS_MM		 (PROC_MS_VSS | PROC_MS_ANON | PROC_MS_FILE | \
				  PROC_MS_SHMEM | PROC_MS_SWAP)
#define PROC_MS_COMMON		 (PROC_MS_INFO | PROC_MS_MM)

#define PROC_MS_ITERATE_FD	 (PROC_MS_NR_FDS | PROC_MS_ASHMEM | \
				  PROC_MS_DMABUF)

#define PROC_MS_ITERATE_VMA	 (PROC_MS_PSS | PROC_MS_NATIVEHEAP | \
				  PROC_MS_JAVAHEAP)
#define PROC_MS_ITERATE_VMA_NAME (PROC_MS_NATIVEHEAP | PROC_MS_JAVAHEAP)

#define PROC_MS_ITERATE_MTRACK	 (PROC_MS_GPU)

#define PROC_MS_VALID_FLAGS	 (PROC_MS_COMMON | PROC_MS_ITERATE_FD | \
				  PROC_MS_ITERATE_VMA | PROC_MS_ITERATE_MTRACK)

#define PROC_MS_MAX_SIZE 400

/* ioctl cmd */
#define __PROC_MSIO		0xFB

#define CMD_PROC_MS_PID		_IO(__PROC_MSIO, 1)
#define CMD_PROC_MS_SIZE	_IO(__PROC_MSIO, 2)
#define CMD_PROC_MS_SIZE_UID	_IO(__PROC_MSIO, 3)

#define CMD_PROC_MS_MIN		CMD_PROC_MS_PID
#define CMD_PROC_MS_MAX		CMD_PROC_MS_SIZE_UID

#define CMD_PROC_MS_INVALID	0xFFFFFFFF

#ifdef __KERNEL__
struct proc_ms {
	char comm[TASK_COMM_LEN];
	u8 is_32bit;
	short oom_score_adj;
	int nr_fds;
	uid_t uid;
	pid_t pid;
	pid_t ppid;

	u32 anon;
	u32 file;
	u32 shmem;
	u32 swap;
	u32 vss;

	/* read from vma */
	u32 rss;
	u32 swap_rss;
	u32 javaheap;
	u32 nativeheap;

	/*read from fdinfo */
	u32 ashmem;
	u32 dmabuf;
	u32 gpu;
};

struct proc_size_ms {
	u32 flags;
	uid_t uid;
	u32 size;
	struct proc_ms arr_ms[0];
};

struct proc_pid_ms {
	u32 flags;
	pid_t pid;
	struct proc_ms ms;
};

long proc_memstat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else /* __KERNEL__ */

#define TASK_COMM_LEN (16)
#define PAGE_SHIFT (12)

struct proc_ms {
	char comm[TASK_COMM_LEN];
	__u8 is_32bit;
	short oom_score_adj;
	int nr_fds;
	uid_t uid;
	pid_t pid;
	pid_t ppid;

	/* pages */
	__u32 anon;
	__u32 file;
	__u32 shmem;
	__u32 swap;
	__u32 vss;

	/* read from vma */
	__u32 pss;
	__u32 swap_rss;
	__u32 javaheap;
	__u32 nativeheap;

	/*re;ad from fdinfo */
	__u32 ashmem;
	__u32 dmabuf;
	__u32 gpu;
};

struct proc_size_ms {
	__u32 flags;
	uid_t uid;
	__u32 size;
	struct proc_ms arr_ms[0];
};

struct proc_pid_ms {
	__u32 flags;
	pid_t pid;
	struct proc_ms ms;
};
#endif /* __KERNEL__ */

#endif /* _OSVELTE_PROC_MEMSTAT_H  */
