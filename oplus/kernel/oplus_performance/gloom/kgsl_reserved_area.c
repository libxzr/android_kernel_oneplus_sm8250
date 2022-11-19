// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/mm.h>
#include <linux/resmap_account.h>
#include "kgsl_reserved_area.h"

unsigned long kgsl_search_range(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry,
		unsigned long start, unsigned end,
		unsigned long addr, unsigned long len, uint64_t align,
		unsigned long hint, search_range_function_t search_range)
{
	struct vm_area_struct *reserve_vma;
	unsigned long res_start, res_end;
	unsigned long result = -ENOMEM;
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;

	result = search_range(private, entry, start, addr, len, align, 0);
	if (!IS_ERR_VALUE(result))
		return result;

	if (hint != 0) {
		result = search_range(private, entry, addr, end, len,
				align, 0);
		if (!IS_ERR_VALUE(result))
			return result;
	}

	reserve_vma = mm->reserve_vma;
	if (reserve_vma) {
		count_resmap_event(RESMAP_ACTION);

		res_start = reserve_vma->vm_start;
		res_end = reserve_vma->vm_end;

		if (len > (res_end - res_start)) {
			count_resmap_event(RESMAP_FAIL);
			return result;
		}

		result = search_range(private, entry, res_start, res_end, len, align,
				VM_UNMAPPED_AREA_RESERVED);

		if (IS_ERR_VALUE(result)) {
			static DEFINE_RATELIMIT_STATE(try_reserve_fail_rs,
					5*HZ, 8);
			unsigned long chunk, chunk_start;
			unsigned long used_size, vma_size, lastend;

			count_resmap_event(RESMAP_FAIL);
			if (__ratelimit(&try_reserve_fail_rs)) {
				used_size = 0;
				lastend = res_start;
				chunk = 0;
				chunk_start = 0;

				for (vma = mm->reserve_mmap; vma;
						vma = vma->vm_next) {
					vma_size = vma->vm_end - vma->vm_start;
					if ((vma->vm_start - lastend) >	chunk) {
						chunk = vma->vm_start - lastend;
						chunk_start = lastend;
					}
					lastend = vma->vm_end;
					used_size += vma_size;
				}

				if ((res_end - lastend) > chunk) {
					chunk = res_end - lastend;
					chunk_start = lastend;
				}

				pr_err("emergency! current:%s pid:%d leader:%s result:%lx reserve_vma:%p(%lu) alloc len:%luKB start:0x%lx end:0x%lx reserve_map_count:%d used_size:%luKB chunk:%luKB chunk_start:%#lx align:%#lx reserve_highest_vm_end:%#lx\n",
					current->comm, current->pid,
					current->group_leader->comm,
					result, reserve_vma, res_end - res_start,
					len>>10, res_start, res_end,
					mm->reserve_map_count, used_size>>10,
					chunk>>10, chunk_start, align,
					mm->reserve_highest_vm_end);
			}
		} else
			count_resmap_event(RESMAP_SUCCESS);
	}

	return result;
}
