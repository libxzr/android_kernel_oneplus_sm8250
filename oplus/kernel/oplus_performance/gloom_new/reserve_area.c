// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/sched/signal.h>
#include <linux/security.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/highmem.h>
#include <linux/reserve_area.h>
#ifdef CONFIG_OPLUS_HEALTHINFO
#include <soc/oplus/healthinfo.h>
#endif
#include <linux/version.h>
#define STACK_RLIMIT_OVERFFLOW (32<<20)
#define THRIDPART_APP_UID_LOW_LIMIT 10000UL
#define NS_PER_SEC (1000000000LLU)
#define TRIGGER_TIME_NS (300*NS_PER_SEC)

int svm_oom_pid = -1;
unsigned long svm_oom_jiffies = 0;
static int va_feature = RESERVE_FEATURES;

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

int get_va_feature_value(unsigned int feature)
{
	return va_feature & (feature & ~RESERVE_LOGGING);
}

unsigned long get_unmmaped_area_from_anti_fragment(struct mm_struct *mm,
		struct vm_unmapped_area_info *info)
{
	struct vm_unmapped_area_info info_b;
	unsigned long addr = -ENOMEM;

	switch (info->length) {
		case 4096: case 8192: case 16384: case 32768:
		case 65536: case 131072: case 262144:
			info_b = *info;
			info_b.high_limit = current->mm->va_feature_rnd - (SIZE_10M * (ilog2(info->length) - 12));
			info_b.low_limit = current->mm->va_feature_rnd - (SIZE_10M * 7);
			addr = unmapped_area_topdown(&info_b);
			if (!offset_in_page(addr))
				return addr;
		default:
			break;
	}

	return addr;
}

unsigned long get_unmmaped_area_from_reserved(struct mm_struct *mm,
		struct vm_unmapped_area_info *info)
{
	info->flags = VM_UNMAPPED_AREA_TOPDOWN;
	info->low_limit = max(PAGE_SIZE, mmap_min_addr);
	info->high_limit = mm->mmap_base;

	return vm_unmapped_area(info);
}

static ssize_t va_feature_write(struct file *file,
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

	ret = kstrtol(kbuf, 16, &val);
	if (ret)
		return -EINVAL;

	va_feature = val & RESERVE_FEATURES;
	return len;
}

static ssize_t va_feature_read(struct file *file,
		char __user *buffer, size_t count, loff_t *off)
{
	char kbuf[12] = {'0'};
	int len;

	len = snprintf(kbuf, 12, "%#x\n", va_feature);
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

static const struct file_operations proc_va_feature_ops = {
	.write          = va_feature_write,
	.read		= va_feature_read,
};

int create_reserved_area_enable_proc(struct proc_dir_entry *parent)
{
	if (parent && proc_create("va_feature", S_IRUSR|S_IWUSR,
				parent, &proc_va_feature_ops)) {
		printk("Register va_feature interface passed.\n");
		return 0;
	}
	pr_err("Register va_feature interface failed.\n");
	return -ENOMEM;
}
EXPORT_SYMBOL(create_reserved_area_enable_proc);

#ifdef CONFIG_OPLUS_HEALTHINFO
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

	if (!((va_feature & RESERVE_LOGGING) &&
				(current->pid == current->tgid) &&
				is_compat_task() &&
				check_parent_is_zygote(current) &&
				(uid >= THRIDPART_APP_UID_LOW_LIMIT)))
		return;

	svm_oom_msg = (char*)kmalloc(128, GFP_KERNEL);
	if (!svm_oom_msg)
		return;

	if (is_locked) {
		if (mm->va_feature & RESERVE_AREA)
			res = 1;
	} else {
		down_read(&mm->mmap_sem);
		if (mm->va_feature & RESERVE_AREA)
			res = 1;
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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	current_time_ns = ktime_to_ns(ktime_get_boottime());
#else
	current_time_ns = ktime_get_boot_ns();
#endif
	if ((current_time_ns > current->real_start_time) ||
			(current_time_ns - current->real_start_time >= TRIGGER_TIME_NS))
		over_time = 1;

	if (oom || (!change_stack && !res && over_time)) {
		len = snprintf(svm_oom_msg, 127,
				"{\"version\":1, \"size\":%lu, \"uid\":%u, \"type\":\"%s,%s,%s\"}",
				0, uid,
				(oom ? "oom" : "no_oom"),
				(res ? "res" : "no_res"),
				(change_stack ? "stack" : "no_stack"));
		svm_oom_msg[len] = '\0';
		ohm_action_trig_with_msg(OHM_SVM_MON, svm_oom_msg);
	}
	kfree(svm_oom_msg);
}
#else
void trigger_svm_oom_event(struct mm_struct *mm, bool brk_risk, bool is_locked)
{
	pr_warn("[gloom] CONFIG_OPLUS_HEALTHINFO is not enabled.\n");
}
#endif
