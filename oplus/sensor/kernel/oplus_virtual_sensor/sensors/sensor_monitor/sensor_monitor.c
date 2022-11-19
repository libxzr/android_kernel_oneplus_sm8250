
#include "sensor_monitor.h"
#include "sensor_cmd.h"
#include "virtual_sensor.h"
#include <linux/notifier.h>
#include "scp_helper.h"
#include "mtk_nanohub.h"
#include "scp_excep.h"
#include "hf_sensor_type.h"
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>



#define SENSOR_MONITOR_TAG                         "[sensor_monitor] "
#define SENSOR_MONITOR_FUN(f)                      pr_err(SENSOR_MONITOR_TAG"%s\n", __func__)
#define SENSOR_MONITOR_PR_ERR(fmt, args...)        pr_err(SENSOR_MONITOR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define SENSOR_MONITOR_KERNEL_LOG(fmt, args...)    pr_err(SENSOR_MONITOR_TAG fmt, ##args)

static struct virtual_sensor_init_info sensor_monitor_init_info;
static int first_enable = 0;
static int retry_count = 0;
static struct timer_list enable_timer;
static struct workqueue_struct *enable_wq;
static struct work_struct enable_work;

extern int mtk_nanohub_set_cmd_to_hub(uint8_t sensor_id,
    enum CUST_ACTION action, void *data);
extern int scp_notify_store(uint16_t node_type, uint16_t adsp_event_counts);

static int sensor_monitor_open_report_data(int open)
{
    return 0;
}

static int sensor_monitor_enable_nodata(int en)
{
    int res = 0;
    int trace = 0;
    uint64_t fb_phys_addr;
    SENSOR_MONITOR_KERNEL_LOG("enable nodata, en = %d\n", en);

    if (en == 1 && first_enable == 0) {
        res = oplus_enable_to_hub(ID_SENSOR_MONITOR, en);

        fb_phys_addr = (uint64_t)scp_get_reserve_mem_phys(SENS_FB_MEM_ID);
        SENSOR_MONITOR_KERNEL_LOG("SENS_FB_MEM_ID = %u\n", fb_phys_addr);

        trace = fb_phys_addr & 0xFFFFFFFF;
        SENSOR_MONITOR_KERNEL_LOG("phy:0x%llx, scp_phys_addr:0x%x", fb_phys_addr,
            trace);

        res = mtk_nanohub_set_cmd_to_hub(ID_SENSOR_MONITOR,
                CUST_ACTION_SET_TRACE, &trace);

        if (res < 0) {
            pr_err(
                "sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
                ID_SENSOR_MONITOR, CUST_ACTION_SET_TRACE);
            return 0;
        }

        first_enable = 1;
    }

    return res;
}

static int sensor_monitor_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return oplus_set_delay_to_hub(ID_SENSOR_MONITOR, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int sensor_monitor_batch(int flag, int64_t samplingPeriodNs,
    int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    sensor_monitor_set_delay(samplingPeriodNs);
#endif

    SENSOR_MONITOR_KERNEL_LOG("samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
        samplingPeriodNs,
        maxBatchReportLatencyNs);

    return oplus_batch_to_hub(ID_SENSOR_MONITOR, flag, samplingPeriodNs,
            maxBatchReportLatencyNs);
}

static int sensor_monitor_flush(void)
{
    return oplus_flush_to_hub(ID_SENSOR_MONITOR);
}
/*
static int sensor_monitor_data_report(struct data_unit_t *input_event)
{
    struct oplus_sensor_event event;

    memset(&event, 0, sizeof(struct oplus_sensor_event));

    event.handle = ID_SENSOR_MONITOR;
    event.flush_action = DATA_ACTION;
    event.time_stamp = (int64_t)input_event->time_stamp;
    event.word[0] = input_event->oplus_data_t.sensor_monitor_event.state;
    event.word[1] = input_event->oplus_data_t.sensor_monitor_event.report_count;

    return virtual_sensor_data_report(event);
}

static int sensor_monitor_flush_report()
{
    return virtual_sensor_flush_report(ID_SENSOR_MONITOR);
}
*/

static int sensor_monitor_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;
    uint16_t adsp_event_counts = 0;
    uint16_t node_type = 0;

    SENSOR_MONITOR_KERNEL_LOG("recv data, flush_action = %d, state = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->oplus_data_t.sensor_monitor_event.state,
        event->oplus_data_t.sensor_monitor_event.report_count,
        (int64_t)event->time_stamp);

    if (event->flush_action == DATA_ACTION) {
        adsp_event_counts = event->oplus_data_t.sensor_monitor_event.state;
        err = scp_notify_store(node_type, adsp_event_counts);
    }

    return err;
}

