/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "sensor_cmd.h"
#ifdef _OPLUS_SENSOR_HUB_VI
//cmd for kernel to scp
int oplus_enable_to_hub(uint8_t handle, int enabledisable)
{
    return sensor_enable_to_hub(handle,enabledisable);
}

int oplus_set_delay_to_hub(uint8_t handle, unsigned int delayms)
{
    return sensor_set_delay_to_hub(handle,delayms);
}

int oplus_batch_to_hub(uint8_t handle,
    int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    return sensor_batch_to_hub(handle,flag,samplingPeriodNs,maxBatchReportLatencyNs);
}

int oplus_flush_to_hub(uint8_t handle)
{
    return sensor_flush_to_hub(handle);
}
#else
extern int mtk_nanohub_enable_to_hub(uint8_t sensor_id, int enabledisable);
extern int mtk_nanohub_batch_to_hub(uint8_t sensor_id,
        int flag, int64_t samplingPeriodNs,
        int64_t maxBatchReportLatencyNs);
extern int mtk_nanohub_flush_to_hub(uint8_t sensor_id);
int oplus_enable_to_hub(uint8_t handle, int enabledisable)
{
    return  mtk_nanohub_enable_to_hub(handle,enabledisable);
}

int oplus_set_delay_to_hub(uint8_t handle, unsigned int delayms)
{
    return 0;
}

int oplus_batch_to_hub(uint8_t handle,
    int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    return mtk_nanohub_batch_to_hub(handle,flag,samplingPeriodNs,maxBatchReportLatencyNs);
}

int oplus_flush_to_hub(uint8_t handle)
{
    return  mtk_nanohub_flush_to_hub(handle);
}
#endif


