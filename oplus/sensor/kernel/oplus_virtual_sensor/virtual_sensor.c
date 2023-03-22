/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) "<VIRTUAL_SENSOR> " fmt

#include "virtual_sensor.h"

static struct virtual_sensor_context *virtual_sensor_context_obj;
static struct virtual_sensor_init_info *virtual_sensor_init_list[max_virtual_sensor_support] = { 0 };

void oplus_init_sensor_state(struct SensorState *mSensorState)
{
    #ifndef _OPLUS_SENSOR_HUB_VI
    struct SensorState *p = NULL;
    #endif
    mSensorState[SENSOR_TYPE_CAMERA_PROTECT].sensorType = SENSOR_TYPE_CAMERA_PROTECT;
    mSensorState[SENSOR_TYPE_CAMERA_PROTECT].rate = SENSOR_RATE_ONCHANGE;

    mSensorState[SENSOR_TYPE_FREE_FALL].sensorType = SENSOR_TYPE_FREE_FALL;
    mSensorState[SENSOR_TYPE_FREE_FALL].rate = SENSOR_RATE_ONCHANGE;

    mSensorState[SENSOR_TYPE_PICKUP_DETECT].sensorType = SENSOR_TYPE_PICKUP_DETECT;
    mSensorState[SENSOR_TYPE_PICKUP_DETECT].rate = SENSOR_RATE_ONCHANGE;

    mSensorState[SENSOR_TYPE_FP_DISPLAY].sensorType = SENSOR_TYPE_FP_DISPLAY;
    mSensorState[SENSOR_TYPE_FP_DISPLAY].rate = SENSOR_RATE_ONCHANGE;


    mSensorState[SENSOR_TYPE_LUX_AOD].sensorType = SENSOR_TYPE_LUX_AOD;
    mSensorState[SENSOR_TYPE_LUX_AOD].rate = SENSOR_RATE_ONCHANGE;

#ifdef CONFIG_OPLUS_FEATURE_SENSOR_MONITOR
    mSensorState[SENSOR_TYPE_SENSOR_MONITOR].sensorType = SENSOR_TYPE_SENSOR_MONITOR;
    mSensorState[SENSOR_TYPE_SENSOR_MONITOR].rate = SENSOR_RATE_ONCHANGE;
#endif

    #ifdef CONFIG_OPLUS_FEATURE_TP_GESTURE
    mSensorState[SENSOR_TYPE_TP_GESTURE].sensorType = SENSOR_TYPE_TP_GESTURE;
    mSensorState[SENSOR_TYPE_TP_GESTURE].rate = SENSOR_RATE_ONCHANGE;
    #endif

    mSensorState[SENSOR_TYPE_PEDO_MINUTE].sensorType = SENSOR_TYPE_PEDO_MINUTE;

    #ifdef CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION
    mSensorState[SENSOR_TYPE_OPLUS_ACTIVITY_RECOGNITION].sensorType =
        SENSOR_TYPE_OPLUS_ACTIVITY_RECOGNITION;
    mSensorState[SENSOR_TYPE_OPLUS_ACTIVITY_RECOGNITION].rate = SENSOR_RATE_ONCHANGE;
    #endif //CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION

    #ifdef CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
    mSensorState[SENSOR_TYPE_ELEVATOR_DETECT].sensorType = SENSOR_TYPE_ELEVATOR_DETECT;
    mSensorState[SENSOR_TYPE_ELEVATOR_DETECT].rate = SENSOR_RATE_ONCHANGE;
    #endif //CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT

    #ifndef _OPLUS_SENSOR_HUB_VI
    //mtk 2.0 need to add gain info
    p = &mSensorState[SENSOR_TYPE_CAMERA_PROTECT];
    p->gain = 1;
    strlcpy(p->name, "cameraprotect", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

    p = &mSensorState[SENSOR_TYPE_FREE_FALL];
    p->gain = 1;
    strlcpy(p->name, "free_fall", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

    p = &mSensorState[SENSOR_TYPE_PICKUP_DETECT];
    p->gain = 1;
    strlcpy(p->name, "pickup", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

    p = &mSensorState[SENSOR_TYPE_FP_DISPLAY];
    p->gain = 1;
    strlcpy(p->name, "fpdisplay", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

    p = &mSensorState[SENSOR_TYPE_LUX_AOD];
    p->gain = 1;
    strlcpy(p->name, "lux_aod", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

#ifdef CONFIG_OPLUS_FEATURE_SENSOR_MONITOR
    p = &mSensorState[SENSOR_TYPE_SENSOR_MONITOR];
    p->gain = 1;
    strlcpy(p->name, "sensor_monitor", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));
#endif

    p = &mSensorState[SENSOR_TYPE_PEDO_MINUTE];
    p->gain = 1;
    strlcpy(p->name, "pedominute", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));

    #ifdef CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION
    p = &mSensorState[SENSOR_TYPE_OPLUS_ACTIVITY_RECOGNITION];
    p->gain = 1;
    strlcpy(p->name, "acrecongnition", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));
    #endif //CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION

    #ifdef CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
    p = &mSensorState[SENSOR_TYPE_ELEVATOR_DETECT];
    p->gain = 1;
    strlcpy(p->name, "elevator_detect", sizeof(p->name));
    strlcpy(p->vendor, "oplus", sizeof(p->vendor));
    #endif //CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT

    pr_err("set gain = 1----\n");
    #endif
}

static struct virtual_sensor_context *virtual_sensor_context_alloc_object(void)
{
    int index = 0;
    struct virtual_sensor_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

    pr_debug("virtual_sensor_context_alloc_object++++\n");
    if (!obj) {
        pr_err("Alloc virtual_sensor object error!\n");
        return NULL;
    }
    mutex_init(&obj->virtual_sensor_op_mutex);
    for (index = camera_protect; index < max_virtual_sensor_support; ++index) {
        obj->virtual_sensor_context[index].is_first_data_after_enable = false;
        obj->virtual_sensor_context[index].is_polling_run = false;
        obj->virtual_sensor_context[index].is_batch_enable = false;
        obj->virtual_sensor_context[index].power = 0;
        obj->virtual_sensor_context[index].enable = 0;
        obj->virtual_sensor_context[index].delay_ns = -1;
        obj->virtual_sensor_context[index].latency_ns = -1;
    }
    pr_debug("virtual_sensor_context_alloc_object----\n");
    return obj;
}

static int handle_to_index(int handle)
{
    int index = -1;

    switch (handle) {
    case ID_CAMERA_PROTECT:
        index = camera_protect;
        break;
    case ID_FREE_FALL:
        index = free_fall;
        break;
    case ID_PICKUP_DETECT:
        index = pickup_detect;
        break;
    case ID_FP_DISPLAY:
        index = fp_display;
        break;
    case ID_LUX_AOD:
        index = lux_aod;
        break;
    case ID_PEDO_MINUTE:
        index = pedo_minute;
        break;
    #ifdef CONFIG_OPLUS_FEATURE_TP_GESTURE
    case ID_TP_GESTURE:
        index = tp_gesture;
        break;
    #endif
    #ifdef CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION
    case ID_OPLUS_ACTIVITY_RECOGNITION:
        index = oplus_activity_recognition;
        break;
    #endif //CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION

    #ifdef CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
    case ID_ELEVATOR_DETECT:
        index = elevator_detect;
        break;
    #endif //CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT
#ifdef CONFIG_OPLUS_FEATURE_SENSOR_MONITOR
    case ID_SENSOR_MONITOR:
        index = sensor_monitor;
        break;
#endif
    default:
        index = -1;
        pr_err("%s invalid handle:%d, index:%d\n", __func__,
            handle, index);
        return index;
    }
    pr_err("%s  handle:%d, index:%d\n", __func__,
        handle, index);
    return index;
}

void virtual_sensor_report_data(struct data_unit_t *data,int handle)
{
    int index = -1 ;
    struct virtual_sensor_context *cxt = virtual_sensor_context_obj;

    index = handle_to_index(handle);

    if(index != -1) {
        if ( NULL != cxt->virtual_sensor_context[index].virtual_sensor_ctl.report_data ) {
            cxt->virtual_sensor_context[index].virtual_sensor_ctl.report_data(data,NULL);
        } else {
            pr_err("not regiter report data");
        }
    }

}

#ifndef CONFIG_NANOHUB
static int virtual_sensor_enable_and_batch(int index)
{
    struct virtual_sensor_context *cxt = virtual_sensor_context_obj;
    int err;

    /* power on -> power off */
    if (cxt->virtual_sensor_context[index].power == 1 &&
        cxt->virtual_sensor_context[index].enable == 0) {
        pr_debug("VIRTUAL_SENSOR disable\n");
        /* turn off the power */
        err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata(0);
        if (err) {
            pr_err("virtual_sensor turn off power err = %d\n", err);
            return -1;
        }

        cxt->virtual_sensor_context[index].power = 0;
        cxt->virtual_sensor_context[index].delay_ns = -1;
        return 0;
    }
    /* power off -> power on */
    if (cxt->virtual_sensor_context[index].power == 0 &&
        cxt->virtual_sensor_context[index].enable == 1) {
        pr_debug("VIRTUAL_SENSOR power on\n");
        err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata(1);
        if (err) {
            pr_err("virtual_sensor turn on power err = %d\n", err);
            return -1;
        }

        cxt->virtual_sensor_context[index].power = 1;
    }
    /* rate change */
    if (cxt->virtual_sensor_context[index].power == 1 &&
        cxt->virtual_sensor_context[index].delay_ns >= 0) {
        pr_debug("VIRTUAL_SENSOR set batch\n");
        /* set ODR, fifo timeout latency */
        if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_support_batch) {
            err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch(0,
                    cxt->virtual_sensor_context[index].delay_ns,
                    cxt->virtual_sensor_context[index].latency_ns);
        } else {
            err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch(0,
                    cxt->virtual_sensor_context[index].delay_ns, 0);
        }
        if (err) {
            pr_err("virtual_sensor set batch(ODR) err %d\n", err);
            return -1;
        }
    }
    return 0;
}
#endif

static ssize_t virtual_sensor_store_active(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct virtual_sensor_context *cxt = virtual_sensor_context_obj;
    int err = 0, handle = -1, en = 0, index = -1;

    err = sscanf(buf, "%d,%d", &handle, &en);
    if (err < 0) {
        pr_err("virtual_sensor_store_active param error: err = %d\n",
            err);
        return err;
    }
    pr_debug("virtual_sensor_store_active handle=%d, en=%d\n", handle, en);
    index = handle_to_index(handle);
    if (index < 0) {
        pr_err("[%s] invalid handle\n", __func__);
        return -1;
    }

    if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata == NULL) {
        pr_debug("[%s] ctl not registered\n", __func__);
        return -1;
    }

    mutex_lock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
    if (en == 1) {
        cxt->virtual_sensor_context[index].enable = 1;
    } else if (en == 0) {
        cxt->virtual_sensor_context[index].enable = 0;
    } else {
        pr_err(" virtual_sensor_store_active error !!\n");
        err = -1;
        goto err_out;
    }
#ifdef CONFIG_NANOHUB
    if (cxt->virtual_sensor_context[index].enable == 1) {
        err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata(1);
        if (err) {
            pr_err("virtual_sensor turn on power err = %d\n", err);
            goto err_out;
        }
    } else {
        err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata(0);
        if (err) {
            pr_err("virtual_sensor turn off power err = %d\n", err);
            goto err_out;
        }
    }
#else
    err = virtual_sensor_enable_and_batch(index);
#endif
    pr_debug("%s done\n", __func__);
err_out:
    mutex_unlock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
    return err;
}

