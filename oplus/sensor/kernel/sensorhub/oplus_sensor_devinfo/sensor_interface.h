/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H
#include <linux/types.h>

#define DEV_TAG					 "[sensor_devinfo] "
#define DEVINFO_LOG(fmt, args...)   pr_err(DEV_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

struct als_info{
	uint16_t brightness;
	uint16_t dc_mode;
	bool use_lb_algo;
};

struct cali_data {
	int acc_data[6];
	int gyro_data[6];
	union {
		struct {
			int ps0_offset;
			int ps0_value;
			int ps0_distance_delta;
			int ps1_offset;
			int ps1_value;
			int ps1_distance_delta;
		};
		struct {
			int ps_low_offset;
			int ps_normal_offset;
			int ps_low;
			int ps_normal;
			int nReserve[2];
		};
		int ps_cali_data[6];
	};
	int als_value;
	int als_factor;
	int cct_cali_data[6];
	int rear_als_value;
	int rear_als_factor;
};

struct sensorhub_interface {
	int (*send_factory_mode)(int sensor_type, int mode, void *result);
	int (*send_selft_test)(int sensor_type, void *result);
	int (*send_reg_config)(int sensor_type);
	int (*send_cfg)(struct cali_data* cali_data);
	int (*send_utc_time)(void);
	int (*send_lcdinfo)(struct als_info *lcd_info);
	void (*init_sensorlist)(void);
	bool (*is_sensor_available)(char *name);
};

void init_sensorhub_interface(struct sensorhub_interface **si);


#endif /*SENSOR_INTERFACE_H*/
