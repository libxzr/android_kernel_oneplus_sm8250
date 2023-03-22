// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021-2022 Oplus. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/cpufreq.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/topology.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/cpufreq_health.h>
#include <linux/cpumask.h>
#include <linux/pm_opp.h>

struct oplus_cpufreq_health_attr {
	struct attribute attr;
	ssize_t (*show)(struct oplus_cpufreq_health *och, char *buf);
	ssize_t (*store)(struct oplus_cpufreq_health *och, const char *buf,
		size_t count);
};

static ssize_t show_floor_limit(struct oplus_cpufreq_health *och, char *buf)
{
	unsigned long flags;
	int i, count = 0;

	if (!och->table)
		return 0;

	spin_lock_irqsave(&och->lock, flags);
	for (i = 0; i < och->len; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count, "%-10u\t%-10llu\t%-10llu\n",
				  och->table[i].frequency, och->table[i].floor_time, och->table[i].floor_count);
	}
	count += snprintf(buf + count, PAGE_SIZE - count, "elapsed %lld\n",
				jiffies_64_to_clock_t(och->last_update_time - och->bootup_time));
	spin_unlock_irqrestore(&och->lock, flags);
	return count;
}

static ssize_t show_ceiling_limit(struct oplus_cpufreq_health *och, char *buf)
{
	unsigned long flags;
	int i, count = 0;

	if (!och->table)
		return 0;

	spin_lock_irqsave(&och->lock, flags);
	for (i = 0; i < och->len; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count, "%-10u\t%-10llu\t%-10llu\n",
				  och->table[i].frequency, och->table[i].ceiling_time, och->table[i].ceiling_count);
	}
	count += snprintf(buf + count, PAGE_SIZE - count, "elapsed %lld\n",
				jiffies_64_to_clock_t(och->last_update_time - och->bootup_time));
	spin_unlock_irqrestore(&och->lock, flags);
	return count;
}

static ssize_t show_cpu_voltage(struct oplus_cpufreq_health *och, char *buf)
{
	unsigned long flags;
	int i, count = 0;

	if (!och->table)
		return 0;

	spin_lock_irqsave(&och->lock, flags);
	for (i = 0; i < och->len; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count, "%-10u\t%-10u\n",
				  och->table[i].frequency, och->table[i].voltage);
	}
	spin_unlock_irqrestore(&och->lock, flags);
	return count;
}

#define oplus_cpufreq_health_attr_ro(_name)		\
static struct oplus_cpufreq_health_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define oplus_cpufreq_health_attr_rw(_name)			\
static struct oplus_cpufreq_health_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

oplus_cpufreq_health_attr_ro(floor_limit);
oplus_cpufreq_health_attr_ro(ceiling_limit);
oplus_cpufreq_health_attr_ro(cpu_voltage);

static struct attribute *default_attrs[] = {
	&floor_limit.attr,
	&ceiling_limit.attr,
	&cpu_voltage.attr,
	NULL
};

#define to_oplus_cpufreq_health(k) \
		container_of(k, struct oplus_cpufreq_health, kobj)
