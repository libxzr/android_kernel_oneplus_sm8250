/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#ifndef _SENSOR_CMD_H
#define _SENSOR_CMD_H

#ifdef _OPLUS_SENSOR_HUB_VI
#include <SCP_sensorHub.h>
#include <hwmsensor.h>
#else
#include "mtk_nanohub.h"
#include "hf_sensor_type.h"
#endif

//cmd for kernel to scp
int oplus_enable_to_hub(uint8_t handle, int enabledisable);
int oplus_set_delay_to_hub(uint8_t handle, unsigned int delayms);
int oplus_batch_to_hub(uint8_t handle,
    int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
int oplus_flush_to_hub(uint8_t handle);

#endif
