/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#ifndef SENSOR_DEVINFO_H
#define SENSOR_DEVINFO_H

typedef struct {//old struct
    int prox_ne;
    int prox_sw;
    int ges_offset_n;
    int ges_offset_e;
    int ges_offset_s;
    int ges_offset_w;
    int GsensorData[3];
    int prox_offset_L;
    int prox_offset_H;
    int gain_als;
    int GyroData[3];
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
            int dump1;
            int dump2;
        };
        int ps_data[6];
    };
	int baro_cali_offset;
	int nReserve[42];
} sensor_cali_file_v1_t;

typedef struct {//new struct
    union {
        struct {
            int ps0_offset;
            int ps0_mean;
            int ps0_distance_delta;
            int ps1_offset;
            int ps1_mean;
            int ps1_distance_delta;
            int double_ps_reserve[4];
        };
        struct {
            int ps_low_offset;
            int ps_normal_offset;
            int ps_low;
            int ps_normal;
            int underlcd_ps_reserve[6];
        };
        int ps_data[10];
    };
    int als_gain;
    int als_reserve[9];
    int gsensor_data[3];
    int gsensor_reserve[7];
    int gyro_data[3];
    int gyro_reserve[7];
    int cct_cali_data[6];
    int cct_reserve[4];
    int rear_als_gain;
    int rear_als_reserve[9];
	int baro_cali_offset;
	int nReserve[13];
} sensor_cali_file_v2_t;


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
	int baro_cali_offset;
};

struct oplus_als_cali_data {
    int red_max_lux;
    int green_max_lux;
    int blue_max_lux;
    int white_max_lux;
    int cali_coe;
    int row_coe;
    struct proc_dir_entry         *proc_oplus_als;
};

enum {
    OPLUS_ACTION_RW_REGISTER = 110,
    OPLUS_ACTION_SCP_SYNC_UTC,
    OPLUS_ACTION_SCP_DEVICE_INFO,
    OPLUS_ACTION_SCP_SYNC_CALI_DATA,
    OPLUS_ACTION_CONFIG_REG,
    OPLUS_ACTION_SELF_TEST,
    OPLUS_ACTION_SET_FACTORY_MODE,
    OPLUS_ACTION_SET_LCD_INFO,
};

enum sensor_mode {
    PS_FACTORY_MODE = 1,
    PS_NORMAL_MODE,
    GSENSOR_FACTORY_MODE,
    GSENSOR_NORMAL_MODE,
    CCT_FACTORY_MODE,
    CCT_NORMAL_MODE,
};

enum light_sensor_type {
    NORMAL_LIGHT_TYPE = 1,
    UNDER_SCREEN_LIGHT_TYPE,
    NORMAL_NEED_COMPENSATION,
};


enum sensor_id {
    OPLUS_ACCEL = 1,
    OPLUS_GYRO,
    OPLUS_MAG,
    OPLUS_LIGHT,
    OPLUS_PROXIMITY,
    OPLUS_SAR,
    OPLUS_CCT,
    OPLUS_BAROMETER,
    OPLUS_SARS,
    LAST_SENOSR,
    OPLUS_PICK_UP = LAST_SENOSR + 1,
    OPLUS_LUX_LOD,
    OPLUS_ALSPS_ARCH,
};

extern int get_sensor_parameter(struct cali_data *data);
extern void update_sensor_parameter(void);
extern bool is_sensor_available(char *name);
extern int oplus_send_selftest_cmd_to_hub(int sensorType, void *testresult);
extern int oplus_send_factory_mode_cmd_to_hub(int sensorType, int mode, void *result);
extern int get_light_sensor_type(void);
extern bool is_support_new_arch_func(void);
extern bool is_support_mtk_origin_cali_func(void);
extern int get_sensor_parameter_rear_als(struct cali_data *data);
#endif //SENSOR_DEVINFO_H
