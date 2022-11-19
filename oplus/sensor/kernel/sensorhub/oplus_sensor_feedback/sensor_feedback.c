// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */


#define pr_fmt(fmt) "<sensor_feedback>" fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/soc/qcom/smem.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/time64.h>
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include "hf_manager.h"
#include "scp.h"
#include "sensor_feedback.h"
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
#include <soc/oplus/system/kernel_fb.h>
#endif

#define SENSOR_DEVICE_TYPE	  "10002"
#define SENSOR_POWER_TYPE	   "10003"
#define SENSOR_STABILITY_TYPE   "10004"
#define SENSOR_PFMC_TYPE		"10005"
#define SENSOR_MEMORY_TYPE	  "10006"

#define SENSOR_DEBUG_DEVICE_TYPE	  "20002"
#define SENSOR_DEBUG_POWER_TYPE	   "20003"
#define SENSOR_DEBUG_STABILITY_TYPE   "20004"
#define SENSOR_DEBUG_PFMC_TYPE		"20005"
#define SENSOR_DEBUG_MEMORY_TYPE	  "20006"

#define CUST_ACTION_SET_MEM_ADDR 10
static struct sensor_fb_cxt *g_sensor_fb_cxt = NULL;

/*fb_field :maxlen 19*/
struct sensor_fb_conf g_fb_conf[] = {
	{PS_INIT_FAIL_ID, "device_ps_init_fail", SENSOR_DEVICE_TYPE},
	{PS_I2C_ERR_ID, "device_ps_i2c_err", SENSOR_DEVICE_TYPE},
	{PS_ALLOC_FAIL_ID, "device_ps_alloc_fail", SENSOR_DEVICE_TYPE},
	{PS_ESD_REST_ID, "device_ps_esd_reset", SENSOR_DEVICE_TYPE},
	{PS_NO_INTERRUPT_ID, "device_ps_no_irq", SENSOR_DEVICE_TYPE},
	{PS_FIRST_REPORT_DELAY_COUNT_ID, "device_ps_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{PS_ORIGIN_DATA_TO_ZERO_ID, "device_ps_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{PS_CALI_DATA_ID, "device_ps_cali_data", SENSOR_DEBUG_DEVICE_TYPE},


	{ALS_INIT_FAIL_ID, "device_als_init_fail", SENSOR_DEVICE_TYPE},
	{ALS_I2C_ERR_ID, "device_als_i2c_err", SENSOR_DEVICE_TYPE},
	{ALS_ALLOC_FAIL_ID, "device_als_alloc_fail", SENSOR_DEVICE_TYPE},
	{ALS_ESD_REST_ID, "device_als_esd_reset", SENSOR_DEVICE_TYPE},
	{ALS_NO_INTERRUPT_ID, "device_als_no_irq", SENSOR_DEVICE_TYPE},
	{ALS_FIRST_REPORT_DELAY_COUNT_ID, "device_als_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{ALS_ORIGIN_DATA_TO_ZERO_ID, "device_als_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{ALS_CALI_DATA_ID, "device_als_cali_data", SENSOR_DEBUG_DEVICE_TYPE},


	{ACCEL_INIT_FAIL_ID, "device_acc_init_fail", SENSOR_DEVICE_TYPE},
	{ACCEL_I2C_ERR_ID, "device_acc_i2c_err", SENSOR_DEVICE_TYPE},
	{ACCEL_ALLOC_FAIL_ID, "device_acc_alloc_fail", SENSOR_DEVICE_TYPE},
	{ACCEL_ESD_REST_ID, "device_acc_esd_reset", SENSOR_DEVICE_TYPE},
	{ACCEL_NO_INTERRUPT_ID, "device_acc_no_irq", SENSOR_DEVICE_TYPE},
	{ACCEL_FIRST_REPORT_DELAY_COUNT_ID, "device_acc_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{ACCEL_ORIGIN_DATA_TO_ZERO_ID, "device_acc_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{ACCEL_CALI_DATA_ID, "device_acc_cali_data", SENSOR_DEBUG_DEVICE_TYPE},
	{ACCEL_DATA_BLOCK_ID, "device_acc_data_block", SENSOR_DEBUG_DEVICE_TYPE},


	{GYRO_INIT_FAIL_ID, "device_gyro_init_fail", SENSOR_DEVICE_TYPE},
	{GYRO_I2C_ERR_ID, "device_gyro_i2c_err", SENSOR_DEVICE_TYPE},
	{GYRO_ALLOC_FAIL_ID, "device_gyro_alloc_fail", SENSOR_DEVICE_TYPE},
	{GYRO_ESD_REST_ID, "device_gyro_esd_reset", SENSOR_DEVICE_TYPE},
	{GYRO_NO_INTERRUPT_ID, "device_gyro_no_irq", SENSOR_DEVICE_TYPE},
	{GYRO_FIRST_REPORT_DELAY_COUNT_ID, "device_gyro_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{GYRO_ORIGIN_DATA_TO_ZERO_ID, "device_gyro_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{GYRO_CALI_DATA_ID, "device_gyro_cali_data", SENSOR_DEBUG_DEVICE_TYPE},


	{MAG_INIT_FAIL_ID, "device_mag_init_fail", SENSOR_DEVICE_TYPE},
	{MAG_I2C_ERR_ID, "device_mag_i2c_err", SENSOR_DEVICE_TYPE},
	{MAG_ALLOC_FAIL_ID, "device_mag_alloc_fail", SENSOR_DEVICE_TYPE},
	{MAG_ESD_REST_ID, "device_mag_esd_reset", SENSOR_DEVICE_TYPE},
	{MAG_NO_INTERRUPT_ID, "device_mag_no_irq", SENSOR_DEVICE_TYPE},
	{MAG_FIRST_REPORT_DELAY_COUNT_ID, "device_mag_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{MAG_ORIGIN_DATA_TO_ZERO_ID, "device_mag_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{MAG_CALI_DATA_ID, "device_mag_cali_data", SENSOR_DEBUG_DEVICE_TYPE},


	{SAR_INIT_FAIL_ID, "device_sar_init_fail", SENSOR_DEVICE_TYPE},
	{SAR_I2C_ERR_ID, "device_sar_i2c_err", SENSOR_DEVICE_TYPE},
	{SAR_ALLOC_FAIL_ID, "device_sar_alloc_fail", SENSOR_DEVICE_TYPE},
	{SAR_ESD_REST_ID, "device_sar_esd_reset", SENSOR_DEVICE_TYPE},
	{SAR_NO_INTERRUPT_ID, "device_sar_no_irq", SENSOR_DEVICE_TYPE},
	{SAR_FIRST_REPORT_DELAY_COUNT_ID, "device_sar_rpt_delay", SENSOR_DEBUG_DEVICE_TYPE},
	{SAR_ORIGIN_DATA_TO_ZERO_ID, "device_sar_to_zero", SENSOR_DEBUG_DEVICE_TYPE},
	{SAR_CALI_DATA_ID, "device_sar_cali_data", SENSOR_DEBUG_DEVICE_TYPE},


	{POWER_SENSOR_INFO_ID, "debug_power_sns_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_ACCEL_INFO_ID, "debug_power_acc_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_GYRO_INFO_ID, "debug_power_gyro_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_MAG_INFO_ID, "debug_power_mag_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_PROXIMITY_INFO_ID, "debug_power_prox_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_LIGHT_INFO_ID, "debug_power_light_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_WISE_LIGHT_INFO_ID, "debug_power_wiseligt_info", SENSOR_DEBUG_POWER_TYPE},
	{POWER_WAKE_UP_RATE_ID, "debug_power_wakeup_rate", SENSOR_DEBUG_POWER_TYPE},
	{POWER_ADSP_SLEEP_RATIO_ID, "power_adsp_sleep_ratio", SENSOR_POWER_TYPE},

	{DOUBLE_TAP_REPORTED_ID, "device_double_tap_reported", SENSOR_DEBUG_DEVICE_TYPE},
	{DOUBLE_TAP_PREVENTED_BY_NEAR_ID, "device_double_tap_prevented_by_near", SENSOR_DEBUG_DEVICE_TYPE},
	{DOUBLE_TAP_PREVENTED_BY_ATTITUDE_ID, "device_double_prevented_by_attitude", SENSOR_DEBUG_DEVICE_TYPE},
	{DOUBLE_TAP_PREVENTED_BY_FREEFALL_Z_ID, "device_double_prevented_by_freefall_z", SENSOR_DEBUG_DEVICE_TYPE},
	{DOUBLE_TAP_PREVENTED_BY_FREEFALL_SLOPE_ID, "device_double_prevented_by_freefall_slope", SENSOR_DEBUG_DEVICE_TYPE},

	{ALAILABLE_SENSOR_LIST_ID, "available_sensor_list", SENSOR_DEBUG_DEVICE_TYPE},

	{HAL_SENSOR_NOT_FOUND, "device_hal_not_found", SENSOR_DEVICE_TYPE},
	{HAL_QMI_ERROR, "device_hal_qmi_error", SENSOR_DEVICE_TYPE},
	{HAL_SENSOR_TIMESTAMP_ERROR, "device_hal_ts_error", SENSOR_DEBUG_DEVICE_TYPE}
};

static int find_event_id(int16_t event_id)
{
	int len = sizeof(g_fb_conf) / sizeof(g_fb_conf[0]);
	int ret = -1;
	int index = 0;

	for (index = 0; index < len; index++) {
		if (g_fb_conf[index].event_id == event_id) {
			ret = index;
		}
	}

	return ret;
}

static ssize_t adsp_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;
	uint16_t adsp_event_counts = 0;

	spin_lock(&sensor_fb_cxt->rw_lock);
	adsp_event_counts = sensor_fb_cxt->adsp_event_counts;
	spin_unlock(&sensor_fb_cxt->rw_lock);
	pr_info("adsp_value = %d\n", adsp_event_counts);
	return snprintf(buf, PAGE_SIZE, "%hu\n", adsp_event_counts);
}

static ssize_t adsp_notify_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;
	uint16_t adsp_event_counts = 0;
	uint16_t node_type = 0;
	int err = 0;

	err = sscanf(buf, "%hu %hu", &node_type, &adsp_event_counts);

	if (err < 0) {
		pr_err("adsp_notify_store error: err = %d\n", err);
		return err;
	}

	spin_lock(&sensor_fb_cxt->rw_lock);
	sensor_fb_cxt->adsp_event_counts = adsp_event_counts;
	sensor_fb_cxt->node_type = node_type;
	spin_unlock(&sensor_fb_cxt->rw_lock);
	pr_info("adsp_value = %d, node_type=%d\n", adsp_event_counts,
		node_type);

	set_bit(THREAD_WAKEUP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
	/*wake_up_interruptible(&sensor_fb_cxt->wq);*/
	wake_up(&sensor_fb_cxt->wq);
	return count;
}

int scp_notify_store(uint16_t node_type, uint16_t adsp_event_counts)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;

	spin_lock(&sensor_fb_cxt->rw_lock);
	sensor_fb_cxt->adsp_event_counts = adsp_event_counts;
	sensor_fb_cxt->node_type = node_type;
	spin_unlock(&sensor_fb_cxt->rw_lock);
	pr_info("adsp_value = %d, node_type=%d\n", adsp_event_counts,
		node_type);

	set_bit(THREAD_WAKEUP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
	/*wake_up_interruptible(&sensor_fb_cxt->wq);*/
	wake_up(&sensor_fb_cxt->wq);
	return 0;
}

static ssize_t hal_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t event_ct = 0;
	uint16_t event_id = 0;
	char strbuf[32] = {0x00};
	int err = 0;
	int index = 0;
	unsigned char payload[1024] = {0x00};

	pr_info("hal_info_store\n");
	memset(strbuf, 0, 32);
	memset(payload, 0, 1024);

	err = sscanf(buf, "%u %u %31s", &event_id, &event_ct, strbuf);

	if (err < 0) {
		pr_err("hal_info_store error: err = %d\n", err);
		return count;
	}

	strbuf[31] = '\0';

	index = find_event_id(event_id);

	if (index == -1) {
		pr_info("nout find event_id =%d\n", event_id);
		return count;
	}

	scnprintf(payload, sizeof(payload),
		"NULL$$EventField@@%s$$FieldData@@%d$$detailData@@%s",
		g_fb_conf[index].fb_field,
		event_ct,
		strbuf);
	pr_info("payload =%s\n", payload);

#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
	oplus_kevent_fb(FB_SENSOR, g_fb_conf[index].fb_event_id, payload);
#endif
	return count;
}


static ssize_t test_id_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;
	uint16_t adsp_event_counts = 0;
	uint16_t node_type = 0;
	uint16_t event_id = 0;
	uint16_t event_data = 0;
	int err = 0;

	err = sscanf(buf, "%hu %hu %hu %hu", &node_type, &adsp_event_counts, &event_id,
			&event_data);

	if (err < 0) {
		pr_err("test_id_store error: err = %d\n", err);
		return count;
	}

	spin_lock(&sensor_fb_cxt->rw_lock);
	sensor_fb_cxt->adsp_event_counts = adsp_event_counts;
	sensor_fb_cxt->node_type = node_type;
	spin_unlock(&sensor_fb_cxt->rw_lock);

	sensor_fb_cxt->fb_smem.event[0].event_id = event_id;
	sensor_fb_cxt->fb_smem.event[0].count = event_data;

	pr_info("test_id_store adsp_value = %d, node_type=%d \n", adsp_event_counts,
		node_type);
	pr_info("test_id_store event_id = %d, event_data=%d \n", event_id, event_data);

	set_bit(THREAD_WAKEUP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
	/*wake_up_interruptible(&sensor_fb_cxt->wq);*/
	wake_up(&sensor_fb_cxt->wq);
	return count;
}

static ssize_t sensor_list_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;
	uint16_t sensor_list[2] = {0x00};
	spin_lock(&sensor_fb_cxt->rw_lock);
	sensor_list[0] = sensor_fb_cxt->sensor_list[0];
	sensor_list[1] = sensor_fb_cxt->sensor_list[1];
	spin_unlock(&sensor_fb_cxt->rw_lock);
	pr_info("phy = 0x%x, virt = 0x%x\n", sensor_list[0], sensor_list[1]);

	return snprintf(buf, PAGE_SIZE, "phy = 0x%x, virt = 0x%x\n", sensor_list[0],
			sensor_list[1]);
}

DEVICE_ATTR(adsp_notify, 0644, adsp_notify_show, adsp_notify_store);
DEVICE_ATTR(hal_info, 0644, NULL, hal_info_store);
DEVICE_ATTR(test_id, 0644, NULL, test_id_store);
DEVICE_ATTR(sensor_list, 0644, sensor_list_show, NULL);

static struct attribute *sensor_feedback_attributes[] = {
	&dev_attr_adsp_notify.attr,
	&dev_attr_hal_info.attr,
	&dev_attr_test_id.attr,
	&dev_attr_sensor_list.attr,
	NULL
};

static struct attribute_group sensor_feedback_attribute_group = {
	.attrs = sensor_feedback_attributes
};

#define SMEM_SENSOR_FEEDBACK (128)
static int read_data_from_share_mem(struct sensor_fb_cxt *sensor_fb_cxt)
{
	void *smem_addr = NULL;
	struct fb_event_smem *fb_event = NULL;

	smem_addr = (void *)(long)scp_get_reserve_mem_virt(SENS_FB_MEM_ID);
	fb_event = (struct fb_event_smem *)smem_addr;

	if (fb_event == NULL) {
		return -2;
	}

	memcpy((void *)&sensor_fb_cxt->fb_smem, (void *)fb_event,
		sizeof(sensor_fb_cxt->fb_smem));
	return 0;
}

int procce_special_event_id(unsigned short event_id, int count,
	struct sensor_fb_cxt *sensor_fb_cxt)
{
	int ret = 0;

	if (event_id == ALAILABLE_SENSOR_LIST_ID) {
		sensor_fb_cxt->sensor_list[0] = (uint32_t)
			sensor_fb_cxt->fb_smem.event[count].buff[0];
		sensor_fb_cxt->sensor_list[1] = (uint32_t)
			sensor_fb_cxt->fb_smem.event[count].buff[1];
		pr_info("sensor_list virt_sns = 0x%x, phy_sns = 0x%x\n",
			sensor_fb_cxt->sensor_list[0], sensor_fb_cxt->sensor_list[1]);
		ret = 1;
	}

	return ret;
}

static int parse_shr_info(struct sensor_fb_cxt *sensor_fb_cxt)
{
	int ret = 0;
	int count = 0;
	uint16_t event_id = 0;
	int index = 0;
	unsigned char payload[1024] = {0x00};
	int fb_len = 0;
	unsigned char detail_buff[128] = {0x00};

	for (count = 0; count < sensor_fb_cxt->adsp_event_counts; count ++) {
		event_id = sensor_fb_cxt->fb_smem.event[count].event_id;
		pr_info("event_id =%d, count =%d\n", event_id, count);

		index = find_event_id(event_id);

		if (index == -1) {
			pr_info("not find event_id =%d, count =%d\n", event_id, count);
			continue;
		}

		ret = procce_special_event_id(event_id, count, sensor_fb_cxt);

		if (ret == 1) {
			continue;
		}

		memset(payload, 0, sizeof(payload));
		memset(detail_buff, 0, sizeof(detail_buff));
		snprintf(detail_buff, sizeof(detail_buff), "%d %d %d",
			sensor_fb_cxt->fb_smem.event[count].buff[0],
			sensor_fb_cxt->fb_smem.event[count].buff[1],
			sensor_fb_cxt->fb_smem.event[count].buff[2]);
		fb_len += scnprintf(payload, sizeof(payload),
				"NULL$$EventField@@%s$$FieldData@@%d$$detailData@@%s",
				g_fb_conf[index].fb_field,
				sensor_fb_cxt->fb_smem.event[count].count,
				detail_buff);
		pr_info("payload =%s\n", payload);
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
		oplus_kevent_fb(FB_SENSOR, g_fb_conf[index].fb_event_id, payload);
#endif
	}

	return ret;
}

static int sensor_report_thread(void *arg)
{
	int ret = 0;
	struct sensor_fb_cxt *sensor_fb_cxt = (struct sensor_fb_cxt *)arg;
	uint16_t node_type = 0;
	pr_info("sensor_report_thread step1!\n");

	while (!kthread_should_stop()) {
		wait_event_interruptible(sensor_fb_cxt->wq, test_bit(THREAD_WAKEUP,
				(unsigned long *)&sensor_fb_cxt->wakeup_flag));

		clear_bit(THREAD_WAKEUP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
		set_bit(THREAD_SLEEP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
		spin_lock(&sensor_fb_cxt->rw_lock);
		node_type = sensor_fb_cxt->node_type;
		spin_unlock(&sensor_fb_cxt->rw_lock);

		if (node_type == 0) {
			ret = read_data_from_share_mem(sensor_fb_cxt);

		} else if (node_type == 2) {
		} else if (node_type == 3) { /*power done*/
		} else {
			pr_info("test from node\n");
		}

		ret = parse_shr_info(sensor_fb_cxt);
		spin_lock(&sensor_fb_cxt->rw_lock);
		memset((void *)&sensor_fb_cxt->fb_smem, 0, sizeof(struct fb_event_smem));
		sensor_fb_cxt->adsp_event_counts = 0;
		spin_unlock(&sensor_fb_cxt->rw_lock);
	}

	pr_info("step2 ret =%s\n", ret);
	return ret;
}

static ssize_t sensor_list_read_proc(struct file *file, char __user *buf,
	size_t count, loff_t *off)
{
	char page[128] = {0};
	int len = 0;
	struct sensor_fb_cxt *sensor_fb_cxt = (struct sensor_fb_cxt *)PDE_DATA(
			file_inode(file));

	len = snprintf(page, sizeof(page), "phy = 0x%x, virt = 0x%x\n",
			sensor_fb_cxt->sensor_list[0], sensor_fb_cxt->sensor_list[1]);
	len = simple_read_from_buffer(buf, count, off, page, strlen(page));
	pr_info("phy = 0x%x, virt = 0x%x, len=%d \n", sensor_fb_cxt->sensor_list[0],
		sensor_fb_cxt->sensor_list[1],
		len);
	return len;
}

static const struct proc_ops sensor_list_fops = {
	.proc_read = sensor_list_read_proc,
};

static int sensor_polling_thread(void *arg)
{
	struct sensor_fb_cxt *sensor_fb_cxt = (struct sensor_fb_cxt *)arg;
	struct hf_client *client = sensor_fb_cxt->client;
	struct hf_manager_event data[4];
	uint16_t adsp_event_counts = 0;
	uint16_t node_type = 0;
	int size = 0, i = 0;

	if (!client) {
		return -EINVAL;
	}
	while (!kthread_should_stop()) {
		memset(data, 0, sizeof(data));

		size =  hf_client_poll_sensor(client, data, ARRAY_SIZE(data));
		if (size < 0)
			continue;
		for(i = 0; i < size; ++i) {
			if (data[i].sensor_type == SENSOR_TYPE_SENSOR_MONITOR &&
				data[i].action == DATA_ACTION) {
				adsp_event_counts = data[i].word[0];
				pr_info("recv data, state = %d, report_count = %d\n",
						adsp_event_counts, data[i].word[1]);
				scp_notify_store(node_type, adsp_event_counts);
			}
		}
	}
	return 0;
}

static int enable_monitor_sensor(struct sensor_fb_cxt *sensor_fb_cxt)
{
	int ret = 0;
	struct hf_manager_cmd cmd;
	struct custom_cmd cust_cmd;
	uint64_t fb_phys_addr;
	struct hf_manager_batch *batch = NULL;
	sensor_fb_cxt->client = hf_client_create();
	if (!sensor_fb_cxt->client) {
		pr_err("hf_client_create fail\n");
		return -ENOMEM;
	}

	ret = hf_client_find_sensor(sensor_fb_cxt->client, SENSOR_TYPE_SENSOR_MONITOR);
	if (ret < 0) {
		pr_err("hf_client_find_sensor sensor monitor fail\n");
		return -EINVAL;
	}

	if (!sensor_fb_cxt->poll_task) {
		sensor_fb_cxt->poll_task = kthread_run(sensor_polling_thread, (void *)sensor_fb_cxt,
								  "sensor_polling_thread");
		if (IS_ERR(sensor_fb_cxt->poll_task)) {
			pr_err("poll thread create failed\n");
			return -ENOMEM;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = SENSOR_TYPE_SENSOR_MONITOR;
	cmd.action = HF_MANAGER_SENSOR_ENABLE;
	batch = (struct hf_manager_batch *)cmd.data;
	batch->delay = 20000000;
	batch->latency = 0;
	ret = hf_client_control_sensor(sensor_fb_cxt->client, &cmd);
	if (ret < 0) {
		pr_err("hf_client_control_sensor sensor monitor fail\n");
		return -EINVAL;
	}

	fb_phys_addr = scp_get_reserve_mem_phys(SENS_FB_MEM_ID);
	pr_info("SENS_FB_MEM_ID = %u\n", fb_phys_addr);
	memset(&cust_cmd, 0, sizeof(cust_cmd));
	cust_cmd.command = CUST_ACTION_SET_MEM_ADDR;
	cust_cmd.rx_len = 0;
	cust_cmd.tx_len = sizeof(cust_cmd.data[0]);
	cust_cmd.data[0] = fb_phys_addr & 0xFFFFFFFF;
	pr_info("phy:0x%llx, scp_phys_addr:0x%x", fb_phys_addr, cust_cmd.data[0]);
	ret = hf_client_custom_cmd(sensor_fb_cxt->client, SENSOR_TYPE_SENSOR_MONITOR, &cust_cmd);
	if (ret < 0) {
		pr_err("hf_client_custom_cmd sensor monitor fail\n");
		return -EINVAL;
	}

	return 0;
}


static void enable_monitor_sensor_work(struct work_struct *dwork)
{
	struct sensor_fb_cxt *sensor_fb_cxt = container_of(dwork, struct sensor_fb_cxt, enable_sensor_work.work);
	enable_monitor_sensor(sensor_fb_cxt);
}

static int create_sensor_node(struct sensor_fb_cxt *sensor_fb_cxt)
{
	int err = 0;
	struct proc_dir_entry *pentry = NULL;

	err = sysfs_create_group(&sensor_fb_cxt->sensor_fb_dev->dev.kobj,
			&sensor_feedback_attribute_group);

	if (err < 0) {
		pr_err("unable to create sensor_feedback_attribute_group file err=%d\n", err);
		goto sysfs_create_failed;
	}

	kobject_uevent(&sensor_fb_cxt->sensor_fb_dev->dev.kobj, KOBJ_ADD);

	sensor_fb_cxt->proc_sns =  proc_mkdir("sns_debug", NULL);

	if (!sensor_fb_cxt->proc_sns) {
		pr_err("can't create sns_debug proc\n");
		err = -EFAULT;
		goto sysfs_create_failed;
	}

	pentry = proc_create_data("sensor_list", 0666, sensor_fb_cxt->proc_sns,
			&sensor_list_fops, sensor_fb_cxt);

	if (!pentry) {
		pr_err("create sensor_list proc failed.\n");
		err = -EFAULT;
		goto sysfs_create_failed;
	}

	return 0;
sysfs_create_failed:
	sysfs_remove_group(&sensor_fb_cxt->sensor_fb_dev->dev.kobj,
		&sensor_feedback_attribute_group);
	return err;
}

static int sensor_feedback_probe(struct platform_device *pdev)
{
	int err = 0;
	struct sensor_fb_cxt *sensor_fb_cxt = NULL;

	sensor_fb_cxt = kzalloc(sizeof(struct sensor_fb_cxt), GFP_KERNEL);

	if (sensor_fb_cxt == NULL) {
		pr_err("kzalloc g_sensor_fb_cxt failed\n");
		err = -ENOMEM;
		goto alloc_sensor_fb_failed;
	}

	/*sensor_fb_cxt init*/
	sensor_fb_cxt->sensor_fb_dev = pdev;
	g_sensor_fb_cxt = sensor_fb_cxt;
	spin_lock_init(&sensor_fb_cxt->rw_lock);
	init_waitqueue_head(&sensor_fb_cxt->wq);
	set_bit(THREAD_SLEEP, (unsigned long *)&sensor_fb_cxt->wakeup_flag);
	platform_set_drvdata(pdev, sensor_fb_cxt);

	err = create_sensor_node(sensor_fb_cxt);

	if (err != 0) {
		pr_info("create_sensor_node failed\n");
		goto create_sensor_node_failed;
	}

	/*create sensor_feedback_task thread*/
	sensor_fb_cxt->report_task = kthread_create(sensor_report_thread,
			(void *)sensor_fb_cxt,
			"sensor_feedback_task");

	if (IS_ERR(sensor_fb_cxt->report_task)) {
		pr_info("kthread_create failed\n");
		err = PTR_ERR(sensor_fb_cxt->report_task);
		goto create_task_failed;
	}

	/*wake up thread of report_task*/
	wake_up_process(sensor_fb_cxt->report_task);

	INIT_DELAYED_WORK(&sensor_fb_cxt->enable_sensor_work, enable_monitor_sensor_work);
	schedule_delayed_work(&sensor_fb_cxt->enable_sensor_work, msecs_to_jiffies(2000));

	pr_info("sensor_feedback_init success\n");
	return 0;

create_task_failed:
create_sensor_node_failed:
	kfree(sensor_fb_cxt);
	g_sensor_fb_cxt = NULL;
alloc_sensor_fb_failed:
	return err;
}


static int sensor_feedback_remove(struct platform_device *pdev)
{
	struct sensor_fb_cxt *sensor_fb_cxt = g_sensor_fb_cxt;
	sysfs_remove_group(&sensor_fb_cxt->sensor_fb_dev->dev.kobj,
		&sensor_feedback_attribute_group);
	kfree(sensor_fb_cxt);
	g_sensor_fb_cxt = NULL;
	return 0;
}

static const struct of_device_id of_drv_match[] = {
	{ .compatible = "oplus,sensor-feedback"},
	{},
};
MODULE_DEVICE_TABLE(of, of_drv_match);

static struct platform_driver _driver = {
	.probe	  = sensor_feedback_probe,
	.remove	 = sensor_feedback_remove,
	.driver	 = {
		.name	   = "sensor_feedback",
		.of_match_table = of_drv_match,
	},
};

static int __init sensor_feedback_init(void)
{
	pr_info("sensor_feedback_init call\n");

	platform_driver_register(&_driver);
	return 0;
}


static void __exit sensor_feedback_exit(void)
{
	pr_info("sensor_feedback_exit call\n");

	platform_driver_unregister(&_driver);
}


module_init(sensor_feedback_init);
module_exit(sensor_feedback_exit);


MODULE_AUTHOR("JangHua.Tang");
MODULE_LICENSE("GPL v2");
