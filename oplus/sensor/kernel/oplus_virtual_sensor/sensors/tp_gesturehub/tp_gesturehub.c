/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "tp_gesturehub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>

#define TP_GESTURE_TAG                   "[tp_gesturehub] "
#define TP_GESTURE_FUN(f)                pr_err(TP_GESTURE_TAG"%s\n", __func__)
#define TP_GESTURE_PR_ERR(fmt, args...)  pr_err(TP_GESTURE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define TP_GESTURE_LOG(fmt, args...)     pr_err(TP_GESTURE_TAG fmt, ##args)

static struct virtual_sensor_init_info tp_gesturehub_init_info;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static struct wakeup_source tp_gesture_wake_lock;
#else
static struct wakeup_source *tp_gesture_wake_lock = NULL;
#endif

static int tp_gesture_open_report_data(int open)
{
    return 0;
}

static int tp_gesture_enable_nodata(int en)
{
    TP_GESTURE_LOG("tp_gesture enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_TP_GESTURE, en);
}

static int tp_gesture_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_TP_GESTURE, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int tp_gesture_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    tp_gesture_set_delay(samplingPeriodNs);
#endif

    TP_GESTURE_LOG("tp_gesture: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs, maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_TP_GESTURE, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int tp_gesture_flush(void)
{
    return oplus_flush_to_hub(ID_TP_GESTURE);
}

static int tp_gesture_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_TP_GESTURE;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.tp_gesture_data_t.value;
    event.word[1] = input_event->oplus_data_t.tp_gesture_data_t.report_count;
    return virtual_sensor_data_report(event);
}

static int  tp_gesture_flush_report()
{
    return virtual_sensor_flush_report(ID_TP_GESTURE);
}

static int tp_gesture_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    TP_GESTURE_LOG("tp_gesture recv data, flush_action = %d, value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action,
        event->oplus_data_t.tp_gesture_data_t.value,
        event->oplus_data_t.tp_gesture_data_t.report_count,
        (int64_t)event->time_stamp);

    if (event->flush_action == DATA_ACTION) {
        /*hold 100 ms timeout wakelock*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
        __pm_wakeup_event(&tp_gesture_wake_lock, msecs_to_jiffies(100));
#else
        __pm_wakeup_event(tp_gesture_wake_lock, msecs_to_jiffies(100));
#endif
        tp_gesture_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = tp_gesture_flush_report();
    }

    return err;
}

static int tp_gesturehub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data = tp_gesture_open_report_data;
    ctl.enable_nodata = tp_gesture_enable_nodata;
    ctl.set_delay = tp_gesture_set_delay;
    ctl.batch = tp_gesture_batch;
    ctl.flush = tp_gesture_flush;
    ctl.report_data = tp_gesture_recv_data;
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

    err = virtual_sensor_register_control_path(&ctl, ID_TP_GESTURE);
    if (err) {
        TP_GESTURE_PR_ERR("register tp_gesture control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_TP_GESTURE, tp_gesture_recv_data);
    if (err < 0) {
        TP_GESTURE_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
    wakeup_source_init(&tp_gesture_wake_lock, "tp_gesture_wake_lock");
#else
    tp_gesture_wake_lock = wakeup_source_register(NULL, "tp_gesture_wake_lock");
#endif
    return 0;
exit:
    return -1;
}

static int tp_gesturehub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info tp_gesturehub_init_info = {
    .name = "tp_gesture_hub",
    .init = tp_gesturehub_local_init,
    .uninit = tp_gesturehub_local_uninit,
};

static int __init tp_gesturehub_init(void)
{
    virtual_sensor_driver_add(&tp_gesturehub_init_info, ID_TP_GESTURE);
    return 0;
}

static void __exit tp_gesturehub_exit(void)
{
    TP_GESTURE_FUN();
}

module_init(tp_gesturehub_init);
module_exit(tp_gesturehub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("zhq@oplus.com");

