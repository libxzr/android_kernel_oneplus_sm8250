/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */


#if !defined(_OPLUS_CFS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _OPLUS_CFS_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM oplus_sched
#define TRACE_INCLUDE_FILE uifirst_sched_trace

TRACE_EVENT(oplus_tp_sched_change_ux,
	TP_PROTO(int chg_ux, int target_cpu),
	TP_ARGS(chg_ux, target_cpu),
	TP_STRUCT__entry(
		__field(int, chg_ux)
		__field(int, target_cpu)
	),
	TP_fast_assign(
		__entry->chg_ux = chg_ux;
		__entry->target_cpu = target_cpu;
	),
	TP_printk("chg_ux=%d target_cpu=%d", __entry->chg_ux, __entry->target_cpu)
);

TRACE_EVENT(oplus_tp_sched_switch_ux,
	TP_PROTO(int chg_ux, int target_cpu),
	TP_ARGS(chg_ux, target_cpu),
	TP_STRUCT__entry(
		__field(int, chg_ux)
		__field(int, target_cpu)
	),
	TP_fast_assign(
		__entry->chg_ux = chg_ux;
		__entry->target_cpu = target_cpu;
	),
	TP_printk("chg_ux=%d target_cpu=%d", __entry->chg_ux, __entry->target_cpu)
);

#endif /* _OPLUS_CFS_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
