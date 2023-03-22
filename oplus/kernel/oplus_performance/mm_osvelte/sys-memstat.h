/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef _OSVELTE_SYS_MEMSTAT_H
#define _OSVELTE_SYS_MEMSTAT_H

#include <linux/vmstat.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/healthinfo/ion.h>

enum mtrack_type {
	MTRACK_DMABUF,
	MTRACK_GPU,
	MTRACK_HYBRIDSWAP,
	MTRACK_MAX
};

enum mtrack_subtype {
	MTRACK_DMABUF_SYSTEM_HEAP,
	MTRACK_DMABUF_POOL,
	MTRACK_GPU_TOTAL,
	MTRACK_HYBRIDSWAP_TOTAL,
	MTRACK_GPU_PROC_KERNEL,
	MTRACK_SUBTYPE_MAX
};

struct mtrack_debugger {
	long (*mem_usage)(enum mtrack_subtype type);
	long (*pid_mem_usage)(enum mtrack_subtype type, pid_t pid);
	void (*dump_usage_stat)(bool verbose);
};

static inline unsigned long sys_totalram(void)
{
	return totalram_pages();
}

static inline unsigned long sys_freeram(void)
{
	return global_zone_page_state(NR_FREE_PAGES);
}

static inline unsigned long sys_inactive_file(void)
{
	return global_node_page_state(NR_ACTIVE_FILE);
}

static inline unsigned long sys_active_file(void)
{
	return global_node_page_state(NR_INACTIVE_FILE);
}

static inline unsigned long sys_file(void)
{
	return global_node_page_state(NR_FILE_PAGES);
}

static inline unsigned long sys_slab_reclaimable(void)
{
	return global_node_page_state(NR_SLAB_RECLAIMABLE);
}

static inline unsigned long sys_slab_unreclaimable(void)
{
	return global_node_page_state(NR_SLAB_UNRECLAIMABLE);
}

static inline unsigned long sys_vmalloc(void)
{
	return vmalloc_nr_pages();
}

static inline unsigned long sys_inactive_anon(void)
{
	return global_node_page_state(NR_INACTIVE_ANON);
}

static inline unsigned long sys_active_anon(void)
{
	return global_node_page_state(NR_ACTIVE_ANON);
}

static inline unsigned long sys_anon(void)
{
	return global_node_page_state(NR_ANON_MAPPED);
}

static inline unsigned long sys_page_tables(void)
{
	return global_zone_page_state(NR_PAGETABLE);
}

static inline unsigned long sys_kernel_stack(void)
{
	return global_zone_page_state(NR_KERNEL_STACK_KB) >> (PAGE_SHIFT - 10);
}

static inline unsigned long sys_kernel_misc_reclaimable(void)
{
	return  global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE);
}

static inline unsigned long sys_sharedram(void)
{
	return global_node_page_state(NR_SHMEM);
}

int register_mtrack_debugger(enum mtrack_type type,
			     struct mtrack_debugger *debugger);
void unregister_mtrack_debugger(enum mtrack_type type,
				struct mtrack_debugger *debugger);
int register_mtrack_procfs(enum mtrack_type t, const char *name, umode_t mode,
			   const struct file_operations *proc_fops, void *data);
void unregister_mtrack_procfs(enum mtrack_type t, const char *name);

inline long read_mtrack_mem_usage(enum mtrack_type t, enum mtrack_subtype s);
inline long read_pid_mtrack_mem_usage(enum mtrack_type t, enum mtrack_subtype s,
				      pid_t pid);
inline void dump_mtrack_usage_stat(enum mtrack_type t, bool verbose);

int sys_memstat_init(void);
int sys_memstat_exit(void);

#endif /* _OSVELTE_SYS_MEMSTAT_H */
