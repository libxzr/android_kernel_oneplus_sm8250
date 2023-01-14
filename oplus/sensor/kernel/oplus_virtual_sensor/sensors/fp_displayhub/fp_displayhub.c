/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "fp_displayhub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>

#define FP_DISPLAY_TAG                   "[fp_displayhub] "
#define FP_DISPLAY_FUN(f)                pr_err(FP_DISPLAY_TAG"%s\n", __func__)
#define FP_DISPLAY_PR_ERR(fmt, args...)  pr_err(FP_DISPLAY_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define FP_DISPLAY_LOG(fmt, args...)     pr_err(FP_DISPLAY_TAG fmt, ##args)

static struct virtual_sensor_init_info fp_displayhub_init_info;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static struct wakeup_source fp_dis_wake_lock;
#else
static struct wakeup_source *fp_dis_wake_lock = NULL;
#endif
static int fp_display_open_report_data(int open)
{
    return 0;
}

static int fp_display_enable_nodata(int en)
{
    FP_DISPLAY_LOG("enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_FP_DISPLAY, en);
}

static int fp_display_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_FP_DISPLAY, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int fp_display_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    fp_display_set_delay(samplingPeriodNs);
#endif

    FP_DISPLAY_LOG("samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs,
        maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_FP_DISPLAY, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int fp_display_flush(void)
{
    return oplus_flush_to_hub(ID_FP_DISPLAY);
}

static int fp_display_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_FP_DISPLAY;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.fp_display_data_t.value;
    event.word[1] = input_event->oplus_data_t.fp_display_data_t.report_count;
    return virtual_sensor_data_report(event);
}

static int fp_display_flush_report()
{
    return virtual_sensor_flush_report(ID_FP_DISPLAY);
}

static int fp_display_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    FP_DISPLAY_LOG("recv data, flush_action = %d, value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.fp_display_data_t.value,
        event->oplus_data_t.fp_display_data_t.report_count,
        (int64_t)event->time_stamp);


    if (event->flush_action == DATA_ACTION) {
        /*hold 100 ms timeout wakelock*/
        #if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
        __pm_wakeup_event(&fp_dis_wake_lock, msecs_to_jiffies(100));
        #else
        __pm_wakeup_event(fp_dis_wake_lock, msecs_to_jiffies(100));
        #endif
        fp_display_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = fp_display_flush_report();
    }

    return err;
}

static int fp_displayhub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data = fp_display_open_report_data;
    ctl.enable_nodata = fp_display_enable_nodata;
    ctl.set_delay = fp_display_set_delay;
    ctl.batch = fp_display_batch;
    ctl.flush = fp_display_flush;
    ctl.report_data = fp_display_recv_data;

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

    err = virtual_sensor_register_control_path(&ctl, ID_FP_DISPLAY);

    if (err) {
        FP_DISPLAY_LOG("register fp_display control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_FP_DISPLAY, fp_display_recv_data);

    if (err < 0) {
        FP_DISPLAY_LOG("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
    wakeup_source_init(&fp_dis_wake_lock, "fp_dis_wake_lock");
    #else
    fp_dis_wake_lock = wakeup_source_register(NULL,"fp_dis_wake_lock");
    #endif

    return 0;
exit:
    return -1;
}

static int fp_displayhub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info fp_displayhub_init_info = {
    .name = "fp_display_hub",
    .init = fp_displayhub_local_init,
    .uninit = fp_displayhub_local_uninit,
};

static int __init fp_displayhub_init(void)
{
    virtual_sensor_driver_add(&fp_displayhub_init_info, ID_FP_DISPLAY);
    return 0;
}

static void __exit fp_displayhub_exit(void)
{
    FP_DISPLAY_FUN();
}

module_init(fp_displayhub_init);
module_exit(fp_displayhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