/*----------------------------------------------------------------------------*/
static ssize_t virtual_sensor_show_active(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    int enable_div[max_virtual_sensor_support];
    int index = 0;
    struct virtual_sensor_context *cxt = virtual_sensor_context_obj;

    for (index = camera_protect; index < max_virtual_sensor_support; ++index) {
        enable_div[index] =
            cxt->virtual_sensor_context[index].enable;
        pr_debug("virtual_sensor index:%d enable: %d\n",
            index, enable_div[index]);
    }

#ifdef CONFIG_OPLUS_FEATURE_SENSOR_MONITOR
    return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
        enable_div[camera_protect], enable_div[free_fall],
        enable_div[pickup_detect], enable_div[fp_display],
        enable_div[lux_aod], enable_div[sensor_monitor]);
#else
return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n",
        enable_div[camera_protect], enable_div[free_fall],
        enable_div[pickup_detect], enable_div[fp_display],
        enable_div[lux_aod]);
#endif
}

static ssize_t virtual_sensor_show_sensordevnum(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}


static ssize_t virtual_sensor_store_batch(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct virtual_sensor_context *cxt = virtual_sensor_context_obj;
    int index = -1, handle = 0, flag = 0, err = 0;
    int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

    err = sscanf(buf, "%d,%d,%lld,%lld",
            &handle, &flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
    if (err != 4) {
        pr_err("virtual_sensor_store_batch param error: err = %d\n",
            err);
        return err;
    }
    index = handle_to_index(handle);
    if (index < 0) {
        pr_err("[%s] invalid handle\n", __func__);
        return -1;
    }

    if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch == NULL) {
        pr_err("[%s] index = %d ctl not registered\n", __func__, index);
        return -1;
    }

    pr_err("handle %d, flag:%d, PeriodNs:%lld, LatencyNs: %lld\n",
        handle, flag, samplingPeriodNs, maxBatchReportLatencyNs);

    cxt->virtual_sensor_context[index].delay_ns = samplingPeriodNs;
    cxt->virtual_sensor_context[index].latency_ns = maxBatchReportLatencyNs;

    mutex_lock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
#ifdef CONFIG_NANOHUB
    if (cxt->virtual_sensor_context[index].delay_ns >= 0) {
        if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_support_batch) {
            err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch(0,
                    cxt->virtual_sensor_context[index].delay_ns,
                    cxt->virtual_sensor_context[index].latency_ns);
        } else {
            err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch(0,
                    cxt->virtual_sensor_context[index].delay_ns, 0);
        }
        if (err) {
            pr_err("virtual_sensor set batch(ODR) err %d\n", err);
            goto err_out;
        }
    } else {
        pr_info("batch state no need change\n");
    }
