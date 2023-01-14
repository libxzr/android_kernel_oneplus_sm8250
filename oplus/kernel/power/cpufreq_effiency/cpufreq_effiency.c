#include <linux/cpufreq_effiency.h>

// affect_mode, @1: enable cpufreq effiency, @0: disable cprfreq effiency.
static int affect_mode = 0;
module_param(affect_mode, int, 0664);

// Silver cluster, param@0: affect_freq1, param@1: affect_thres1, param@2: affect_freq2, param@3: affect_thres2, param@4 need mask freq.
static int cluster0_effiency[MAX_CLUSTER_PARAMETERS] = { 0, 0, 0, 0, 0};
module_param_array(cluster0_effiency, int, NULL, 0664);

// Gold cluster, param@0: affect_freq1, param@1: affect_thres1, param@2: affect_freq2, param@3: affect_thres2, param@4 need mask freq.
static int cluster1_effiency[MAX_CLUSTER_PARAMETERS] = { 0, 0, 0, 0, 0};
module_param_array(cluster1_effiency, int, NULL, 0664);

// Gold_plus cluster, param@0: affect_freq1, param@1: affect_thres1, param@2: affect_freq2, param@3: affect_thres2, param@4 need mask freq.
static int cluster2_effiency[MAX_CLUSTER_PARAMETERS] = { 0, 0, 0, 0, 0};
module_param_array(cluster2_effiency, int, NULL, 0664);

// declare a global variable to record platform sod id information.
static unsigned int platform_soc_id = 0;

// declare a spinlock which been inited in init function.
raw_spinlock_t power_effiency_lock;

static int get_cluster_num(struct cpufreq_policy *policy)
{
	int first_cpu, cluster_num;
	struct device *cpu_dev;

	first_cpu = cpumask_first(policy->related_cpus);
	cpu_dev = get_cpu_device(first_cpu);
	if(cpu_dev == NULL) {
		pr_err("failed to get cpu device, return. \n");
		return -1;
	}

	cluster_num = topology_physical_package_id(cpu_dev->id);
	if(cluster_num >= MAX_CLUSTER) {
		pr_err("failed to get cluster, as error cluster id, return. \n");
		return -1;
	}

	if (platform_soc_id == SM8350_SOC_ID) {
		if(*(opp_number + cluster_num) != *(sm8350_cluster_pd + cluster_num)) {
			//pr_info("opp_number: %d  cluster_pd: %d. \n", *(opp_number + cluster_num), *(sm8350_cluster_pd + cluster_num));
			return -1;
		}
	} else if (platform_soc_id == SM8450_SOC_ID) {
		if(*(opp_number + cluster_num) != *(sm8450_cluster_pd + cluster_num)) {
			//pr_info("opp_number: %d  cluster_pd: %d. \n", *(opp_number + cluster_num), *(sm8450_cluster_pd + cluster_num));
			return -1;
		}
	}

	//pr_info("cluster_num: %d platform_soc_id: %d opp_number: %d. \n", cluster_num, platform_soc_id, *(opp_number + cluster_num));

	return cluster_num;
}

static unsigned int *get_cluster_pd(struct cpufreq_policy *policy)
{
	int cluster_id;

	cluster_id = get_cluster_num(policy);
	if (platform_soc_id == SM8350_SOC_ID) {
		switch (cluster_id) {
			case SLIVER_CLUSTER:
				return sm8350_pd_sliver;
			break;
			case GOLDEN_CLUSTER:
				return sm8350_pd_golden;
			break;
			case GOPLUS_CLUSTER:
				return sm8350_pd_goplus;
			break;
			default:
				return NULL;
			break;
		}
	} else if (platform_soc_id == SM8450_SOC_ID) {
		switch (cluster_id) {
			case SLIVER_CLUSTER:
				return sm8450_pd_sliver;
			break;
			case GOLDEN_CLUSTER:
				return sm8450_pd_golden;
			break;
			case GOPLUS_CLUSTER:
				return sm8450_pd_goplus;
			break;
			default:
				return NULL;
			break;
		}
	} else {
		return NULL;
	}
}

static bool was_diff_powerdomain(struct cpufreq_policy *policy, unsigned int freq)
{
	int index, index_pre;
	unsigned int *cluster_pd;

	index = cpufreq_frequency_table_target(policy, freq, CPUFREQ_RELATION_L);
	index_pre = cpufreq_frequency_table_target(policy, freq -1, CPUFREQ_RELATION_H);

	if (index == index_pre) {
		//pr_info("index: %d index_pre: %d \n", index, index_pre);
		return false;
	}

	cluster_pd = get_cluster_pd(policy);
	if(cluster_pd != NULL) {
		//pr_info("freq: %d *(cluster_pd + index): %d *(cluster_pd + index_pre): %d \n", freq, *(cluster_pd + index), *(cluster_pd + index_pre));
		if(*(cluster_pd + index) == *(cluster_pd + index_pre)) {
			return false;
		} else {
			return true;  //diff power domain
		}
	}

	return false;
}

// @freq, the need mask frequence point.
static bool was_mask_freq(struct cpufreq_policy *policy, unsigned int freq)
{
	int cluster_id;
	bool ret;

	cluster_id = get_cluster_num(policy);
	switch (cluster_id) {
		case SLIVER_CLUSTER:
			ret = (freq == cluster0_effiency[MASK_FREQ_VALUE])? true : false;
		break;
		case GOLDEN_CLUSTER:
			ret = (freq == cluster1_effiency[MASK_FREQ_VALUE])? true : false;
		break;
		case GOPLUS_CLUSTER:
			ret = (freq == cluster2_effiency[MASK_FREQ_VALUE])? true : false;
		break;
		default:
			ret = false;
		break;
	}

	return ret;
}

