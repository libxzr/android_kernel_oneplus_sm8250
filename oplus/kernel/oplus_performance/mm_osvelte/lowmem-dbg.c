// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/ksm.h>
#include <linux/version.h>
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/proc_fs.h>
#include <linux/vmstat.h>
#include <linux/vmalloc.h>

#include "../../../mm/slab.h"

#include "common.h"
#include "memstat.h"
#include "sys-memstat.h"
#include "lowmem-dbg.h"

static void lowmem_dbg_dump(struct work_struct *work);

static DEFINE_MUTEX(dump_mutex);
static DECLARE_WORK(lowmem_dbg_work, lowmem_dbg_dump);

struct files_acct {
	bool verbose;
	size_t sz;
};

struct lowmem_dbg_cfg {
	u64 interval;
	u64 last_jiffies;
	u64 watermark_low;
	u64 watermark_anon;
	u64 watermark_slab;
	u64 watermark_shmem;
	u64 watermark_dmabuf;
	u64 watermark_gpu;
};

static struct lowmem_dbg_cfg g_cfg;

struct task_struct *find_lock_task_mm_dup(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();

	for_each_thread(p, t) {
		task_lock(t);
		if (likely(t->mm))
			goto found;
		task_unlock(t);
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

static int dump_procs(bool verbose)
{
	struct task_struct *p;
	struct task_struct *tsk;
	char task_state = ' ';
	char frozen_mark = ' ';
	unsigned long tsk_nr_ptes = 0;
	pid_t ppid = 0;

	osvelte_info("======= %s\n", __func__);
	osvelte_info("comm             32   uid s f   pid  ppid   oom       vss    anon    file   shmem    swap\n");
	rcu_read_lock();
	for_each_process(p) {
		tsk = find_lock_task_mm_dup(p);
		if (!tsk)
			continue;

		tsk_nr_ptes = mm_pgtables_bytes(tsk->mm);
		task_state = task_state_to_char(tsk);
		/* check whether we have freezed a task. */
		frozen_mark = frozen(tsk) ? '*' : ' ';
		ppid = task_pid_nr(rcu_dereference(tsk->real_parent));

		osvelte_info("%-16s %2d %5d %c %c %5d %5d %5d %9lu %7lu %7lu %7lu %7lu\n",
			     tsk->comm, test_ti_thread_flag(task_thread_info(tsk), TIF_32BIT) != 0,
			     from_kuid(&init_user_ns, task_uid(tsk)),
			     task_state, frozen_mark,
			     tsk->pid, ppid, tsk->signal->oom_score_adj,
			     tsk->mm->total_vm,
			     get_mm_counter(tsk->mm, MM_ANONPAGES),
			     get_mm_counter(tsk->mm, MM_FILEPAGES),
			     get_mm_counter(tsk->mm, MM_SHMEMPAGES),
			     get_mm_counter(tsk->mm, MM_SWAPENTS));
		task_unlock(tsk);
	}
	rcu_read_unlock();

	return 0;
}

static int match_dmabuf_file(const void *p, struct file *file, unsigned fd)
{
	struct dma_buf *dmabuf;
	struct files_acct *acct = (struct files_acct *)p;

	if (!file)
		return 0;

	if (!is_dma_buf_file(file))
		return 0;

	dmabuf = file->private_data;
	if (!dmabuf->size)
		return 0;

	if (acct->verbose)
		osvelte_info("inode: %ld sz: %zu\n",
			     file_inode(dmabuf->file)->i_ino, dmabuf->size / SZ_1K);

	acct->sz += dmabuf->size;
	return 0;
}

int dump_procs_dmabuf_info(bool verbose)
{
	struct task_struct *tsk = NULL;

	osvelte_info("======= %s\n", __func__);
	osvelte_info("%-16s %-5s size\n", "comm", "pid");

	rcu_read_lock();
	for_each_process(tsk) {
		struct files_acct acct = {
			.verbose = verbose,
			.sz = 0,
		};

		if (tsk->flags & PF_KTHREAD)
			continue;

		task_lock(tsk);
		iterate_fd(tsk->files, 0, match_dmabuf_file, (void *)&acct);
		task_unlock(tsk);

		if (acct.sz)
			osvelte_info("%-16s %5d %zu\n", tsk->comm, tsk->pid, acct.sz / SZ_1K);
	}
	rcu_read_unlock();
	return 0;
}

#if defined(CONFIG_SLUB_DEBUG) || defined(CONFIG_SLUB_STAT_DEBUG) || defined(CONFIG_SLABINFO)
static int dump_slab_info(bool verbose)
{
	osvelte_info("======= %s\n", __func__);

	if (likely(!verbose)) {
		unsigned long slab_pages = 0;
		struct kmem_cache *cachep = NULL;
		struct kmem_cache *max_cachep = NULL;
		struct kmem_cache *prev_max_cachep = NULL;

		mutex_lock(&slab_mutex);
		list_for_each_entry(cachep, &slab_caches, list) {
			struct slabinfo sinfo;
			unsigned long scratch;

			memset(&sinfo, 0, sizeof(sinfo));
			get_slabinfo(cachep, &sinfo);
			scratch = sinfo.num_slabs << sinfo.cache_order;

			if (slab_pages < scratch) {
				slab_pages = scratch;
				prev_max_cachep = max_cachep;
				max_cachep = cachep;
			}
		}

		if (max_cachep || prev_max_cachep)
			osvelte_info("name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> :"
				     " tunables <limit> <batchcount> <sharedfactor> : slabdata <active_slabs> <num_slabs> <sharedavail>\n");

		if (max_cachep) {
			struct slabinfo sinfo;

			memset(&sinfo, 0, sizeof(sinfo));
			get_slabinfo(max_cachep, &sinfo);

			osvelte_info("%-17s %6lu %6lu %6u %4u %4d : tunables %4u %4u %4u : slabdata %6lu %6lu %6lu\n",
				     max_cachep->name, sinfo.active_objs,
				     sinfo.num_objs, max_cachep->size,
				     sinfo.objects_per_slab,
				     (1 << sinfo.cache_order),
				     sinfo.limit, sinfo.batchcount, sinfo.shared,
				     sinfo.active_slabs, sinfo.num_slabs,
				     sinfo.shared_avail);
		}

		if (prev_max_cachep) {
			struct slabinfo sinfo;

			memset(&sinfo, 0, sizeof(sinfo));
			get_slabinfo(prev_max_cachep, &sinfo);

			osvelte_info("%-17s %6lu %6lu %6u %4u %4d : tunables %4u %4u %4u : slabdata %6lu %6lu %6lu\n",
				     prev_max_cachep->name, sinfo.active_objs,
				     sinfo.num_objs, prev_max_cachep->size,
				     sinfo.objects_per_slab,
				     (1 << sinfo.cache_order),
				     sinfo.limit, sinfo.batchcount, sinfo.shared,
				     sinfo.active_slabs, sinfo.num_slabs,
				     sinfo.shared_avail);
		}
		mutex_unlock(&slab_mutex);

	} else {
		struct kmem_cache *cachep = NULL;

		osvelte_info("# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> :"
			     " tunables <limit> <batchcount> <sharedfactor> : slabdata <active_slabs> <num_slabs> <sharedavail>\n");

		mutex_lock(&slab_mutex);
		list_for_each_entry(cachep, &slab_caches, list) {
			struct slabinfo sinfo;

			memset(&sinfo, 0, sizeof(sinfo));
			get_slabinfo(cachep, &sinfo);

			osvelte_info("%-17s %6lu %6lu %6u %4u %4d : tunables %4u %4u %4u : slabdata %6lu %6lu %6lu\n",
				     cachep->name, sinfo.active_objs,
				     sinfo.num_objs, cachep->size,
				     sinfo.objects_per_slab,
				     (1 << sinfo.cache_order),
				     sinfo.limit, sinfo.batchcount, sinfo.shared,
				     sinfo.active_slabs, sinfo.num_slabs,
				     sinfo.shared_avail);
		}
		mutex_unlock(&slab_mutex);
	}
	return 0;
}
#else /* CONFIG_SLUB_DEBUG || CONFIG_SLUB_STAT_DEBUG || CONFIG_SLABINFO */
static inline int dump_slab_info(bool verbose)
{
	return 0;
}
#endif /* CONFIG_SLUB_DEBUG || CONFIG_SLUB_STAT_DEBUG || CONFIG_SLABINFO */

static void dump_each_zone(void)
{
	unsigned long free_pcp = 0;
	int cpu;
	struct zone *zone;

	for_each_populated_zone(zone) {
		for_each_online_cpu(cpu)
			free_pcp += per_cpu_ptr(zone->pageset, cpu)->pcp.count;
	}

	osvelte_info("active_anon:%lu inactive_anon:%lu isolated_anon:%lu\n"
		     " active_file:%lu inactive_file:%lu isolated_file:%lu\n"
		     " unevictable:%lu dirty:%lu writeback:%lu unstable:%lu\n"
		     " slab_reclaimable:%lu slab_unreclaimable:%lu\n"
		     " mapped:%lu shmem:%lu pagetables:%lu bounce:%lu\n"
		     " free:%lu free_pcp:%lu free_cma:%lu\n",
		     global_node_page_state(NR_ACTIVE_ANON),
		     global_node_page_state(NR_INACTIVE_ANON),
		     global_node_page_state(NR_ISOLATED_ANON),
		     global_node_page_state(NR_ACTIVE_FILE),
		     global_node_page_state(NR_INACTIVE_FILE),
		     global_node_page_state(NR_ISOLATED_FILE),
		     global_node_page_state(NR_UNEVICTABLE),
		     global_node_page_state(NR_FILE_DIRTY),
		     global_node_page_state(NR_WRITEBACK),
		     global_node_page_state(NR_UNSTABLE_NFS),
		     global_node_page_state(NR_SLAB_RECLAIMABLE),
		     global_node_page_state(NR_SLAB_UNRECLAIMABLE),
		     global_node_page_state(NR_FILE_MAPPED),
		     global_node_page_state(NR_SHMEM),
		     global_zone_page_state(NR_PAGETABLE),
		     global_zone_page_state(NR_BOUNCE),
		     global_zone_page_state(NR_FREE_PAGES),
		     free_pcp,
		     global_zone_page_state(NR_FREE_CMA_PAGES));

	for_each_populated_zone(zone) {
		free_pcp = 0;
		for_each_online_cpu(cpu)
			free_pcp += per_cpu_ptr(zone->pageset, cpu)->pcp.count;

		osvelte_info("%s"
			     " free:%lukB"
			     " min:%lukB"
			     " low:%lukB"
			     " high:%lukB"
			     " active_anon:%lukB"
			     " inactive_anon:%lukB"
			     " active_file:%lukB"
			     " inactive_file:%lukB"
			     " unevictable:%lukB"
			     " writepending:%lukB"
			     " present:%lukB"
			     " managed:%lukB"
			     " mlocked:%lukB"
			     " pagetables:%lukB"
			     " bounce:%lukB"
			     " free_pcp:%lukB"
			     " local_pcp:%ukB"
			     " free_cma:%lukB"
			     "\n",
			     zone->name, K(zone_page_state(zone, NR_FREE_PAGES)),
			     K(min_wmark_pages(zone)),
			     K(low_wmark_pages(zone)),
			     K(high_wmark_pages(zone)),
			     K(zone_page_state(zone, NR_ZONE_ACTIVE_ANON)),
			     K(zone_page_state(zone, NR_ZONE_INACTIVE_ANON)),
			     K(zone_page_state(zone, NR_ZONE_ACTIVE_FILE)),
			     K(zone_page_state(zone, NR_ZONE_INACTIVE_FILE)),
			     K(zone_page_state(zone, NR_ZONE_UNEVICTABLE)),
			     K(zone_page_state(zone, NR_ZONE_WRITE_PENDING)),
			     K(zone->present_pages),
			     K(zone_managed_pages(zone)),
			     K(zone_page_state(zone, NR_MLOCK)),
			     K(zone_page_state(zone, NR_PAGETABLE)),
			     K(zone_page_state(zone, NR_BOUNCE)),
			     K(free_pcp),
			     K(this_cpu_read(zone->pageset->pcp.count)),
			     K(zone_page_state(zone, NR_FREE_CMA_PAGES)));
	}
}


static void __lowmem_dbg_dump(struct lowmem_dbg_cfg *cfg)
{
	unsigned long tot, free, available;
	unsigned long slab_reclaimable, slab_unreclaimable;
	unsigned long anon, active_anon, inactive_anon;
	unsigned long file, active_file, inactive_file, shmem;
	unsigned long vmalloc, pgtbl, kernel_stack, kernel_misc_reclaimable;
	unsigned long dmabuf, dmabuf_pool, gpu, unaccounted;
	int i;
	struct sysinfo si;

	mutex_lock(&dump_mutex);
	tot = sys_totalram();
	free = sys_freeram();
	available = si_mem_available();

	slab_reclaimable = sys_slab_reclaimable();
	slab_unreclaimable = sys_slab_unreclaimable();

	si_swapinfo(&si);

	anon = sys_anon();
	active_anon = sys_active_anon();
	inactive_anon = sys_inactive_anon();
	shmem = sys_sharedram();

	file = sys_file();
	active_file = sys_active_file();
	inactive_file = sys_inactive_file();

	vmalloc = sys_vmalloc();
	pgtbl = sys_page_tables();
	kernel_stack = sys_kernel_stack();
	kernel_misc_reclaimable = sys_kernel_misc_reclaimable();

	dmabuf = read_mtrack_mem_usage(MTRACK_DMABUF, MTRACK_DMABUF_SYSTEM_HEAP);
	dmabuf_pool = read_mtrack_mem_usage(MTRACK_DMABUF, MTRACK_DMABUF_POOL);
	gpu = read_mtrack_mem_usage(MTRACK_GPU, MTRACK_GPU_TOTAL);
	unaccounted = tot - free - slab_reclaimable - slab_unreclaimable -
		vmalloc - anon - file - pgtbl - kernel_stack - dmabuf -
		gpu - kernel_misc_reclaimable;

	osvelte_info("lowmem_dbg start osvelte v%d.%d.%d <<<<<<<\n",
		     OSVELTE_MAJOR, OSVELTE_MINOR, OSVELTE_PATCH_NUM);
	dump_each_zone();

	osvelte_info("total: %lu free: %lu available: %lu\n",
		     K(tot), K(free), K(available));
	osvelte_info("swap_total: %lu swap_free: %lu\n",
		     K(si.totalswap), K(si.freeswap));
	osvelte_info("slab_reclaimable: %lu slab_unreclaimable: %lu\n",
		     K(slab_reclaimable), K(slab_unreclaimable));
	osvelte_info("anon: %lu active_anon: %lu inactive_anon: %lu\n",
		     K(anon), K(active_anon), K(inactive_anon));
	osvelte_info("file: %lu active_file: %lu inactive_file: %lu shmem: %lu\n",
		     K(file), K(active_file), K(inactive_file), K(shmem));
	osvelte_info("vmalloc: %lu page_tables: %lu kernel_stack: %lu kernel_misc_reclaimable: %lu\n",
		     K(vmalloc), K(pgtbl), K(kernel_stack), K(kernel_misc_reclaimable));
	osvelte_info("dmabuf: %lu dmabuf_pool: %lu gpu: %lu unaccounted: %lu\n",
		     K(dmabuf), K(dmabuf_pool), K(gpu), K(unaccounted));

	if (likely(cfg)) {
		dump_procs(anon + si.totalswap - si.freeswap > cfg->watermark_anon);
		dump_slab_info(slab_unreclaimable > cfg->watermark_slab);
		dump_procs_dmabuf_info(dmabuf > cfg->watermark_dmabuf);
	} else {
		dump_procs(true);
		dump_slab_info(true);
		dump_procs_dmabuf_info(false);
	}

	for (i = MTRACK_GPU; i < MTRACK_MAX; i++)
		dump_mtrack_usage_stat(i, false);

	osvelte_info("lowmem_dbg end >>>>>>>\n");
	mutex_unlock(&dump_mutex);
}

static void lowmem_dbg_dump(struct work_struct *work)
{
	__lowmem_dbg_dump(&g_cfg);
}

void oplus_memory_dump(struct work_struct *work)
{
	__lowmem_dbg_dump(NULL);
}
EXPORT_SYMBOL_GPL(oplus_memory_dump);

static unsigned long lowmem_dbg_count(struct shrinker *s,
				      struct shrink_control *sc)
{
	return sys_active_file() + sys_inactive_file() +
		sys_active_anon() + sys_inactive_anon();
}

static unsigned long lowmem_dbg_scan(struct shrinker *s, struct shrink_control *sc)
{
	static atomic_t atomic_lmk = ATOMIC_INIT(0);
	struct lowmem_dbg_cfg *cfg = &g_cfg;
	long available;
	u64 now;

	if (atomic_inc_return(&atomic_lmk) > 1) {
		atomic_dec(&atomic_lmk);
		return 0;
	}

	now = get_jiffies_64();
	if (time_before64(now, (cfg->last_jiffies + cfg->interval)))
		goto done;
	cfg->last_jiffies = now;

	available = si_mem_available();
	if (available > cfg->watermark_low)
		goto done;

	schedule_work(&lowmem_dbg_work);

done:
	atomic_dec(&atomic_lmk);

	return 0;
}

static struct shrinker lowmem_dbg_shrinker = {
	.scan_objects = lowmem_dbg_scan,
	.count_objects = lowmem_dbg_count,
	.seeks = DEFAULT_SEEKS
};

int osvelte_lowmem_dbg_init(void)
{
	int ret;
	struct lowmem_dbg_cfg *cfg = &g_cfg;
	unsigned long total_ram = sys_totalram();

	ret = register_shrinker(&lowmem_dbg_shrinker);
	if (ret)
		return -ENOMEM;

	cfg->interval = 10 * HZ;

	if (total_ram >= PAGES(SZ_2G + SZ_2G))
		cfg->watermark_low = PAGES(SZ_1G);
	else if (total_ram >= PAGES(SZ_2G + SZ_1G))
		cfg->watermark_low = PAGES(SZ_512M);
	else
		cfg->watermark_low = PAGES(SZ_256M);
	cfg->watermark_anon = total_ram / 2;
	cfg->watermark_slab = PAGES(SZ_1G);
	cfg->watermark_shmem = PAGES(SZ_1G);
	cfg->watermark_dmabuf = PAGES(SZ_2G + SZ_512M);
	cfg->watermark_gpu = PAGES(SZ_2G + SZ_512M);

	osvelte_info("%s interval: %lu watermark low: %lu anon: %lu ashmem: %lu dmabuf: %lu\n",
		     __func__, cfg->interval, cfg->watermark_low, cfg->watermark_anon,
		     cfg->watermark_shmem, cfg->watermark_dmabuf);
	return 0;
}

int osvelte_lowmem_dbg_exit(void)
{
	unregister_shrinker(&lowmem_dbg_shrinker);
	return 0;
}
