/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#ifndef __VIRTUAL_SENSOR_H__
#define __VIRTUAL_SENSOR_H__

//#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/atomic.h>
#include <linux/ioctl.h>
#include <linux/major.h>
#include <linux/version.h>
#include "oplus_sensor_attr.h"
#include "oplus_sensor_event.h"
#include "sensor_cmd.h"
#ifdef  _OPLUS_SENSOR_HUB_VI
#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <SCP_sensorHub.h>
#include "hwmsensor.h"
#else
#include "mtk_nanohub.h"
#include "hf_sensor_type.h"
#endif

#define OP_VIRTUAL_SENSOR_DELAY 0X01
#define OP_VIRTUAL_SENSOR_ENABLE    0X02
#define OP_VIRTUAL_SENSOR_GET_DATA  0X04

#define VIRTUAL_SENSOR_INVALID_VALUE -1
#define VIRTUAL_SENSOR_MISC_DEV_NAME  "m_virtual_sensor_misc"
/* ORIENTATION, GRV, GMRV, RV, LA, GRAVITY, UNCALI_GYRO, UNCALI_MAG, PDR */
enum virtual_sensor_handle {
    camera_protect,
    free_fall,
    pickup_detect,
    fp_display,
    lux_aod,
    pedo_minute,
    #ifdef CONFIG_OPLUS_FEATURE_TP_GESTURE
    tp_gesture,
    #endif
    #ifdef CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION
    oplus_activity_recognition,
    #endif //CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION

    #ifdef CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
    elevator_detect,
    #endif //CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
#ifdef CONFIG_OPLUS_FEATURE_SENSOR_MONITOR
    sensor_monitor,
#endif
    max_virtual_sensor_support,
};

struct virtual_sensor_control_path {
    int (*open_report_data)(int open);  /* open data rerport to HAL */
    int (*enable_nodata)(int en);  /* only enable not report event to HAL */
    int (*set_delay)(u64 delay);
    int (*batch)(int flag,
        int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
    int (*flush)(void);/* open data rerport to HAL */
    int (*access_data_fifo)(void);/* version2.used for flush operate */
    bool is_report_input_direct;
    bool is_support_batch;/* version2.used for batch mode support flag */
    bool is_support_wake_lock;
    int (*virtual_sensor_calibration)(int type, int cali[3]);/* v3  factory API1 */
    int (*report_data)(struct data_unit_t *event, void *reserved);
};

struct virtual_sensor_init_info {
    char *name;
    int (*init)(void);
    int (*uninit)(void);
};


struct virtual_sensor_drv_obj {
    void *self;
    int polling;
    int (*virtual_sensor_operate)(void *self, uint32_t command, void *buff_in,
        int size_in, void *buff_out, int size_out, int *actualout);
};

struct virtual_sensor_control_context {
    struct virtual_sensor_control_path virtual_sensor_ctl;
    bool is_active_nodata;
    bool is_active_data;
    bool is_first_data_after_enable;
    bool is_polling_run;
    bool is_batch_enable;
    int power;
    int enable;
    int64_t delay_ns;
    int64_t latency_ns;
};
struct virtual_sensor_context {
    struct oplus_sensor_attr_t mdev;
    struct work_struct report;
    struct mutex virtual_sensor_op_mutex;
    atomic_t trace;
    struct virtual_sensor_control_context virtual_sensor_context[max_virtual_sensor_support];
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
    struct wakeup_source ws[max_virtual_sensor_support];
    #else
    struct wakeup_source* ws[max_virtual_sensor_support];
    #endif
    char *wake_lock_name[max_virtual_sensor_support];
};

/* driver API for internal */
/* extern int virtual_sensor_enable_nodata(int enable); */
/* extern int virtual_sensor_attach(struct virtual_sensor_drv_obj *obj); */
/* driver API for third party vendor */

/* for auto detect */
int virtual_sensor_driver_add(struct virtual_sensor_init_info *obj, int handle);
int virtual_sensor_register_control_path(struct virtual_sensor_control_path *ctl,
    int handle);

extern int virtual_sensor_data_report(struct oplus_sensor_event event);
extern int virtual_sensor_flush_report(int handle);
#endif  //__VIRTUAL_SENSOR_H__