static unsigned int select_effiency_freq(struct cpufreq_policy *policy, unsigned int freq, unsigned int loadadj_freq)
{
	unsigned int freq_temp, index_temp, affect_thres;
	int cluster_id;

	index_temp = cpufreq_frequency_table_target(policy, freq - 1, CPUFREQ_RELATION_H);
	freq_temp = policy->freq_table[index_temp].frequency;

	if((loadadj_freq > freq) || (loadadj_freq < freq_temp)) {
		return freq;
	}

	cluster_id = get_cluster_num(policy);
	switch (cluster_id) {
		case SLIVER_CLUSTER:
			if ((cluster0_effiency[AFFECT_FREQ_VALUE2] > 0) && (freq >= cluster0_effiency[AFFECT_FREQ_VALUE2])) {
				affect_thres = cluster0_effiency[AFFECT_THRES_SIZE2];
			} else if ((cluster0_effiency[AFFECT_FREQ_VALUE1] > 0) && (freq >= cluster0_effiency[AFFECT_FREQ_VALUE1])) {
				affect_thres = cluster0_effiency[AFFECT_THRES_SIZE1];
			} else {
				affect_thres = 0;
			}
		break;
		case GOLDEN_CLUSTER:
			if ((cluster1_effiency[AFFECT_FREQ_VALUE2] > 0) && (freq >= cluster1_effiency[AFFECT_FREQ_VALUE2])) {
				affect_thres = cluster1_effiency[AFFECT_THRES_SIZE2];
			} else if ((cluster1_effiency[AFFECT_FREQ_VALUE1] > 0) && (freq >= cluster1_effiency[AFFECT_FREQ_VALUE1])) {
				affect_thres = cluster1_effiency[AFFECT_THRES_SIZE1];
			} else {
				affect_thres = 0;
			}
		break;
		case GOPLUS_CLUSTER:
			if ((cluster2_effiency[AFFECT_FREQ_VALUE2] > 0) && (freq >= cluster2_effiency[AFFECT_FREQ_VALUE2])) {
				affect_thres = cluster2_effiency[AFFECT_THRES_SIZE2];
			} else if ((cluster2_effiency[AFFECT_FREQ_VALUE1] > 0) && (freq >= cluster2_effiency[AFFECT_FREQ_VALUE1])) {
				affect_thres = cluster2_effiency[AFFECT_THRES_SIZE1];
			} else {
				affect_thres = 0;
			}
		break;
		default:
			affect_thres = 0;
		break;
	}

	if(abs(loadadj_freq - freq_temp) < affect_thres) {
		return freq_temp;
	}

	return freq;
}

unsigned int update_power_effiency_lock(struct cpufreq_policy *policy, unsigned int freq, unsigned int loadadj_freq)
{
	unsigned int temp_index;
	unsigned long flags;

	if ((affect_mode == 0) || (freq <= 0)) {
		return freq;
	}

	raw_spin_lock_irqsave(&power_effiency_lock, flags);
	if (was_mask_freq(policy, freq)) {
		temp_index = cpufreq_frequency_table_target(policy, freq -1 , CPUFREQ_RELATION_H);
		freq = policy->freq_table[temp_index].frequency;
	} else if (was_diff_powerdomain(policy, freq)) {
		freq = select_effiency_freq(policy, freq, loadadj_freq);
	}
	raw_spin_unlock_irqrestore(&power_effiency_lock, flags);

	return freq;
}
EXPORT_SYMBOL(update_power_effiency_lock);

static int cpufreq_pd_init(void)
{
	const char *prop_str;

	if (!of_root) {
		pr_info("of_root is null!\n");
		return -1;
	}

	of_node_get(of_root);
	prop_str = of_get_property(of_root, "compatible", NULL);
	if (!prop_str) {
		pr_info("of_root's compatible is null!\n");
		of_node_put(of_root);
		return -1;
	}

	if (strstr(prop_str, PLATFORM_SM8350)) {
		platform_soc_id = SM8350_SOC_ID;
	} else if (strstr(prop_str, PLATFORM_SM8450)) {
		platform_soc_id = SM8450_SOC_ID;
	} else {
		platform_soc_id = ABSENT_SOC_ID;
	}
	of_node_put(of_root);

	return 0;
}

static int frequence_opp_init(struct cpufreq_policy *policy)
{
	int first_cpu, cluster_id, opp_num;
	struct device *cpu_dev;

	first_cpu = cpumask_first(policy->related_cpus);
	cpu_dev = get_cpu_device(first_cpu);
	if(cpu_dev == NULL) {
		pr_err("failed to get cpu device, return. \n");
		return -1;
	}

	cluster_id = topology_physical_package_id(cpu_dev->id);
	if(cluster_id >= MAX_CLUSTER) {
		pr_err("failed to get cluster, as error cluster id, return. \n");
		return -1;
	}

	opp_num = dev_pm_opp_get_opp_count(cpu_dev);
	// Init the OOP number during boot stage.
	if (*(opp_number + cluster_id) != opp_num) {
		*(opp_number + cluster_id) = opp_num;
	}

	return 0;
}

static int __init cpufreq_effiency_init(void)
{
	struct cpufreq_policy *policy;
	int cpu;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy == NULL) {
			pr_err("cpu %d, policy is null\n", cpu);
			continue;
		}

		frequence_opp_init(policy);
	}

	cpufreq_pd_init();

	raw_spin_lock_init(&power_effiency_lock);

	pr_info("cpufreq_effiency_init finished. \n");

	return 0;
}

module_init(cpufreq_effiency_init);

MODULE_DESCRIPTION("cpufreq_effiency");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
