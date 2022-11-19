/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef __OPLUS_FOREGROUND_IO_OPT__
#define __OPLUS_FOREGROUND_IO_OPT__

#ifdef CONFIG_FG_TASK_UID
#include <linux/healthinfo/fg.h>
#endif /*CONFIG_FG_TASK_UID*/

extern unsigned int sysctl_fg_io_opt;

extern void fg_bg_max_count_init(struct request_queue *q);
extern void queue_throtl_add_request(struct request_queue *q,
					    struct request *rq, bool front);
extern ssize_t queue_fg_count_max_show(struct request_queue *q,
	char *page);
extern ssize_t queue_fg_count_max_store(struct request_queue *q,
	const char *page, size_t count);
extern ssize_t queue_both_count_max_show(struct request_queue *q,
	char *page);
extern ssize_t queue_both_count_max_store(struct request_queue *q,
	const char *page, size_t count);
extern bool high_prio_for_task(struct task_struct *t);
extern struct request * smart_peek_request(struct request_queue *q);
#endif /*__OPLUS_FOREGROUND_IO_OPT__*/
