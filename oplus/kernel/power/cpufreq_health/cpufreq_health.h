/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Oplus. All rights reserved.
 */


#ifndef _CPUFREQ_HEALTH_H
#define _CPUFREQ_HEALTH_H
#include <linux/kernel_stat.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/slab.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "OCH: " fmt

struct oplus_cpufreq_frequency_table {
	unsigned int frequency;
	u64 floor_time;
	u64 ceiling_time;
	s64 floor_count;
	s64 ceiling_count;
	unsigned int voltage;
};

struct oplus_cpufreq_health {
	struct oplus_cpufreq_frequency_table *table;
	unsigned int prev_floor_freq;
	unsigned int curr_floor_freq;
	unsigned int prev_ceiling_freq;
	unsigned int curr_ceiling_freq;
	u64 last_update_time;
	u64 bootup_time;
	unsigned int len;
	spinlock_t lock;
	struct kobject kobj;
};

extern void cpufreq_health_get_state(struct cpufreq_policy *policy);
#endif /* _CPUFREQ_HEALTH_H */
