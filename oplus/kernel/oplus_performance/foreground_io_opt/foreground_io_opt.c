// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/list_sort.h>
#include "foreground_io_opt.h"

#define CREATE_TRACE_POINTS
#include <trace/foreground_io_opt_trace.h>

#define FG_CNT_DEF 20
#define BOTH_CNT_DEF 10

void fg_bg_max_count_init(struct request_queue *q)
{
	q->fg_count_max = FG_CNT_DEF;
	q->both_count_max = BOTH_CNT_DEF;

	q->fg_count = FG_CNT_DEF;
	q->both_count = BOTH_CNT_DEF;
}

static inline bool should_get_fg_req(struct request_queue *q)
{
	if (!list_empty(&q->fg_head)
		 && (q->fg_count > 0))
		 return true;

	return false;
}

static inline bool should_get_bg_req(struct request_queue *q)
{
	if (q->both_count > 0)
		 return true;

	return false;
}

static struct request *get_fg_bg_req(struct request_queue *q)
{

	struct request *rq = NULL;

	if (!list_empty(&q->queue_head)) {
		if (should_get_fg_req(q)) {
			rq = list_entry(q->fg_head.next, struct request, fg_list);
			q->fg_count--;
			trace_block_fg_io_peek_req(current, (long)rq,"FG\0",q->fg_count);
		}
		else if (should_get_bg_req(q)) {
			rq = list_entry_rq(q->queue_head.next);
			q->both_count--;
			trace_block_fg_io_peek_req(current, (long)rq,"BG\0",q->both_count);
		}
		else {
			q->fg_count = q->fg_count_max;
			q->both_count = q->both_count_max;
			rq = list_entry_rq(q->queue_head.next);
		}
	}
	return rq;
}

struct request * smart_peek_request(struct request_queue *q)
{
	return get_fg_bg_req(q);
}

void queue_throtl_add_request(struct request_queue *q,
					    struct request *rq, bool front)
{
	struct list_head *head;

	if (unlikely(!sysctl_fg_io_opt))
		return;

	if (rq->cmd_flags & REQ_FG) {
		head = &q->fg_head;
		if (front)
			list_add(&rq->fg_list, head);
		else
			list_add_tail(&rq->fg_list, head);
	}
}

/*blk-sys*/
static ssize_t
queue_var_show(unsigned long var, char *page)
{
	if (unlikely(!sysctl_fg_io_opt))
		return 0;

	return sprintf(page, "%lu\n", var);
}

static ssize_t
queue_var_store(unsigned long *var, const char *page, size_t count)
{
	int err;
	unsigned long v;

	if (unlikely(!sysctl_fg_io_opt))
		return 0;

	err = kstrtoul(page, 10, &v);
	if (err || v > UINT_MAX)
		return -EINVAL;

	*var = v;

	return count;
}

ssize_t queue_fg_count_max_show(struct request_queue *q,
	char *page)
{
	int cnt = q->fg_count_max;

	return queue_var_show(cnt, (page));
}

ssize_t queue_fg_count_max_store(struct request_queue *q,
	const char *page, size_t count)
{
	unsigned long cnt;
	ssize_t ret = queue_var_store(&cnt, page, count);

	if (ret < 0)
		return ret;

	q->fg_count_max= cnt;

	return ret;
}

ssize_t queue_both_count_max_show(struct request_queue *q,
	char *page)
{
	int cnt = q->both_count_max;

	return queue_var_show(cnt, (page));
}

ssize_t queue_both_count_max_store(struct request_queue *q,
	const char *page, size_t count)
{
	unsigned long cnt;
	ssize_t ret = queue_var_store(&cnt, page, count);

	if (ret < 0)
		return ret;

	q->both_count_max= cnt;

	return ret;
}
