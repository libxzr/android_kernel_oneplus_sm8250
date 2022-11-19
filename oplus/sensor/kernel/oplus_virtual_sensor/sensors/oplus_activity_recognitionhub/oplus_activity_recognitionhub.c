/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "oplus_activity_recognitionhub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>

#define OPLUS_ACTIVITY_RECOGNITION_TAG                   "[oplus_activity_recognition] "
#define OPLUS_ACTIVITY_RECOGNITION_FUN(f)                pr_err(OPLUS_ACTIVITY_RECOGNITION_TAG"%s\n", __func__)
#define OPLUS_ACTIVITY_RECOGNITION_PR_ERR(fmt, args...)  pr_err(OPLUS_ACTIVITY_RECOGNITION_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define OPLUS_ACTIVITY_RECOGNITION_LOG(fmt, args...)     pr_err(OPLUS_ACTIVITY_RECOGNITION_TAG fmt, ##args)

static struct virtual_sensor_init_info oplus_activity_recognitionhub_init_info;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static struct wakeup_source oplus_activity_recognition_wake_lock;
#else
static struct wakeup_source *oplus_activity_recognition_wake_lock = NULL;
#endif

static int oplus_activity_recognition_open_report_data(int open)
{
    return 0;
}

static int oplus_activity_recognition_enable_nodata(int en)
{
    OPLUS_ACTIVITY_RECOGNITION_LOG("oplus_activity_recognition enable nodata, en = %d\n", en);
    return oplus_enable_to_hub(ID_OPLUS_ACTIVITY_RECOGNITION, en);
}

static int oplus_activity_recognition_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_OPLUS_ACTIVITY_RECOGNITION, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int oplus_activity_recognition_batch(int flag, int64_t samplingPeriodNs,
    int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    oplus_activity_recognition_set_delay(samplingPeriodNs);
#endif

    OPLUS_ACTIVITY_RECOGNITION_LOG("oplus_activity_recognition: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs, maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_OPLUS_ACTIVITY_RECOGNITION, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int oplus_activity_recognition_flush(void)
{
    return oplus_flush_to_hub(ID_OPLUS_ACTIVITY_RECOGNITION);
}

static int oplus_activity_recognition_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;
    memset(&event, 0, sizeof(struct oplus_sensor_event));
    event.handle = ID_OPLUS_ACTIVITY_RECOGNITION;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.oplus_activity_recognition_event.motion_count;
    event.word[1] = input_event->oplus_data_t.oplus_activity_recognition_event.motion_ith;
    event.word[2] = input_event->oplus_data_t.oplus_activity_recognition_event.incar_state;
    event.word[3] = input_event->oplus_data_t.oplus_activity_recognition_event.activity_mode;
    event.word[4] = input_event->oplus_data_t.oplus_activity_recognition_event.delta_time;
    return virtual_sensor_data_report(event);
}

static int oplus_activity_recognition_flush_report()
{
    return virtual_sensor_flush_report(ID_OPLUS_ACTIVITY_RECOGNITION);
}

static int oplus_activity_recognition_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    OPLUS_ACTIVITY_RECOGNITION_LOG("oplus_activity_recognition recv data, flush_action = %d, motion_count = %d, motion_ith = %d, incar_state = %d, activity_mode = %d, delta_time = %d, timestamp = %lld\n",
        event->flush_action,
        event->oplus_data_t.oplus_activity_recognition_event.motion_count,
        event->oplus_data_t.oplus_activity_recognition_event.motion_ith,
        event->oplus_data_t.oplus_activity_recognition_event.incar_state,
        event->oplus_data_t.oplus_activity_recognition_event.activity_mode,
        event->oplus_data_t.oplus_activity_recognition_event.delta_time,
        (int64_t)event->time_stamp);

    if (event->flush_action == DATA_ACTION) {
        /*hold 100 ms timeout wakelock*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
        __pm_wakeup_event(&oplus_activity_recognition_wake_lock, msecs_to_jiffies(100));
#else
        __pm_wakeup_event(oplus_activity_recognition_wake_lock, msecs_to_jiffies(100));
#endif
        oplus_activity_recognition_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = oplus_activity_recognition_flush_report();
    }

    return err;
}

static int oplus_activity_recognitionhub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data = oplus_activity_recognition_open_report_data;
    ctl.enable_nodata = oplus_activity_recognition_enable_nodata;
    ctl.set_delay = oplus_activity_recognition_set_delay;
    ctl.batch = oplus_activity_recognition_batch;
    ctl.flush = oplus_activity_recognition_flush;
    ctl.report_data = oplus_activity_recognition_recv_data;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#ifdef OPLUS_FEATURE_SENSOR_ALGORITHM
    ctl.is_support_wake_lock = true;
#endif
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#ifdef OPLUS_FEATURE_SENSOR_ALGORITHM
    ctl.is_support_wake_lock = true;
#endif
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_OPLUS_ACTIVITY_RECOGNITION);
    if (err) {
        OPLUS_ACTIVITY_RECOGNITION_PR_ERR("register oplus_activity_recognition control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_OPLUS_ACTIVITY_RECOGNITION,
            oplus_activity_recognition_recv_data);
    if (err < 0) {
        OPLUS_ACTIVITY_RECOGNITION_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
    wakeup_source_init(&oplus_activity_recognition_wake_lock, "oplus_activity_recognition_wake_lock");
#else
    oplus_activity_recognition_wake_lock = wakeup_source_register(NULL,
            "oplus_activity_recognition_wake_lock");
#endif
    OPLUS_ACTIVITY_RECOGNITION_LOG("oplus_activity_recognitionhub_local_init done.");
    return 0;
exit:
    return -1;
}

static int oplus_activity_recognitionhub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info oplus_activity_recognitionhub_init_info = {
    .name = "oplus_activity_recognition_hub",
    .init = oplus_activity_recognitionhub_local_init,
    .uninit = oplus_activity_recognitionhub_local_uninit,
};

static int __init oplus_activity_recognitionhub_init(void)
{
    virtual_sensor_driver_add(&oplus_activity_recognitionhub_init_info, ID_OPLUS_ACTIVITY_RECOGNITION);
    return 0;
}

static void __exit oplus_activity_recognitionhub_exit(void)
{
    OPLUS_ACTIVITY_RECOGNITION_FUN();
}

module_init(oplus_activity_recognitionhub_init);
module_exit(oplus_activity_recognitionhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