#else
    err = virtual_sensor_enable_and_batch(index);
#endif
    pr_err("%s done\n", __func__);
err_out:
    mutex_unlock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
    return err;
}

static ssize_t virtual_sensor_show_batch(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t virtual_sensor_store_flush(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct virtual_sensor_context *cxt = NULL;
    int index = -1, handle = 0, err = 0;

    err = kstrtoint(buf, 10, &handle);
    if (err != 0) {
        pr_err("virtual_sensor_store_flush param error: err = %d\n",
            err);
    }
    pr_debug("virtual_sensor_store_flush param: handle %d\n", handle);

    mutex_lock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
    cxt = virtual_sensor_context_obj;
    index = handle_to_index(handle);
    if (index < 0) {
        pr_err("[%s] invalid index\n", __func__);
        mutex_unlock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
        return  -1;
    }
    if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.flush != NULL) {
        err = cxt->virtual_sensor_context[index].virtual_sensor_ctl.flush();
    } else {
        pr_err("VIRTUAL_SENSOR OLD ARCH NOT SUPPORT COMMON VRS FLUSH\n");
    }
    if (err < 0) {
        pr_err("virtual_sensor enable flush err %d\n", err);
    }
    mutex_unlock(&virtual_sensor_context_obj->virtual_sensor_op_mutex);
    return err;
}

