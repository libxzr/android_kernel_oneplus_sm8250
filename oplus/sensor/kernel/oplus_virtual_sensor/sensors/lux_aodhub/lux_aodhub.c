/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "lux_aodhub.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>

#define LUX_AOD_TAG                         "[lux_aodhub] "
#define LUX_AOD_FUN(f)                      pr_err(LUX_AOD_TAG"%s\n", __func__)
#define LUX_AOD_PR_ERR(fmt, args...)        pr_err(LUX_AOD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define LUX_AOD_KERNEL_LOG(fmt, args...)    pr_err(LUX_AOD_TAG fmt, ##args)

static struct virtual_sensor_init_info lux_aodhub_init_info;

static int lux_aod_open_report_data(int open)
{
    return 0;
}

static int lux_aod_enable_nodata(int en)
{
    LUX_AOD_KERNEL_LOG("enable nodata, en = %d\n", en);

    return oplus_enable_to_hub(ID_LUX_AOD, en);
}

static int lux_aod_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_LUX_AOD, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int lux_aod_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    lux_aod_set_delay(samplingPeriodNs);
#endif

    LUX_AOD_KERNEL_LOG("samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n", samplingPeriodNs,
        maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_LUX_AOD, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int lux_aod_flush(void)
{
    return oplus_flush_to_hub(ID_LUX_AOD);
}

static int lux_aod_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_LUX_AOD;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.lux_aod_event.state;
    event.word[1] = input_event->oplus_data_t.lux_aod_event.report_count;

    return virtual_sensor_data_report(event);
}

static int lux_aod_flush_report()
{
    return virtual_sensor_flush_report(ID_LUX_AOD);
}

static int lux_aod_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    LUX_AOD_KERNEL_LOG("recv data, flush_action = %d, state = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.lux_aod_event.state,
        event->oplus_data_t.lux_aod_event.report_count, (int64_t)event->time_stamp);


    if (event->flush_action == DATA_ACTION) {
        lux_aod_data_report(event);
    } else if (event->flush_action == FLUSH_ACTION) {
        err = lux_aod_flush_report();
    }

    return err;
}

static int  lux_aodhub_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data =  lux_aod_open_report_data;
    ctl.enable_nodata =  lux_aod_enable_nodata;
    ctl.set_delay =  lux_aod_set_delay;
    ctl.batch =  lux_aod_batch;
    ctl.flush =  lux_aod_flush;
    ctl.report_data = lux_aod_recv_data;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_LUX_AOD);
    if (err) {
        LUX_AOD_KERNEL_LOG("register lux_aod control path err\n");
        goto exit;
    }
#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_LUX_AOD, lux_aod_recv_data);
    if (err < 0) {
        LUX_AOD_KERNEL_LOG("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
#endif

    return 0;
exit:
    return -1;
}

static int lux_aodhub_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info lux_aodhub_init_info = {
    .name = "lux_aod_hub",
    .init = lux_aodhub_local_init,
    .uninit = lux_aodhub_local_uninit,
};

static int __init lux_aodhub_init(void)
{
    virtual_sensor_driver_add(&lux_aodhub_init_info, ID_LUX_AOD);
    return 0;
}

static void __exit  lux_aodhub_exit(void)
{
    LUX_AOD_FUN();
}

module_init(lux_aodhub_init);
module_exit(lux_aodhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LUX_AODHUB driver");
