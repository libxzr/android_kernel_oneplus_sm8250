/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Oplus. All rights reserved.
 */

#ifndef _CPUFREQ_EFFIENCY_H
#define _CPUFREQ_EFFIENCY_H

#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

// the SOC_ID was define in /soc/qcom/socinfo.c
#define ABSENT_SOC_ID    0
#define SM8350_SOC_ID    415
#define SM8450_SOC_ID    457
#define PLATFORM_SM8350  "lahaina"
#define PLATFORM_SM8450  "waipio"

// define the cluster information
#define MAX_CLUSTER      3
#define SLIVER_CLUSTER   0
#define GOLDEN_CLUSTER   1
#define GOPLUS_CLUSTER   2

static unsigned int opp_number[MAX_CLUSTER] = {0, 0, 0};

// define the parameter size
#define MAX_CLUSTER_PARAMETERS 5
#define AFFECT_FREQ_VALUE1     0
#define AFFECT_THRES_SIZE1     1
#define AFFECT_FREQ_VALUE2     2
#define AFFECT_THRES_SIZE2     3
#define MASK_FREQ_VALUE        4

// Power Domain just configuration for SM8350,
// Table need refer to sm8350 clock_plan datasheet.
static unsigned int sm8350_cluster_pd[MAX_CLUSTER] = {16, 16, 19};
static unsigned int sm8350_pd_sliver[16] = {
	0,//300000   -LowSVS
	0,//403200   -LowSVS
	0,//499200   -LowSVS
	0,//595200   -LowSVS
	0,//691200   -LowSVS
	1,//806400   -SVS
	1,//902400   -SVS
	2,//998400   -SVS_L1
	2,//1094400  -SVS_L1
	2,//1209600  -SVS_L1
	3,//1305600  -Nominal
	3,//1401600  -Nominal
	3,//1497600  -Nominal
	4,//1612800  -Nominal_L1
	5,//1708800  -Turbo
	5 //1804800  -Turbo
};

static unsigned int sm8350_pd_golden[16] = {
	0,//710400   -LowSVS
	1,//844800   -SVS
	1,//960000   -SVS
	2,//1075200  -SVS_L1
	2,//1209600  -SVS_L1
	3,//1324800  -Nominal
	3,//1440000  -Nominal
	4,//1555200  -Nominal_L1
	4,//1670400  -Nominal_L1
	4,//1766400  -Nominal_L1
	5,//1881600  -Turbo
	6,//1996000  -Turbo_L1
	6,//2112000  -Turbo_L1
	7,//2227200  -Turbo_L3
	7,//2323200  -Turbo_L3
	8 //2419200  -Boost
};

static unsigned int sm8350_pd_goplus[19] = {
	0,// 844800  -LowSVS
	1,// 960000  -SVS
	1,//1075200  -SVS
	1,//1190400  -SVS
	2,//1305600  -SVS_L1
	2,//1420800  -SVS_L1
	3,//1555200  -Nominal
	3,//1670400  -Nominal
	4,//1785600  -Nominal_L1
	4,//1900800  -Nominal_L1
	4,//2035200  -Nominal_L1
	5,//2150400  -Turbo
	6,//2265600  -Turbo_L1
	7,//2380800  -Turbo_L3
	7,//2496000  -Turbo_L3
	7,//2592000  -Turbo_L3
	7,//2688000  -Turbo_L3
	8,//2764800  -Boost
	8//28416000  -Boost
};

// Power Domain just configuration for SM8450,
// Table need refer to sm8450 clock_plan datasheet.
static unsigned int sm8450_cluster_pd[MAX_CLUSTER] = {15, 18, 21};
static unsigned int sm8450_pd_sliver[15] = {
	0,//307200   -LowSVS
	0,//403200   -LowSVS
	0,//518400   -LowSVS
	0,//614400   -LowSVS
	1,//729600   -SVS
	1,//844800   -SVS
	2,//960000   -SVS_L1
	2,//1075200  -SVS_L1
	3,//1171200  -Nominal
	3,//1267200  -Nominal
	3,//1363200  -Nominal
	4,//1478400  -Nominal_L1
	5,//1574400  -Turbo
	6,//1689600  -Turbo_L1
	6 //1785600  -Turbo_L1
};

static unsigned int sm8450_pd_golden[18] = {
	0,//633600   -LowSVS
	1,//768000   -SVS
	1,//883200   -SVS
	2,//998400   -SVS_L1
	2,//1113600  -SVS_L1
	3,//1209600  -Nominal
	3,//1324800  -Nominal
	4,//1440000  -Nominal_L1
	4,//1555200  -Nominal_L1
	4,//1651200  -Nominal_L1
	5,//1766400  -Turbo
	6,//1881600  -Turbo_L1
	6,//1996800  -Turbo_L1
	7,//2112000  -Turbo_L3
	7,//2227200  -Turbo_L3
	7,//2342400  -Turbo_L3
	8,//2419200  -Boost
	9 //2496000  -BoostPlus
};

static unsigned int sm8450_pd_goplus[21] = {
	0,//808400   -LowSVS
	1,//940800   -SVS
	1,//1056000  -SVS
	1,//1171200  -SVS
	2,//1286400  -SVS_L1
	2,//1401600  -SVS_L1
	3,//1497600  -Nominal
	3,//1612800  -Nominal
	3,//1728000  -Nominal
	4,//1843200  -Nominal_L1
	4,//1958400  -Nominal_L1
	4,//2054400  -Nominal_L1
	5,//2169600  -Turbo
	6,//2284800  -Turbo_L1
	6,//2400000  -Turbo_L1
	7,//2515200  -Turbo_L3
	7,//2630400  -Turbo_L3
	7,//2726400  -Turbo_L3
	7,//2822400  -Turbo_L3
	8,//2841600  -Boost
	9 //2995200  -BoostPlus
};

extern unsigned int update_power_effiency_lock(struct cpufreq_policy *policy, unsigned int freq, unsigned int loadadj_freq);

#endif // _CPUFREQ_EFFIENCY_H