#define to_attr(a) container_of(a, struct oplus_cpufreq_health_attr, attr)

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct oplus_cpufreq_health *och = to_oplus_cpufreq_health(kobj);
	struct oplus_cpufreq_health_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(och, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct oplus_cpufreq_health *och = to_oplus_cpufreq_health(kobj);
	struct oplus_cpufreq_health_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(och, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_oplus_cpufreq_health = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};

#define MAX_CLUSTERS 3

static struct oplus_cpufreq_health oplus_cpufreq_health[MAX_CLUSTERS];

static int freq_table_get_index(struct oplus_cpufreq_health *och, unsigned int freq)
{
	int index;

	for (index = 0; index < och->len; index++)
		if (och->table[index].frequency == freq)
			return index;
	return -EINVAL;
}

void cpufreq_health_get_state(struct cpufreq_policy *policy)
{
	int first_cpu = cpumask_first(policy->related_cpus);
	int cluster_id, floor_idx, ceiling_idx;
	u64 delta, now;
	struct oplus_cpufreq_health *och;
	unsigned long flags;

	cluster_id = topology_physical_package_id(first_cpu);
	if (cluster_id > MAX_CLUSTERS)
		return;

	och = &oplus_cpufreq_health[cluster_id];
	if (och->curr_floor_freq == policy->min &&
		och->curr_ceiling_freq == policy->max)
		return;

	now = get_jiffies_64();

	delta = time_after64(now, och->last_update_time)
		? now - och->last_update_time : 0;
	delta = jiffies_64_to_clock_t(delta);

	floor_idx = freq_table_get_index(och, och->curr_floor_freq);
	ceiling_idx = freq_table_get_index(och, och->curr_ceiling_freq);

	if (floor_idx >= och->len || floor_idx < 0 || ceiling_idx >= och->len || ceiling_idx < 0) {
		pr_err("invalid idx = %d %d\n", floor_idx, ceiling_idx);
		return;
	}

	spin_lock_irqsave(&och->lock, flags);
	/* statistics floor */
	och->table[floor_idx].floor_time += delta;
	if (policy->min != och->curr_floor_freq)
		och->table[floor_idx].floor_count++;
	och->prev_floor_freq = och->curr_floor_freq;
	och->curr_floor_freq = policy->min;

	/* statistics ceiling */
	och->table[ceiling_idx].ceiling_time += delta;
	if (policy->max != och->curr_ceiling_freq)
		och->table[ceiling_idx].ceiling_count++;
	och->prev_ceiling_freq = och->curr_ceiling_freq;
	och->curr_ceiling_freq = policy->max;

	/* updat last_update_time */
	och->last_update_time = now;
	spin_unlock_irqrestore(&och->lock, flags);
	pr_debug("cluster = %d freq = %d  %d, delta = %lld\n",
			cluster_id,
			och->prev_floor_freq,
			och->prev_ceiling_freq,
			delta);
}

static int cluster_init(int first_cpu, struct device *dev)
{
	int idx;
	int cluster_id;
	struct oplus_cpufreq_health *och;
	struct cpufreq_frequency_table *pos;
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(first_cpu);
	if (!policy)
		return -EINVAL;

	cluster_id = topology_physical_package_id(first_cpu);
	if (cluster_id >= MAX_CLUSTERS) {
		pr_err("Unsupported number of clusters(%d). Only %u supported\n",
				cluster_id,
				MAX_CLUSTERS);
		cpufreq_cpu_put(policy);
		return -EINVAL;
	}
	pr_info("cluster idx = %d, cpumask = 0x%x\n", cluster_id,
			(int)cpumask_bits(policy->related_cpus)[0]);

	och = &oplus_cpufreq_health[cluster_id];
	/* how many freq entry */
	cpufreq_for_each_valid_entry_idx(pos, policy->freq_table, idx);
	och->len = idx;

	och->table = kcalloc(och->len,
			sizeof(struct oplus_cpufreq_frequency_table),
			GFP_KERNEL);
	if (!och->table) {
		pr_err("No memory\n");
		cpufreq_cpu_put(policy);
		return -ENOMEM;
	}

	cpufreq_for_each_valid_entry_idx(pos, policy->freq_table, idx) {
		if (idx > och->len)
			break;
		och->table[idx].frequency = pos->frequency;
	}
	och->curr_floor_freq = och->prev_floor_freq = policy->min;
	och->curr_ceiling_freq = och->prev_ceiling_freq = policy->max;

	och->bootup_time = och->last_update_time = get_jiffies_64();
	spin_lock_init(&och->lock);
	cpufreq_cpu_put(policy);
	kobject_init(&och->kobj, &ktype_oplus_cpufreq_health);
	return kobject_add(&och->kobj, &dev->kobj, "cpufreq_health");
};

static int horae_cpu_stats_init(int first_cpu, struct device *cpu_dev)
{
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *freq_table, *pos;
	struct oplus_cpufreq_health *och;
	struct dev_pm_opp *opp;
	unsigned long freq;
	unsigned long mV;
	int cluster_id, idx;
	unsigned long flags;

	cluster_id = topology_physical_package_id(first_cpu);
	if (cluster_id >= MAX_CLUSTERS) {
		pr_err("Unsupported number of clusters(%d). Only %u supported\n",
				cluster_id,
				MAX_CLUSTERS);
		return -EINVAL;
	}

	och = &oplus_cpufreq_health[cluster_id];

	policy = cpufreq_cpu_get(first_cpu);
	freq_table = policy->freq_table;

	spin_lock_irqsave(&och->lock, flags);
	cpufreq_for_each_valid_entry(pos, freq_table) {
		idx = freq_table_get_index(och, pos->frequency);
		freq = pos->frequency * 1000;
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq);

		if (IS_ERR(opp))
			goto out;

		mV = dev_pm_opp_get_voltage(opp) / 1000;
		dev_pm_opp_put(opp);
		if (!mV)
			goto out;

		och->table[idx].voltage = mV;
		pr_info("freq = %ld, voltage = %ld\n", freq, mV);
	}
out:
	spin_unlock_irqrestore(&och->lock, flags);
	cpufreq_cpu_put(policy);

	return 0;
}

int __init oplus_cpufreq_health_init(void)
{
	struct cpufreq_policy *policy;
	int cpu, first_cpu;
	struct device *cpu_dev;
	cpumask_var_t available_cpus;

	cpumask_copy(available_cpus, cpu_possible_mask);

	for_each_possible_cpu(cpu) {
		if (!cpumask_test_cpu(cpu, available_cpus))
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_info("cpu %d, policy is null\n", cpu);
			continue;
		}

		cpumask_andnot(available_cpus, available_cpus, policy->related_cpus);
		first_cpu = cpumask_first(policy->related_cpus);
		cpu_dev = get_cpu_device(first_cpu);
		if (!cpu_dev)
			return -ENODEV;

		cluster_init(first_cpu, cpu_dev);
		horae_cpu_stats_init(first_cpu, cpu_dev);
	}

	return 0;
}

late_initcall(oplus_cpufreq_health_init);

MODULE_DESCRIPTION("CpuFreq health");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
