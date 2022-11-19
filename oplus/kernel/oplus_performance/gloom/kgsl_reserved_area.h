/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "kgsl_device.h"
#include <linux/resmap_account.h>

typedef unsigned long search_range_function_t(struct kgsl_process_private *private,
			struct kgsl_mem_entry *entry,
			unsigned long start, unsigned long end,
			unsigned long len, uint64_t align, unsigned long mmap_flags);


#define try_reserved_region(vma, addr, len, private, entry, result) do {\
	if ((!vma && check_valid_reserve_addr(current->mm, addr, addr+len)) ||\
			(vma && ((addr + len) <= vma->vm_start))) {\
		result = _gpu_set_svm_region(private, entry, addr, len);\
		if (!IS_ERR_VALUE(result))\
			return result;\
	}\
} while (0);

#define record_svm_oom_info(val) do {\
	if (IS_ERR_VALUE(val)) {\
		static DEFINE_RATELIMIT_STATE(svm_log_limit, 1*HZ, 1);\
		if (__ratelimit(&svm_log_limit)) {\
			svm_oom_pid = current->tgid;\
			svm_oom_jiffies = jiffies;\
		}\
	}\
} while (0);

extern unsigned long kgsl_search_range(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry,
		unsigned long start, unsigned end,
		unsigned long addr, unsigned long len, uint64_t align,
		unsigned long hint, search_range_function_t search_range);
