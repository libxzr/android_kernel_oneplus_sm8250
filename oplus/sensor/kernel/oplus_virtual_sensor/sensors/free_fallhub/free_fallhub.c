/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "free_fallhub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>

#define FREE_FALL_TAG                   "[free_fallhub] "
#define FREE_FALL_FUN(f)                pr_err(FREE_FALL_TAG"%s\n", __func__)
#define FREE_FALL_PR_ERR(fmt, args...)  pr_err(FREE_FALL_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define FREE_FALL_LOG(fmt, args...)     pr_err(FREE_FALL_TAG fmt, ##args)

static struct virtual_sensor_init_info free_fallhub_init_info;

static int free_fall_open_report_data(int open)
{
    return 0;
}

static int free_fall_enable_nodata(int en)
{
    FREE_FALL_LOG("free_fall enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_FREE_FALL, en);
}

static int free_fall_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_FREE_FALL, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int free_fall_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    free_fall_set_delay(samplingPeriodNs);
#endif

    FREE_FALL_LOG("free_fall: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n", samplingPeriodNs,
        maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_FREE_FALL, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int free_fall_flush(void)
{
    return oplus_flush_to_hub(ID_FREE_FALL);
}

static int free_fall_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_FREE_FALL;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.free_fall_data_t.free_fall_time;
    event.word[1] = input_event->oplus_data_t.free_fall_data_t.angle;
    event.word[2] = input_event->oplus_data_t.free_fall_data_t.report_count;
    return virtual_sensor_data_report(event);
}

static int free_fall_flush_report()
{
    return virtual_sensor_flush_report(ID_FREE_FALL);
}


static int free_fall_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    FREE_FALL_LOG("free_fall recv data, flush_action = %d, free_fall_time = %d, angle = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.free_fall_data_t.free_fall_time,
        event->oplus_data_t.free_fall_data_t.angle,
        event->oplus_data_t.free_fall_data_t.report_count, (int64_t)event->time_stamp);

    if (event->flush_action == DATA_ACTION) {
        free_fall_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = free_fall_flush_report();
    }

    return err;
}

static int free_fallhub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data = free_fall_open_report_data;
    ctl.enable_nodata = free_fall_enable_nodata;
    ctl.set_delay = free_fall_set_delay;
    ctl.batch = free_fall_batch;
    ctl.flush = free_fall_flush;
    ctl.report_data = free_fall_recv_data;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_FREE_FALL);
    if (err) {
        FREE_FALL_PR_ERR("register free_fall control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_FREE_FALL, free_fall_recv_data);
    if (err < 0) {
        FREE_FALL_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif

    return 0;
exit:
    return -1;
}

static int free_fallhub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info free_fallhub_init_info = {
    .name = "free_fall_hub",
    .init = free_fallhub_local_init,
    .uninit = free_fallhub_local_uninit,
};

static int __init free_fallhub_init(void)
{
    virtual_sensor_driver_add(&free_fallhub_init_info, ID_FREE_FALL);
    return 0;
}

static void __exit free_fallhub_exit(void)
{
    FREE_FALL_FUN();
}

module_init(free_fallhub_init);
module_exit(free_fallhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("zhq@oplus.com");

