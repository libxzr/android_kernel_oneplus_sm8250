// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/cred.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/security.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mm_types.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/userfaultfd_k.h>
#include <linux/hugetlb.h>
#include <linux/resmap_account.h>

#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <linux/file.h>
#include <linux/sched/signal.h>

#include "internal.h"

#define THRIDPART_APP_UID_LOW_LIMIT 10000UL

const char *resmap_item_name[] = {
	"resmap_action",
	"resmap_success",
	"resmap_fail",
	"resmap_texit",
};

int svm_oom_pid = -1;
unsigned long svm_oom_jiffies = 0;
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
unsigned long gpu_compat_high_limit_addr;
#endif
DEFINE_PER_CPU(struct resmap_event_state, resmap_event_states);

int rlimit_svm_log = 1;
module_param_named(rlimit_svm_log, rlimit_svm_log, int, 0644);

int reserved_area_enable = 1;
module_param_named(reserved_area_enable, reserved_area_enable, int, 0644);

int reserved_area_checking(struct mm_struct *mm, unsigned long *vm_flags, unsigned long flags, unsigned long addr, unsigned long len)
{
	if (check_reserve_mmap_doing(mm)) {
		*vm_flags |= (VM_BACKUP_CREATE|VM_DONTEXPAND|VM_SOFTDIRTY);
	} else if (is_backed_addr(mm, addr, addr+len)) {
		*vm_flags |= VM_BACKUP_ALLOC;
	} else if (!check_general_addr(mm, addr, addr+len)) {
		pr_err("%s mmap backed base:%#lx addr:%#lx flags %lx len:%#lx is invalid.\n",
				current->comm,	mm->reserve_vma->vm_start,
				addr, flags,
				mm->reserve_vma->vm_end - mm->reserve_vma->vm_start);
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(reserved_area_checking);

void init_reserve_mm(struct mm_struct* mm)
{
	mm->reserve_vma = NULL;
	mm->reserve_mmap = NULL;
	mm->reserve_mm_rb = RB_ROOT;
	mm->reserve_map_count = 0;
	mm->do_reserve_mmap = 0;
	mm->vm_search_two_way = false;

}
EXPORT_SYMBOL(init_reserve_mm);

static struct vm_area_struct *remove_vma(struct vm_area_struct *vma)
{
	struct vm_area_struct *next = vma->vm_next;

	might_sleep();
	if (vma->vm_ops && vma->vm_ops->close)
		vma->vm_ops->close(vma);
	put_vma(vma);
	return next;
}

#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
#define VM_RESERVED_SIZE SZ_512M
#else
#define VM_RESERVED_SIZE SZ_128M
#endif
static inline unsigned long vm_mmap_pgoff_with_check(struct file *file,
		unsigned long addr, unsigned long len, unsigned long prot,
		unsigned long flags, unsigned long pgoff)
{
	struct task_struct *task = current;
	struct mm_struct *mm = task->mm;
	unsigned long retval;

	if ((flags & MAP_BACKUP_CREATE) && (addr == RESERVE_VMAP_ADDR) &&
			!mm->reserve_vma && (mm->do_reserve_mmap == 0)) {
		if (!test_thread_flag(TIF_32BIT) ||
				!reserved_area_enable ||
#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
				!gpu_compat_high_limit_addr ||
#endif
				(flags & MAP_FIXED)) {
			return -EINVAL;
		}

		addr = 0;
		reserve_mmap_doing(mm);
		if (PAGE_ALIGN(len) > RESERVE_VMAP_AREA_SIZE)
			len = RESERVE_VMAP_AREA_SIZE;

		retval = vm_mmap_pgoff(file, addr, len, prot, flags, pgoff);
		reserve_mmap_done(mm);
	} else
		retval = vm_mmap_pgoff(file, addr, len, prot, flags, pgoff);

	return retval;
}
EXPORT_SYMBOL(vm_mmap_pgoff_with_check);

void exit_reserved_mmap(struct mm_struct *mm)
{
	struct mmu_gather tlb;
	struct vm_area_struct *vma;
	unsigned long nr_accounted = 0;
	unsigned long start, end;

	if (!mm->reserve_vma || !mm->reserve_mmap)
		return;

	vma = mm->reserve_mmap;
	start = mm->reserve_vma->vm_start;
	end = mm->reserve_vma->vm_end;
	lru_add_drain();
	flush_cache_mm(mm);
	tlb_gather_mmu(&tlb, mm, start, end);
	unmap_vmas(&tlb, vma, start, end);
	free_pgtables(&tlb, vma, start, end);
	tlb_finish_mmu(&tlb, start, end);

	while (vma) {
		if (vma->vm_flags & VM_ACCOUNT)
			nr_accounted += vma_pages(vma);
		vma = remove_vma(vma);
	}
	vm_unacct_memory(nr_accounted);

	mm->reserve_vma = NULL;
	mm->reserve_mm_rb = RB_ROOT;
}
EXPORT_SYMBOL(exit_reserved_mmap);

int dup_reserved_mmap(struct mm_struct *mm, struct mm_struct *oldmm,
	struct kmem_cache *vm_area_cachep)
{
	struct vm_area_struct *mpnt, *tmp, *prev, **pprev;
	struct rb_node **rb_link, *rb_parent;
	int retval = 0;
	unsigned long charge;
	LIST_HEAD(uf);

	if (!mm->reserve_vma)
		return 0;

	rb_link = &mm->reserve_mm_rb.rb_node;
	rb_parent = NULL;
	pprev = &mm->reserve_mmap;

	prev = NULL;
	for (mpnt = oldmm->reserve_mmap; mpnt; mpnt = mpnt->vm_next) {
		struct file *file;

		if (mpnt->vm_flags & VM_DONTCOPY) {
			vm_stat_account(mm, mpnt->vm_flags, -vma_pages(mpnt));
			continue;
		}
		charge = 0;
		if (mpnt->vm_flags & VM_ACCOUNT) {
			unsigned long len = vma_pages(mpnt);

			if (security_vm_enough_memory_mm(oldmm, len)) /* sic */
				goto fail_nomem;
			charge = len;
		}
		tmp = kmem_cache_alloc(vm_area_cachep, GFP_KERNEL);
		if (!tmp)
			goto fail_nomem;
		*tmp = *mpnt;
		INIT_VMA(tmp);
		retval = vma_dup_policy(mpnt, tmp);
		if (retval)
			goto fail_nomem_policy;
		tmp->vm_mm = mm;
		retval = dup_userfaultfd(tmp, &uf);
		if (retval)
			goto fail_nomem_anon_vma_fork;
		if (tmp->vm_flags & VM_WIPEONFORK) {
			/* VM_WIPEONFORK gets a clean slate in the child. */
			tmp->anon_vma = NULL;
			if (anon_vma_prepare(tmp))
				goto fail_nomem_anon_vma_fork;
		} else if (anon_vma_fork(tmp, mpnt))
			goto fail_nomem_anon_vma_fork;
		tmp->vm_flags &= ~(VM_LOCKED | VM_LOCKONFAULT);
		tmp->vm_next = tmp->vm_prev = NULL;
		file = tmp->vm_file;
		if (file) {
			struct inode *inode = file_inode(file);
			struct address_space *mapping = file->f_mapping;

			get_file(file);
			if (tmp->vm_flags & VM_DENYWRITE)
				atomic_dec(&inode->i_writecount);
			i_mmap_lock_write(mapping);
			if (tmp->vm_flags & VM_SHARED)
				atomic_inc(&mapping->i_mmap_writable);
			flush_dcache_mmap_lock(mapping);
			/* insert tmp into the share list, just after mpnt */
			vma_interval_tree_insert_after(tmp, mpnt,
					&mapping->i_mmap);
			flush_dcache_mmap_unlock(mapping);
			i_mmap_unlock_write(mapping);
		}

		/*
		 * Clear hugetlb-related page reserves for children. This only
		 * affects MAP_PRIVATE mappings. Faults generated by the child
		 * are not guaranteed to succeed, even if read-only
		 */
		if (is_vm_hugetlb_page(tmp))
			reset_vma_resv_huge_pages(tmp);

		/*
		 * Link in the new vma and copy the page table entries.
		 */
		*pprev = tmp;
		pprev = &tmp->vm_next;
		tmp->vm_prev = prev;
		prev = tmp;

		__vma_link_rb(mm, tmp, rb_link, rb_parent);
		rb_link = &tmp->vm_rb.rb_right;
		rb_parent = &tmp->vm_rb;

		mm->reserve_map_count++;
		if (!(tmp->vm_flags & VM_WIPEONFORK))
			retval = copy_page_range(mm, oldmm, mpnt);

		if (tmp->vm_ops && tmp->vm_ops->open)
			tmp->vm_ops->open(tmp);

		if (retval)
			goto out;
	}
out:
	return retval;
fail_nomem_anon_vma_fork:
	mpol_put(vma_policy(tmp));
fail_nomem_policy:
	kmem_cache_free(vm_area_cachep, tmp);
fail_nomem:
	retval = -ENOMEM;
	vm_unacct_memory(charge);
	goto out;
}
EXPORT_SYMBOL(dup_reserved_mmap);

static inline bool check_parent_is_zygote(struct task_struct *tsk)
{
	struct task_struct *t;
	bool ret = false;

	rcu_read_lock();
	t = rcu_dereference(tsk->real_parent);
	if (t) {
		const struct cred *tcred = __task_cred(t);

		if (!strcmp(t->comm, "main") && (tcred->uid.val == 0) &&
				(t->parent != NULL) && !strcmp(t->parent->comm,"init"))
			ret = true;
	}
	rcu_read_unlock();
	return ret;
}

#ifdef CONFIG_OPLUS_HEALTHINFO
void trigger_stack_limit_changed(long value)
{
	char msg[128] = {0};
	snprintf(msg, 127, "{\"version\":1, \"pid\":%d, \"size\":%ld}",
			current->tgid, value);
	ohm_action_trig_with_msg(OHM_RLIMIT_MON, msg);
}

void trigger_svm_oom_event(struct mm_struct *mm, bool brk_risk, bool is_locked)
{
	int len = 0;
	int oom = 0;
	int res = 0;
	int over_time = 0;
	int change_stack = 0;
	struct rlimit *rlim;
	unsigned long long current_time_ns;
	char *svm_oom_msg = NULL;
	unsigned int uid = (unsigned int)(current_uid().val);
	unsigned long backed_vm_size = 0;

	if (!(rlimit_svm_log && (current->pid == current->tgid) &&
				is_compat_task() &&
				check_parent_is_zygote(current) &&
				(uid >= THRIDPART_APP_UID_LOW_LIMIT)))
		return;

	svm_oom_msg = (char*)kmalloc(128, GFP_KERNEL);
	if (!svm_oom_msg)
		return;

	if (is_locked) {
		if (mm->reserve_vma) {
			backed_vm_size =
				mm->reserve_vma->vm_end - mm->reserve_vma->vm_start;
			res = 1;
		}
	} else {
		down_read(&mm->mmap_sem);
		if (mm->reserve_vma) {
			backed_vm_size =
				mm->reserve_vma->vm_end - mm->reserve_vma->vm_start;
			res = 1;
		}
		up_read(&mm->mmap_sem);
	}

	if ((svm_oom_pid == current->pid) &&
			time_after_eq((svm_oom_jiffies + 15*HZ), jiffies)) {
		svm_oom_pid = -1;
		oom = 1;
	}
	rlim = current->signal->rlim + RLIMIT_STACK;
	if (rlim->rlim_cur > STACK_RLIMIT_OVERFFLOW || brk_risk)
		change_stack = 1;

	if (change_stack) {
		len = snprintf(svm_oom_msg, 127,
				"{\"version\":1, \"size\":%ld, \"uid\":%u, \"type\":\"%s,%s,%s\"}",
				(long)rlim->rlim_cur, uid,
				(oom ? "oom" : "no_oom"),
				(res ? "res" : "no_res"),
				(brk_risk ? "brk" : "no_brk"));
		svm_oom_msg[len] = '\0';
		ohm_action_trig_with_msg(OHM_RLIMIT_MON, svm_oom_msg);
		kfree(svm_oom_msg);
		return;
	}

	current_time_ns = ktime_get_boot_ns();
	if ((current_time_ns > current->real_start_time) ||
			(current_time_ns - current->real_start_time >= TRIGGER_TIME_NS))
		over_time = 1;

	if (oom || (!change_stack && !res && over_time)) {
		len = snprintf(svm_oom_msg, 127,
				"{\"version\":1, \"size\":%lu, \"uid\":%u, \"type\":\"%s,%s,%s\"}",
				backed_vm_size, uid,
				(oom ? "oom" : "no_oom"),
				(res ? "res" : "no_res"),
				(change_stack ? "stack" : "no_stack"));
		svm_oom_msg[len] = '\0';
		ohm_action_trig_with_msg(OHM_SVM_MON, svm_oom_msg);
	}
	kfree(svm_oom_msg);
}
#else
void trigger_stack_limit_changed(long value)
{
	pr_warn("[gloom] CONFIG_OPLUS_HEALTHINFO is not enabled.\n");
}

void trigger_svm_oom_event(struct mm_struct *mm, bool brk_risk, bool is_locked)
{
	pr_warn("[gloom] CONFIG_OPLUS_HEALTHINFO is not enabled.\n");
}
#endif
EXPORT_SYMBOL(trigger_stack_limit_changed);
EXPORT_SYMBOL(trigger_svm_oom_event);

static ssize_t reserved_area_enable_write(struct file *file,
	const char __user *buff, size_t len, loff_t *ppos)
{
	char kbuf[12] = {'0'};
	long val;
	int ret;

	if (len > 11)
		len = 11;

	if (copy_from_user(&kbuf, buff, len))
		return -EFAULT;
	kbuf[len] = '\0';

	ret = kstrtol(kbuf, 10, &val);
	if (ret)
		return -EINVAL;

	reserved_area_enable = val ? 1 : 0;

	return len;
}

static ssize_t reserved_area_enable_read(struct file *file,
			char __user *buffer, size_t count, loff_t *off)
{
	char kbuf[12] = {'0'};
	int len;

	len = snprintf(kbuf, 12, "%d\n", reserved_area_enable);
	if (kbuf[len] != '\n')
		kbuf[len] = '\n';

	if (len > *off)
		len -= *off;
	else
		len = 0;

	if (copy_to_user(buffer, kbuf + *off, (len < count ? len : count)))
		return -EFAULT;

	*off += (len < count ? len : count);
	return (len < count ? len : count);
}

static const struct file_operations proc_reserved_area_enable_ops = {
	.write          = reserved_area_enable_write,
	.read		= reserved_area_enable_read,
};

int create_reserved_area_enable_proc(struct proc_dir_entry *parent)
{
	if (parent && proc_create("reserved_area_enable",
				S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
				parent, &proc_reserved_area_enable_ops)) {
		printk("Register reserved_area_enable interface passed.\n");
		return 0;
	}
	pr_err("Register reserved_area_enable interface failed.\n");
	return -ENOMEM;
}

static void sum_resmap_events(unsigned int *ret)
{
	int cpu;
	int i;

	memset(ret, 0, RESMAP_TEXIT * sizeof(unsigned int));

	for_each_online_cpu(cpu) {
		struct resmap_event_state *this = &per_cpu(resmap_event_states,
				cpu);

		for (i = 0; i < RESMAP_TEXIT; i++)
			ret[i] += this->event[i];
	}
}

void all_resmap_events(unsigned int *ret)
{
	get_online_cpus();
	sum_resmap_events(ret);
	put_online_cpus();
}
EXPORT_SYMBOL_GPL(all_resmap_events);

static int resmap_account_show(struct seq_file *s, void *unused)
{
	int i;
	unsigned int all_events[RESMAP_TEXIT];

	all_resmap_events(all_events);
	for (i = 0; i < RESMAP_TEXIT; i++)
		seq_printf(s, "%-18s %u\n", resmap_item_name[i], all_events[i]);
	return 0;
}

static int resmap_account_open(struct inode *inode, struct file *file)
{
	return single_open(file, resmap_account_show, NULL);
}

static const struct file_operations resmap_account_fops = {
	.owner = THIS_MODULE,
	.open = resmap_account_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init resmap_account_init(void)
{
	struct dentry *f = debugfs_create_file("resmap_account", 0444, NULL,
			NULL, &resmap_account_fops);
	if (!f)
		pr_warn("%s create resmap_account failed\n", __func__);
	else
		pr_info("%s create resmap_account passed\n", __func__);

	return 0;
}

device_initcall(resmap_account_init);
MODULE_LICENSE("GPL");
