/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_INCLUDE_PATH ../../block/foreground_io_opt/trace
#define TRACE_SYSTEM foreground_io_opt_trace

#if !defined(_OPLUS_FOREGROUND_IO_OPT_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _OPLUS_FOREGROUND_IO_OPT_TRACE_H

#include <linux/tracepoint.h>

/*trace*/
TRACE_EVENT(block_fg_io_peek_req,
	TP_PROTO(struct task_struct *task, long req_addr, \
		 char * fg, int count),

	TP_ARGS(task, req_addr, fg, count),

	TP_STRUCT__entry(
			__array(char, comm, TASK_COMM_LEN)
			__field(pid_t, pid)
			__field(long, req_addr)
			__array(char, fg, 3)
			__field(int, count)
	),

	TP_fast_assign(
			memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
			__entry->pid = task->pid;
			__entry->req_addr = req_addr;
			memcpy(__entry->fg, fg, 3);
			__entry->count = count;
	),

	TP_printk("%s (%d), req_addr %x task_group:%s, count %d",
		__entry->comm, __entry->pid, __entry->req_addr,
		__entry->fg, __entry->count)
);
#endif /*_OPLUS_FOREGROUND_IO_OPT_TRACE_H*/
#include <trace/define_trace.h>