static int  sensor_monitor_local_init(void)
{
    struct virtual_sensor_control_path ctl = {0};
    int err = 0;

    ctl.open_report_data =  sensor_monitor_open_report_data;
    ctl.enable_nodata =  sensor_monitor_enable_nodata;
    ctl.set_delay =  sensor_monitor_set_delay;
    ctl.batch =  sensor_monitor_batch;
    ctl.flush =  sensor_monitor_flush;
    ctl.report_data = sensor_monitor_recv_data;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = virtual_sensor_register_control_path(&ctl, ID_SENSOR_MONITOR);

    if (err) {
        SENSOR_MONITOR_KERNEL_LOG("register sensor_monitor control path err\n");
        goto exit;
    }

#ifdef _OPLUS_SENSOR_HUB_VI
    err = scp_sensorHub_data_registration(ID_SENSOR_MONITOR,
            sensor_monitor_recv_data);

    if (err < 0) {
        SENSOR_MONITOR_KERNEL_LOG("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }

#endif

    return 0;
exit:
    return -1;
}

static int sensor_monitor_local_uninit(void)
{
    return 0;
}

static struct virtual_sensor_init_info sensor_monitor_init_info = {
    .name = "sensor_monitor",
    .init = sensor_monitor_local_init,
    .uninit = sensor_monitor_local_uninit,
};

static void enable_work_callback(struct work_struct *work)
{
    int ret = 0;

    ret = sensor_monitor_batch(0, 20000000, 0);
    if (ret < 0) {
        SENSOR_MONITOR_KERNEL_LOG("sensor_monitor_batch fail\n");
        goto err;
    }

    ret = sensor_monitor_enable_nodata(1);
    if (ret < 0) {
        SENSOR_MONITOR_KERNEL_LOG("sensor_monitor_enable_nodata fail\n");
        goto err;
    }

    SENSOR_MONITOR_KERNEL_LOG("sensor_monitor_enable_nodata success\n");
    del_timer(&enable_timer);
    retry_count = 0;

    return;

    err:
    if (retry_count < 3) {
    	retry_count ++;
        mod_timer(&enable_timer, jiffies + (60 * HZ));
        SENSOR_MONITOR_KERNEL_LOG("retry_count=%d\n", retry_count);
    } else {
        retry_count = 0;
        SENSOR_MONITOR_KERNEL_LOG("sensor_monitor enable fail\n");
    }
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static void enable_timer_callback(unsigned long data)
{
    queue_work(enable_wq, &enable_work);
}
#else
static void enable_timer_callback(struct timer_list *unused)
{
    queue_work(enable_wq, &enable_work);
}
#endif

static void enable_timer_init(void)
{
    enable_wq = create_singlethread_workqueue("sensor_monitor_enable");
    INIT_WORK(&enable_work, enable_work_callback);

    init_timer(&enable_timer);
    enable_timer.expires = jiffies + (60 * HZ);
    enable_timer.function = &enable_timer_callback;
    enable_timer.data = ((unsigned long)0);
    mod_timer(&enable_timer, enable_timer.expires);
}

static int __init sensor_monitor_init(void)
{
    virtual_sensor_driver_add(&sensor_monitor_init_info, ID_SENSOR_MONITOR);
    SENSOR_MONITOR_KERNEL_LOG("sensor_monitor_init,FB_MEM\n");
    enable_timer_init();
    return 0;
}

static void __exit  sensor_monitor_exit(void)
{
    SENSOR_MONITOR_FUN();
}

module_init(sensor_monitor_init);
module_exit(sensor_monitor_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SENSOR_MONITOR driver");
MODULE_AUTHOR("jiangyiyu");

