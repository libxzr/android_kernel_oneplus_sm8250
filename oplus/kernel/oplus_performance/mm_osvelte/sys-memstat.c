// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/proc_fs.h>
#include <linux/fdtable.h>
#include <linux/seq_file.h>
#include <linux/hashtable.h>
#include <linux/module.h>

#ifdef CONFIG_OPLUS_HEALTHINFO
#include <linux/healthinfo/ion.h>
#endif

#include "common.h"
#include "memstat.h"
#include "sys-memstat.h"

#define DEFINE_PROC_SHOW_ATTRIBUTE(__name)				\
static int __name ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __name ## _show, PDE_DATA(inode));	\
}									\
									\
static const struct file_operations __name ## _proc_ops = {		\
	.open		= __name ## _open,				\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
}

static struct proc_dir_entry *mtrack_procs[MTRACK_MAX];
static struct mtrack_debugger *mtrack_debugger[MTRACK_MAX];

const char * const mtrack_text[MTRACK_MAX] = {
	"dma_buf",
	"gpu",
	"hybridswap"
};

struct dma_info {
	struct dma_buf *dmabuf;
	struct hlist_node head;
};

struct dma_proc {
	char name[TASK_COMM_LEN];
	pid_t pid;
	size_t size;
	struct hlist_head dma_bufs[1 << 10];
	struct list_head head;
};

struct dma_buf_priv {
	int count;
	size_t size;
	struct seq_file *s;
};

static int info_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "osvelte version v%d.%d.%d based on kernel-5.10\n",
		   OSVELTE_MAJOR, OSVELTE_MINOR, OSVELTE_PATCH_NUM);
	return 0;
}
DEFINE_PROC_SHOW_ATTRIBUTE(info);

void unregister_mtrack_debugger(enum mtrack_type type,
				struct mtrack_debugger *debugger)
{
	mtrack_debugger[type] = NULL;
}
EXPORT_SYMBOL_GPL(unregister_mtrack_debugger);

int register_mtrack_debugger(enum mtrack_type type,
			     struct mtrack_debugger *debugger)
{
	if (!debugger)
		return -EINVAL;

	if (mtrack_debugger[type])
		return -EEXIST;

	mtrack_debugger[type] = debugger;
	return 0;
}
EXPORT_SYMBOL_GPL(register_mtrack_debugger);


int register_mtrack_procfs(enum mtrack_type t, const char *name, umode_t mode,
			   const struct file_operations *proc_fops, void *data)
{
	struct proc_dir_entry *entry;
	if (!mtrack_procs[t])
		return -EBUSY;

	entry = proc_create_data(name, mode, mtrack_procs[t], proc_fops, data);
	if (!entry)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(register_mtrack_procfs);

void unregister_mtrack_procfs(enum mtrack_type t, const char *name)
{
	if (!unlikely(mtrack_procs[t]))
		return;

	remove_proc_subtree(name, mtrack_procs[t]);
}
EXPORT_SYMBOL_GPL(unregister_mtrack_procfs);

inline long read_mtrack_mem_usage(enum mtrack_type t, enum mtrack_subtype s)
{
	struct mtrack_debugger *d = mtrack_debugger[t];

	if (d && d->mem_usage)
		return d->mem_usage(s);
	return 0;
}

inline long read_pid_mtrack_mem_usage(enum mtrack_type t,
				      enum mtrack_subtype s, pid_t pid)
{
	struct mtrack_debugger *d = mtrack_debugger[t];

	if (d && d->pid_mem_usage)
		return d->pid_mem_usage(s, pid);
	return 0;
}

inline void dump_mtrack_usage_stat(enum mtrack_type t, bool verbose)
{
	struct mtrack_debugger *d = mtrack_debugger[t];

	if (d && d->dump_usage_stat) {
		osvelte_info("======= dump_%s\n", mtrack_text[t]);
		return d->dump_usage_stat(verbose);
	}
}

#ifdef CONFIG_OPLUS_HEALTHINFO
long read_dmabuf_mem_usage(enum mtrack_subtype type)
{
	if (type == MTRACK_DMABUF_SYSTEM_HEAP)
		return ion_total() >> PAGE_SHIFT;
	else if (type == MTRACK_DMABUF_POOL)
		return global_zone_page_state(NR_IONCACHE_PAGES);

	return 0;
}

static struct mtrack_debugger dmabuf_mtrack_debugger = {
	.mem_usage = read_dmabuf_mem_usage,
};
#endif /* CONFIG_OPLUS_HEALTHINFO */

int sys_memstat_init(void)
{
	struct proc_dir_entry *root;
	int i;

	root = proc_mkdir("osvelte", NULL);
	if (!root) {
		pr_err("create osvelte dir failed\n");
		return -ENOMEM;
	}
	proc_create("info", 0444, root, &info_proc_ops);

	/* create mtrack dir here */
	for (i = 0; i < MTRACK_MAX; i++) {
		mtrack_procs[i] = proc_mkdir(mtrack_text[i], root);
		if (!mtrack_procs[i]) {
			osvelte_err("proc_fs: create %s failed\n",
				    mtrack_text[i]);
		}
	}
#ifdef CONFIG_OPLUS_HEALTHINFO
	register_mtrack_debugger(MTRACK_DMABUF, &dmabuf_mtrack_debugger);
#endif /* CONFIG_OPLUS_HEALTHINFO */

	return 0;
}

int sys_memstat_exit(void)
{
	remove_proc_subtree("osvelte", NULL);
#ifdef CONFIG_OPLUS_HEALTHINFO
	unregister_mtrack_debugger(MTRACK_DMABUF, &dmabuf_mtrack_debugger);
#endif /* CONFIG_OPLUS_HEALTHINFO */
	return 0;
}
