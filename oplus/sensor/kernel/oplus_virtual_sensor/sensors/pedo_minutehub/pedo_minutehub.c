/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "pedo_minutehub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>

#define PEDO_MINUTE_TAG                   "[pedo_minute] "
#define PEDO_MINUTE_FUN(f)                pr_err(PEDO_MINUTE_TAG"%s\n", __func__)
#define PEDO_MINUTE_PR_ERR(fmt, args...)  pr_err(PEDO_MINUTE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PEDO_MINUTE_LOG(fmt, args...)     pr_err(PEDO_MINUTE_TAG fmt, ##args)

static struct virtual_sensor_init_info pedo_minutehub_init_info;


static int pedo_minute_open_report_data(int open)
{
    return 0;
}

static int pedo_minute_enable_nodata(int en)
{
    PEDO_MINUTE_LOG("pedo_minute enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_PEDO_MINUTE, en);
}

static int pedo_minute_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_PEDO_MINUTE, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int pedo_minute_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    pedo_minute_set_delay(samplingPeriodNs);
#endif

    PEDO_MINUTE_LOG("pedo_minute: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs, maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_PEDO_MINUTE, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int pedo_minute_flush(void)
{
    return oplus_flush_to_hub(ID_PEDO_MINUTE);
}

static int pedo_minute_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_PEDO_MINUTE;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.pedo_minute_event.step_count;
    event.word[1] = input_event->oplus_data_t.pedo_minute_event.report_count;
    event.word[2] = input_event->oplus_data_t.pedo_minute_event.move_status;
    event.word[3] = input_event->oplus_data_t.pedo_minute_event.time_gap;
    event.word[4] = input_event->oplus_data_t.pedo_minute_event.step_run_count;
    event.word[5] = input_event->oplus_data_t.pedo_minute_event.step_walk_count;
    return virtual_sensor_data_report(event);
}

static int pedo_minute_flush_report()
{
    return virtual_sensor_flush_report(ID_PEDO_MINUTE);
}

static int pedo_minute_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    PEDO_MINUTE_LOG("pedo_minute recv data, flush_action = %d, step_count value = %d,timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.pedo_minute_event.step_count, (int64_t)event->time_stamp);


    if (event->flush_action == DATA_ACTION) {
        pedo_minute_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = pedo_minute_flush_report();
    }

    return err;
}


static int pedo_minutehub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    PEDO_MINUTE_PR_ERR("pedo_minutehub_local_init start.\n");

    ctl.open_report_data = pedo_minute_open_report_data;
    ctl.enable_nodata = pedo_minute_enable_nodata;
    ctl.set_delay = pedo_minute_set_delay;
    ctl.batch = pedo_minute_batch;
    ctl.flush = pedo_minute_flush;
    ctl.report_data = pedo_minute_recv_data;
    ctl.is_support_wake_lock = true;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_PEDO_MINUTE);
    if (err) {
        PEDO_MINUTE_PR_ERR("register pedo_minute control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI

    err = scp_sensorHub_data_registration(ID_PEDO_MINUTE, pedo_minute_recv_data);
    if (err < 0) {
        PEDO_MINUTE_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif
    PEDO_MINUTE_PR_ERR("pedo_minutehub_local_init over.\n");

    return 0;
exit:
    return -1;
}


static int pedo_minutehub_local_uninit(void)
{
    return 0;
}


static struct virtual_sensor_init_info pedo_minutehub_init_info = {
    .name = "pedo_minute_hub",
    .init = pedo_minutehub_local_init,
    .uninit = pedo_minutehub_local_uninit,
};


static int __init pedo_minutehub_init(void)
{
    virtual_sensor_driver_add(&pedo_minutehub_init_info, ID_PEDO_MINUTE);
    return 0;
}

static void __exit pedo_minutehub_exit(void)
{
    PEDO_MINUTE_FUN();
}

module_init(pedo_minutehub_init);
module_exit(pedo_minutehub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