static ssize_t virtual_sensor_show_flush(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static int virtual_sensor_real_driver_init(void)
{
    int index = 0;
    int err = -1;

    pr_debug("virtual_sensor_real_driver_init +\n");
    for (index = 0; index < max_virtual_sensor_support; index++) {
        pr_debug("index = %d\n", index);
        if (virtual_sensor_init_list[index] != NULL) {
            pr_debug("virtual_sensor try to init driver %s\n",
                virtual_sensor_init_list[index]->name);
            err = virtual_sensor_init_list[index]->init();
            if (err == 0) {
                pr_debug("virtual_sensor real driver %s probe ok\n",
                    virtual_sensor_init_list[index]->name);
            }
        }
    }
    return err;
}

static int virtual_sensor_open(struct inode *inode, struct file *file)
{
    nonseekable_open(inode, file);
    return 0;
}

static ssize_t virtual_sensor_read(struct file *file, char __user *buffer,
    size_t count, loff_t *ppos)
{
    ssize_t read_cnt = 0;

    read_cnt = oplus_sensor_event_read(virtual_sensor_context_obj->mdev.minor,
            file, buffer, count, ppos);

    return read_cnt;
}

static unsigned int virtual_sensor_poll(struct file *file, poll_table *wait)
{
    return oplus_sensor_event_poll(virtual_sensor_context_obj->mdev.minor, file, wait);
}

static const struct file_operations virtual_sensor_fops = {
    .owner = THIS_MODULE,
    .open = virtual_sensor_open,
    .read = virtual_sensor_read,
    .poll = virtual_sensor_poll,
};

static int virtual_sensor_misc_init(struct virtual_sensor_context *cxt)
{
    int err = 0;

    cxt->mdev.minor = MINOR_NUM_FOR_VIRTUAL_SENSOR;
    cxt->mdev.name = VIRTUAL_SENSOR_MISC_DEV_NAME;
    cxt->mdev.fops = &virtual_sensor_fops;
    err = oplus_sensor_attr_register(&cxt->mdev);
    if (err) {
        pr_err("unable to register virtual_sensor misc device!!\n");
    }

    /* dev_set_drvdata(cxt->mdev.this_device, cxt); */
    return err;
}

DEVICE_ATTR(virtual_sensoractive, 0644, virtual_sensor_show_active, virtual_sensor_store_active);
DEVICE_ATTR(virtual_sensorbatch, 0644, virtual_sensor_show_batch, virtual_sensor_store_batch);
DEVICE_ATTR(virtual_sensorflush, 0644, virtual_sensor_show_flush, virtual_sensor_store_flush);
DEVICE_ATTR(virtual_sensordevnum, 0644, virtual_sensor_show_sensordevnum, NULL);

static struct attribute *virtual_sensor_attributes[] = {
    &dev_attr_virtual_sensoractive.attr,
    &dev_attr_virtual_sensorbatch.attr,
    &dev_attr_virtual_sensorflush.attr,
    &dev_attr_virtual_sensordevnum.attr,
    NULL
};

static struct attribute_group virtual_sensor_attribute_group = {
    .attrs = virtual_sensor_attributes
};

int virtual_sensor_register_control_path(struct virtual_sensor_control_path *ctl,
    int handle)
{
    struct virtual_sensor_context *cxt = NULL;
    int index = -1;

    if (NULL == ctl || NULL == ctl->set_delay
        || NULL == ctl->open_report_data
        || NULL == ctl->enable_nodata
        || NULL == ctl->batch || NULL == ctl->flush) {
        pr_err("virtual_sensor handle:%d register control path fail\n",
            handle);
        return -1;
    }
    pr_err("virtual_sensor handle:%d register control path start\n",
        handle);

    index = handle_to_index(handle);
    if (index < 0) {
        pr_err("[%s] invalid handle\n", __func__);
        return -1;
    }

    cxt = virtual_sensor_context_obj;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.set_delay =
        ctl->set_delay;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.open_report_data =
        ctl->open_report_data;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.enable_nodata =
        ctl->enable_nodata;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.report_data =
        ctl->report_data;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.batch = ctl->batch;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.flush = ctl->flush;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_support_batch =
        ctl->is_support_batch;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_report_input_direct =
        ctl->is_report_input_direct;
    cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_support_wake_lock =
        ctl->is_support_wake_lock;
    cxt->wake_lock_name[index] = kzalloc(64, GFP_KERNEL);
    if (!cxt->wake_lock_name[index]) {
        return -1;
    }
    sprintf(cxt->wake_lock_name[index], "virtual_sensor_wakelock-%d", index);
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
    wakeup_source_init(&cxt->ws[index], cxt->wake_lock_name[index]);
    #else
    cxt->ws[index] = wakeup_source_register(NULL, cxt->wake_lock_name[index]);
    #endif

    pr_err("virtual_sensor handle:%d register control path done\n",
        handle);
    return 0;
}

int virtual_sensor_data_report(struct oplus_sensor_event event)
{
    /* pr_debug("+virtual_sensor_data_report! %d, %d, %d, %d\n",x,y,z,status); */
    int err = 0;

    int index = -1;
    struct virtual_sensor_context *cxt = NULL;
    cxt = virtual_sensor_context_obj;

    /*memset(&event, 0, sizeof(struct sensor_event));

    event.handle = handle;
    event.flush_action = DATA_ACTION;
    event.time_stamp = nt;
    event.status = status;
    event.word[0] = x;
    event.word[1] = y;
    event.word[2] = z;
    event.word[3] = scalar;*/

    index = handle_to_index(event.handle);
    if (index < 0) {
        pr_err("[%s] invalid handle\n", __func__);
        return -1;
    }
    err = oplus_sensor_input_event(virtual_sensor_context_obj->mdev.minor, &event);
    if (cxt->virtual_sensor_context[index].virtual_sensor_ctl.open_report_data != NULL &&
        cxt->virtual_sensor_context[index].virtual_sensor_ctl.is_support_wake_lock) {
        pr_err("wake_lock index=%d\n", index);
        /*hold 100 ms timeout wakelock*/
        #if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
        __pm_wakeup_event(&cxt->ws[index], 250);
        #else
        __pm_wakeup_event(cxt->ws[index], 250);
        #endif
    }
    return err;
}

int virtual_sensor_flush_report(int handle)
{
    struct oplus_sensor_event event;
    int err = 0;

    memset(&event, 0, sizeof(struct oplus_sensor_event));
    pr_debug("flush\n");
    event.handle = handle;
    event.flush_action = FLUSH_ACTION;
    err = oplus_sensor_input_event(virtual_sensor_context_obj->mdev.minor, &event);
    return err;
}

static int virtual_sensor_probe(void)
{
    int err;

    pr_debug("+++++++++++++virtual_sensor_probe!!\n");

    virtual_sensor_context_obj = virtual_sensor_context_alloc_object();
    if (!virtual_sensor_context_obj) {
        err = -ENOMEM;
        pr_err("unable to allocate devobj!\n");
        goto exit_alloc_data_failed;
    }
    /* init real virtual_sensoreleration driver */
    err = virtual_sensor_real_driver_init();
    if (err) {
        pr_err("virtual_sensor real driver init fail\n");
        goto real_driver_init_fail;
    }
    /* add misc dev for sensor hal control cmd */
    err = virtual_sensor_misc_init(virtual_sensor_context_obj);
    if (err) {
        pr_err("unable to register virtual_sensor misc device!!\n");
        goto real_driver_init_fail;
    }
    err = sysfs_create_group(&virtual_sensor_context_obj->mdev.this_device->kobj,
            &virtual_sensor_attribute_group);
    if (err < 0) {
        pr_err("unable to create virtual_sensor attribute file\n");
        goto real_driver_init_fail;
    }
    kobject_uevent(&virtual_sensor_context_obj->mdev.this_device->kobj, KOBJ_ADD);

    pr_debug("----virtual_sensor_probe OK !!\n");
    return 0;

real_driver_init_fail:
    kfree(virtual_sensor_context_obj);
exit_alloc_data_failed:
    pr_debug("----virtual_sensor_probe fail !!!\n");
    return err;
}

static int virtual_sensor_remove(void)
{
    int err = 0;

    pr_debug("%s\n", __func__);

    sysfs_remove_group(&virtual_sensor_context_obj->mdev.this_device->kobj,
        &virtual_sensor_attribute_group);
    err = oplus_sensor_attr_deregister(&virtual_sensor_context_obj->mdev);
    if (err) {
        pr_err("misc_deregister fail: %d\n", err);
    }

    kfree(virtual_sensor_context_obj);

    return 0;
}

int virtual_sensor_driver_add(struct virtual_sensor_init_info *obj, int handle)
{
    int err = 0;
    int index = 0;

    pr_err("handle:%d\n", handle);
    if (!obj) {
        pr_err("VIRTUAL_SENSOR handle: %d, driver add fail\n", handle);
        return -1;
    }

    index = handle_to_index(handle);
    if (index < 0) {
        pr_err("[%s] invalid index\n", __func__);
        return  -1;
    }

    if (virtual_sensor_init_list[index] == NULL) {
        virtual_sensor_init_list[index] = obj;
    } else
        pr_err("virtual_sensor_init_list handle:%d already exist\n",
            handle);
    return err;
}

static int __init virtual_sensor_init(void)
{
    pr_debug("%s\n", __func__);

    if (virtual_sensor_probe()) {
        pr_err("failed to register virtual_sensor driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit virtual_sensor_exit(void)
{
    virtual_sensor_remove();
}

late_initcall(virtual_sensor_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VIRTUAL_SENSOR device driver");
