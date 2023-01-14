/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/libfdt.h>
#include <linux/suspend.h>
#include "sensor_devinfo.h"
#include <linux/gpio.h>
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
#include "hf_sensor_type.h"
#include "mtk_nanohub.h"
#ifdef LINUX_KERNEL_VERSION_419
#include "scp.h"
#else /*LINUX_KERNEL_VERSION_419*/
#include "scp_helper.h"
#include "scp_excep.h"
#endif /*LINUX_KERNEL_VERSION_419*/
#else /*CONFIG_OPLUS_SENSOR_MTK68XX*/
#include <SCP_sensorHub.h>
#include <hwmsensor.h>
#include "SCP_power_monitor.h"
#endif /*CONFIG_OPLUS_SENSOR_MTK68XX*/
#include <soc/oplus/system/oplus_project.h>

#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
extern int mtk_nanohub_set_cmd_to_hub(uint8_t sensor_id, enum CUST_ACTION action, void *data);
extern int mtk_nanohub_req_send(union SCP_SENSOR_HUB_DATA *data);
extern int mtk_nanohub_cfg_to_hub(uint8_t sensor_id, uint8_t *data, uint8_t count);
#endif

int register_lcdinfo_notifier(struct notifier_block *nb);
int unregister_lcdinfo_notifier(struct notifier_block *nb);

__attribute__((weak)) int register_lcdinfo_notifier() {
	return -1;
}

__attribute__((weak)) int unregister_lcdinfo_notifier() {
	return -1;
}

enum {
	SAMSUNG = 1,
	BOE,
	TIANMA,
	NT36672C,
	HX83112F
};

#define DEV_TAG                     "[sensor_devinfo] "
#define DEVINFO_LOG(fmt, args...)   pr_err(DEV_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define UINT2Ptr(n)     (uint32_t *)(n)
#define Ptr2UINT32(p)   (uint32_t)(p)

#define IS_SUPPROT_HWCALI           (0x01)
#define IS_IN_FACTORY_MODE          (0x02)
#define IS_SUPPORT_NEW_ARCH         (0x03)
#define GYRO_CALI_VERSION           (0x04)
#define ACC_CALI_RANGE              (0x05)
#define DO_MAG_SELFTEST             (0x06)
#define CONFIG_SAR_REG              (0x08)
#define GOLD_REAR_CCT_3K            (0x09)
#define GOLD_REAR_CCT_6K            (0x0A)
#define SAR_NUM                     (0x0B)
#define GYRO_CALI_ADAPT_Q           (0x0C)
#define ACC_CALI_ADAPT_Q            (0x0D)
#define IS_SUPPORT_MTK_ORIGIN_CALI  (0x0E)
#define GOLD_ALS_FACTOR             (0x0F)
#define GOLD_REAR_ALS_FACTOR        (0x10)
#define GOLD_REAR_CCT_FACTOR        (0x11)

#define RED_MAX_LUX                 (0x01)
#define GREEN_MAX_LUX               (0x02)
#define BLUE_MAX_LUX                (0x03)
#define WHITE_MAX_LUX               (0x04)
#define CALI_COE                    (0x05)
#define ROW_COE                     (0x06)
#ifdef DEBUG_BRIGHTNESS
#define BRIGHTNESS                     (0x07)
#endif
#define OPLUSCUSTOM_FILE "/dev/block/by-name/oplus_custom"
#define ID_REAR_ALS  97
#define ID_REAR_CCT       98

#ifndef ID_SARS
#define ID_SARS -1
#endif

#define SOURCE_NUM 6

struct delayed_work parameter_work;
struct delayed_work utc_work;
struct delayed_work lcdinfo_work;

struct cali_data* g_cali_data = NULL;
static bool is_parameter_updated = false;

static char para_buf[3][128] = {"", "", ""};
static bool is_support_new_arch = false;
static bool is_support_mtk_origin_cali = false;
static bool gyro_cali_adapt_q = false;
static bool acc_cali_adapt_q = false;
static struct oplus_als_cali_data *gdata = NULL;
static struct proc_dir_entry *sensor_proc_dir = NULL;
static int gyro_cali_version = 1;
static int g_reg_address = 0;
static int g_reg_value = 0;
static int g_sar_num = ID_SAR;
static char acc_cali_range[11] = {0};
static char gold_rear_cct_3k[35] = {0};
static char gold_rear_cct_6k[35] = {0};
static char gold_rear_cct_factor[35] = {0};
static int gold_als_factor = 1001;
static int gold_rear_als_factor = 1001;
static uint32_t lb_bri_max = 320;
atomic_t utc_suspend;

enum {
	NONE_TYPE = 0,
	LCM_DC_MODE_TYPE,
	LCM_BRIGHTNESS_TYPE,
	MAX_INFO_TYPE,
};
struct sensorlist_info_t {
	char name[16];
};

struct als_info{
	uint16_t brightness;
	uint16_t dc_mode;
	bool use_lb_algo;
};

struct als_info g_als_info;


bool is_support_new_arch_func(void)
{
	return is_support_new_arch;
}
bool is_support_mtk_origin_cali_func(void)
{
	return is_support_mtk_origin_cali;
}
enum {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	rear_cct,
	maxhandle,
};

static struct sensorlist_info_t sensorlist_info[maxhandle];

typedef struct {
    unsigned int        nMagicNum1;
    unsigned int        nMagicNum2;
    unsigned int        nOther1[2];
    unsigned char       nOther2[64];
    unsigned char       Sensor[256];
} sensor_config_info_t;

__attribute__((weak)) unsigned int get_project() {
	return -1;
}

const static struct fdt_property* oplus_get_dts_feature(int handle, char* node_name, char* feature_name)
{
	int index = 0;
	int offset = 0;
	int len = 0;
	char node[16];
	char *device_name = NULL;
	const struct fdt_property *prop = NULL;
	void *fdt = initial_boot_params;

	if (handle >= maxhandle) {
		DEVINFO_LOG("ivalid sensor handle %d\n", handle);
		return NULL;
	}

	for (index = 1; index <= SOURCE_NUM; index++) {
		sprintf(node, "%s_%d", node_name, index);
		offset = fdt_path_offset(fdt, node);
		if (offset < 0) {
			DEVINFO_LOG("get %s fail", node);
			return NULL;
		}

		device_name = (char *) fdt_getprop(fdt, offset, "device_name", &len);
		if (device_name == NULL) {
			DEVINFO_LOG("get device_name prop fail \n");
			return NULL;
		} else {
			DEVINFO_LOG("device_name = %s", device_name);
		}

		if (!strcmp(device_name, sensorlist_info[handle].name)) {
			prop = fdt_get_property(fdt, offset, feature_name, &len);
			if (!prop) {
				DEVINFO_LOG("get prop fail \n");
				return NULL;
			}
			DEVINFO_LOG("feature_addr %d, len = %d\n", (uint32_t *)prop->data, fdt32_to_cpu(prop->len));
			break;
		}
	}
	return prop;
}

static void is_support_lb_algo(void)
{
	void *fdt = initial_boot_params;
	char *data = NULL;
	char node[16];
	int offset = 0;
	int index = 0;
	int len = 0;
	g_als_info.use_lb_algo = false;
	for (index = 1; index <= SOURCE_NUM; index++) {
		sprintf(node, "%s_%d", "/odm/light", index);
		offset = fdt_path_offset(fdt, node);
		if (offset < 0) {
			DEVINFO_LOG("get %s fail", node);
			return;
		}
		data = (char *)fdt_getprop(fdt, offset, "use_lb_algo", &len);
		if (data) {
			if (fdt32_to_cpu(*(int *)data) == 1) {
				g_als_info.use_lb_algo = true;
				break;
			}
		}
	}
	DEVINFO_LOG("support lb algo %d", g_als_info.use_lb_algo);
}

static void get_lb_max_brightness(void)
{
	void *fdt = initial_boot_params;
	char *data = NULL;
	char node[16];
	int offset = 0;
	int index = 0;
	int len = 0;
	for (index = 1; index <= SOURCE_NUM; index++) {
		sprintf(node, "%s_%d", "/odm/light", index);
		offset = fdt_path_offset(fdt, node);
		if (offset < 0) {
			DEVINFO_LOG("get lb_bri_max fail");
			return;
		}
		data = (char *)fdt_getprop(fdt, offset, "lb_bri_max", &len);
		if (data) {
			lb_bri_max = fdt32_to_cpu(*(int *)data);
			break;
		}
	}
	DEVINFO_LOG("lb_bri_max %d\n", lb_bri_max);
}

static void get_new_arch_info(void)
{
	void *fdt = initial_boot_params;
	int offset = 0;
	int is_support = 0;
	int len = 0;
	char *data = NULL;

	offset = fdt_path_offset(fdt, "/odm/cali_arch");
	if (offset < 0) {
		DEVINFO_LOG("get cali_arch fail");
		return;
	}

	data = (char *)fdt_getprop(fdt, offset, "new-arch-supported", &len);
	if (NULL == data) {
		DEVINFO_LOG("get new-arch-supported fail");
		return;
	} else {
		is_support = fdt32_to_cpu(*(int *)data);
		DEVINFO_LOG("get new-arch-supported = %d", is_support);
	}

	if (1 == is_support) {
		is_support_new_arch = true;
	} else {
		is_support_new_arch = false;
	}
}

static void get_mtk_cali_origin_info(void)
{
	void *fdt = initial_boot_params;
	int offset = 0;
	int mtk_cali_is_support = 0;
	int len = 0;
	char *data = NULL;

	offset = fdt_path_offset(fdt, "/odm/mtk_cali");
	if (offset < 0) {
		DEVINFO_LOG("get cali_arch fail");
		return;
	}

	data = (char *)fdt_getprop(fdt, offset, "mtk-cali-supported", &len);
	if (NULL == data) {
		DEVINFO_LOG("get mtk-cali-supported fail");
		return;
	} else {
		mtk_cali_is_support = fdt32_to_cpu(*(int *)data);
		DEVINFO_LOG("get mtk-cali-supported = %d", mtk_cali_is_support);
	}

	if (1 == mtk_cali_is_support) {
		is_support_mtk_origin_cali = true;
	} else {
		is_support_mtk_origin_cali = false;
	}
}

static void get_acc_gyro_cali_nv_adapt_q_flag(void)
{
	void *fdt = initial_boot_params;
	int offset = 0;
	int is_support = 0;
	int len = 0;
	char *data = NULL;

	offset = fdt_path_offset(fdt, "/odm/cali_nv_adapt_q");
	if (offset < 0) {
		DEVINFO_LOG("get cali_nv_adapt_q fail");
		return;
	}

	data = (char *)fdt_getprop(fdt, offset, "cali_nv_adapt_q", &len);
	if (NULL == data) {
		DEVINFO_LOG("get cali_nv_adapt_q fail");
		return;
	} else {
		is_support = fdt32_to_cpu(*(int *)data);
		DEVINFO_LOG("get cali_nv_adapt_q = %d", is_support);
	}

	if (1 == is_support) {
		acc_cali_adapt_q = true;
		if (!strcmp(sensorlist_info[gyro].name, "bmi160")) {
			gyro_cali_adapt_q = false;
		} else {
			gyro_cali_adapt_q = true;
		}
	} else {
		acc_cali_adapt_q = false;
		gyro_cali_adapt_q = false;
	}
}

static inline int handle_to_sensor(int handle)
{
	int sensor = -1;

	switch (handle) {
	case accel:
		sensor = ID_ACCELEROMETER;
		break;
	case gyro:
		sensor = ID_GYROSCOPE;
		break;
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	case mag:
		sensor = ID_MAGNETIC_FIELD;
		break;
#else
	case mag:
		sensor = ID_MAGNETIC;
		break;
#endif
	case als:
		sensor = ID_LIGHT;
		break;
	case ps:
		sensor = ID_PROXIMITY;
		break;
	case baro:
		sensor = ID_PRESSURE;
		break;
	case rear_cct:
		sensor = ID_REAR_CCT;
	}
	return sensor;
}

static int read_oplus_custom(void *data)
{
	int ret = -1;
	loff_t pos = 0;
	mm_segment_t fs;
	struct file* pfile = NULL;
	sensor_config_info_t config_info;
	sensor_cali_file_v1_t *data_v1 = NULL;
	sensor_cali_file_v2_t *data_v2 = NULL;

	fs = get_fs();
	set_fs(KERNEL_DS);

	pfile = filp_open(OPLUSCUSTOM_FILE, O_RDONLY | O_TRUNC, 0);
	if (IS_ERR(pfile)) {
		set_fs(fs);
		DEVINFO_LOG("failed to open file %s %p %d\n", OPLUSCUSTOM_FILE, pfile, ret);
		return ret;
	}

	if (!is_support_new_arch) {
		data_v1 = (sensor_cali_file_v1_t *)data;
		if (!data_v1) {
			DEVINFO_LOG("data_v1 NULL\n");
			filp_close(pfile, NULL);
			set_fs(fs);
			return ret;
		}

		ret = vfs_read(pfile, (void *)(&config_info), sizeof(config_info), &pos);
		if (ret != sizeof(sensor_config_info_t)) {
			DEVINFO_LOG("failed to read file %s %p\n", OPLUSCUSTOM_FILE, pfile);
			filp_close(pfile, NULL);
			set_fs(fs);
			return ret;
		}
		filp_close(pfile, NULL);
		set_fs(fs);

		memcpy(data_v1, config_info.Sensor, 256);
	} else {
		data_v2 = (sensor_cali_file_v2_t *)data;
		if (!data_v2) {
			DEVINFO_LOG("data_v2 NULL\n");
			filp_close(pfile, NULL);
			set_fs(fs);
			return ret;
		}

		ret = vfs_read(pfile, (void *)(&config_info), sizeof(config_info), &pos);
		if (ret != sizeof(sensor_config_info_t)) {
			DEVINFO_LOG("failed to read file %s %p\n", OPLUSCUSTOM_FILE, pfile);
			filp_close(pfile, NULL);
			set_fs(fs);
			return ret;
		}
		filp_close(pfile, NULL);
		set_fs(fs);

		memcpy(data_v2, config_info.Sensor, 256);
	}
	DEVINFO_LOG("read success = %d\n", ret);

	return 0;
}

static int sensor_read_oplus_custom(struct cali_data *data)
{
	int ret = 0;
	int index = 0;
	sensor_cali_file_v1_t data_v1;
	sensor_cali_file_v2_t data_v2;

	if (!is_support_new_arch) {
		ret = read_oplus_custom(&data_v1);
		if (ret) {
			DEVINFO_LOG("read_oplus_custom error = %d\n", ret);
			return -EINVAL;
		}
		for (index = 0; index < 3; index++) {
			data->acc_data[index] = data_v1.GsensorData[index];
			data->gyro_data[index] = data_v1.GyroData[index];
		}
		for (index = 0; index < 6; index++) {
			data->ps_cali_data[index] = data_v1.ps_data[index];
		}
		data->als_factor = data_v1.gain_als;
		data->baro_cali_offset = data_v1.baro_cali_offset;
	} else {
		ret = read_oplus_custom(&data_v2);
		if (ret) {
			DEVINFO_LOG("read_oplus_custom error = %d\n", ret);
			return -EINVAL;
		}
		for (index = 0; index < 3; index++) {
			data->acc_data[index] = data_v2.gsensor_data[index];
			data->gyro_data[index] = data_v2.gyro_data[index];
		}
		for (index = 0; index < 6; index++) {
			data->ps_cali_data[index] = data_v2.ps_data[index];
			data->cct_cali_data[index] = data_v2.cct_cali_data[index];
		}
		data->als_factor = data_v2.als_gain;
		data->rear_als_factor = data_v2.rear_als_gain;
		data->baro_cali_offset = data_v2.baro_cali_offset;
	}
	DEVINFO_LOG("acc[%d,%d,%d],gyro[%d,%d,%d],ps[%d,%d,%d,%d,%d,%d],als[%d], bar[%d]\n",
		data->acc_data[0], data->acc_data[1], data->acc_data[2],
		data->gyro_data[0], data->gyro_data[1], data->gyro_data[2],
		data->ps_cali_data[0], data->ps_cali_data[1], data->ps_cali_data[2],
		data->ps_cali_data[3], data->ps_cali_data[4], data->ps_cali_data[5],
		data->als_factor, data->baro_cali_offset);
	return 0;
}

static void transfer_lcdinfo_to_scp(struct work_struct *dwork)
{
	int err = 0;
	unsigned int len = 0;
	union SCP_SENSOR_HUB_DATA lcdinfo_req;
	lcdinfo_req.req.sensorType = ID_LIGHT;
	lcdinfo_req.req.action = OPLUS_ACTION_SET_LCD_INFO;
	DEVINFO_LOG("send lcd info to scp brightness %d, dc_mode %d", (uint32_t)g_als_info.brightness, (uint32_t)g_als_info.dc_mode);
	lcdinfo_req.req.data[0] = (uint32_t)g_als_info.brightness << 16 | g_als_info.dc_mode;
	len = sizeof(lcdinfo_req.req);
	#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	err = mtk_nanohub_req_send(&lcdinfo_req);
	#else
	err = scp_sensorHub_req_send(&lcdinfo_req, &len, 1);
	#endif
	if (err < 0 || lcdinfo_req.rsp.action != OPLUS_ACTION_SET_LCD_INFO) {
		DEVINFO_LOG("fail! err %d\n", err);
		return;
	}
}

static void sync_utc2scp_work(struct work_struct *dwork)
{
	int err = 0;
	unsigned int len = 0;
	struct timex txc;
	struct rtc_time tm;
	union SCP_SENSOR_HUB_DATA rtc_req;
	uint32_t utc_data[6] = {0};

	if (atomic_read(&utc_suspend) == 1) {
		DEVINFO_LOG("Will suspend, stop sync utc \n");
		return;
	}

	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec, &tm);

	utc_data[0] = (uint32_t)tm.tm_mday;
	utc_data[1] = (uint32_t)tm.tm_hour;
	utc_data[2] = (uint32_t)tm.tm_min;
	utc_data[3] = (uint32_t)tm.tm_sec;

	rtc_req.req.sensorType = 0;
	rtc_req.req.action = OPLUS_ACTION_SCP_SYNC_UTC;
	rtc_req.req.data[0] = utc_data[0];
	rtc_req.req.data[1] = utc_data[1];
	rtc_req.req.data[2] = utc_data[2];
	rtc_req.req.data[3] = utc_data[3];
	rtc_req.req.data[4] = txc.time.tv_sec;
	rtc_req.req.data[5] = txc.time.tv_usec;
	DEVINFO_LOG("kernel_ts: %u.%u, %u.%u\n",
		txc.time.tv_sec,
		txc.time.tv_usec,
		rtc_req.req.data[4],
		rtc_req.req.data[5]);
	len = sizeof(rtc_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	err = mtk_nanohub_req_send(&rtc_req);
#else
	err = scp_sensorHub_req_send(&rtc_req, &len, 1);
#endif
	if (err < 0 || rtc_req.rsp.action != OPLUS_ACTION_SCP_SYNC_UTC) {
		DEVINFO_LOG("fail! err %d\n", err);
		return;
	}

	schedule_delayed_work(&utc_work, msecs_to_jiffies(2000));
}

int oplus_send_factory_mode_cmd_to_hub(int sensorType, int mode, void *result)
{
	int err = 0;
	unsigned int len = 0;
	union SCP_SENSOR_HUB_DATA fac_req;

	switch (sensorType) {
	case ID_ACCELEROMETER:
		DEVINFO_LOG("ID_ACCELEROMETER : send_factory_mode_cmd_to_hub");
		fac_req.req.sensorType = OPLUS_ACCEL;
		fac_req.req.action = OPLUS_ACTION_SET_FACTORY_MODE;
		fac_req.req.data[0] = mode;
		len = sizeof(fac_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		err = mtk_nanohub_req_send(&fac_req);
#else
		err = scp_sensorHub_req_send(&fac_req, &len, 1);
#endif
		if (err < 0 || fac_req.rsp.action != OPLUS_ACTION_SET_FACTORY_MODE) {
			DEVINFO_LOG("fail! err %d\n", err);
			return -1;
		} else {
			*((uint8_t *) result) = fac_req.rsp.reserve[0];
		}
		break;
	case ID_PROXIMITY:
		DEVINFO_LOG("ID_PROXIMITY : send_factory_mode_cmd_to_hub");
		fac_req.req.sensorType = OPLUS_PROXIMITY;
		fac_req.req.action = OPLUS_ACTION_SET_FACTORY_MODE;
		fac_req.req.data[0] = mode;
		len = sizeof(fac_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		err = mtk_nanohub_req_send(&fac_req);
#else
		err = scp_sensorHub_req_send(&fac_req, &len, 1);
#endif
		if (err < 0 || fac_req.rsp.action != OPLUS_ACTION_SET_FACTORY_MODE) {
			DEVINFO_LOG("fail! err %d\n", err);
			return -1;
		} else {
			*((uint8_t *) result) = fac_req.rsp.reserve[0];
		}
		break;
	case ID_CCT:
		DEVINFO_LOG("ID_CCT : send_factory_mode_cmd_to_hub");
		fac_req.req.sensorType =  OPLUS_CCT;
		fac_req.req.action = OPLUS_ACTION_SET_FACTORY_MODE;
		fac_req.req.data[0] = mode;
		len = sizeof(fac_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		err = mtk_nanohub_req_send(&fac_req);
#else
		err = scp_sensorHub_req_send(&fac_req, &len, 1);
#endif
		if (err < 0 || fac_req.rsp.action != OPLUS_ACTION_SET_FACTORY_MODE) {
			DEVINFO_LOG("fail! err %d\n", err);
			return -1;
		} else {
			*((uint8_t *) result) = fac_req.rsp.reserve[0];
		}
		break;
	default:
		DEVINFO_LOG("invalid sensortype %d\n", err);
	}
	return 1;
}

int oplus_send_selftest_cmd_to_hub(int sensorType, void *testresult)
{
	int err = 0;
	unsigned int len = 0;
	union SCP_SENSOR_HUB_DATA selftest_req;

	switch (sensorType) {
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	case ID_MAGNETIC_FIELD:
#else
	case ID_MAGNETIC:
#endif
		DEVINFO_LOG("ID_MAGNETIC : oplus_send_selftest_cmd_to_hub");
		selftest_req.req.sensorType = OPLUS_MAG;
		selftest_req.req.action = OPLUS_ACTION_SELF_TEST;
		len = sizeof(selftest_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		err = mtk_nanohub_req_send(&selftest_req);
#else
		err = scp_sensorHub_req_send(&selftest_req, &len, 1);
#endif

		if (err < 0 || selftest_req.rsp.action != OPLUS_ACTION_SELF_TEST) {
			DEVINFO_LOG("fail! err %d\n", err);
			return -1;
		} else {
			*((uint8_t *) testresult) = selftest_req.rsp.reserve[0];
		}
		break;
	default:
		DEVINFO_LOG("invalid sensortype %d\n", err);
	}
	return 1;
}

int oplus_send_reg_config_cmd_to_hub(int sensorType)
{
	int ret = 0;
	unsigned int len = 0;
	union SCP_SENSOR_HUB_DATA reg_req;

	reg_req.req.action = OPLUS_ACTION_CONFIG_REG;
	reg_req.req.data[0] = g_reg_address;
	reg_req.req.data[1] = g_reg_value;
	switch (sensorType) {
	case ID_SAR:
		DEVINFO_LOG("ID_SAR : oplus_send_reg_config_cmd_to_hub");
		reg_req.req.sensorType = OPLUS_SAR;
		break;
	case ID_SARS:
		DEVINFO_LOG("ID_SARS : oplus_send_reg_config_cmd_to_hub");
		reg_req.req.sensorType = OPLUS_SARS;
		break;
	default:
		DEVINFO_LOG("invalid sensortype %d\n", sensorType);
		return -1;
	}

	len = sizeof(reg_req.req);
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	ret = mtk_nanohub_req_send(&reg_req);
#else
	ret = scp_sensorHub_req_send(&reg_req, &len, 1);
#endif

	if (ret < 0 || reg_req.rsp.action != OPLUS_ACTION_CONFIG_REG) {
		DEVINFO_LOG("fail! ret %d\n", ret);
		return -1;
	}
	return 1;
}

int get_sensor_parameter(struct cali_data *data)
{
	if (!g_cali_data) {
		DEVINFO_LOG("g_cali_data NULL! \n");
		return -EINVAL;
	}

	if (is_parameter_updated) {
		is_parameter_updated = false;
		sensor_read_oplus_custom(g_cali_data);
	}

	if (data) {
		memcpy(data, g_cali_data, sizeof(struct cali_data));
	} else {
		DEVINFO_LOG("data NULL! \n");
		return -EINVAL;
	}

	return 0;
}

void update_sensor_parameter(void)
{
	is_parameter_updated = true;
}

int get_sensor_parameter_rear_als(struct cali_data *data)
{
	sensor_read_oplus_custom(data);
	return 0;
}

static int init_sensorlist_info(void)
{
	int err = 0;
	int handle = -1;
	int sensor = -1;
	int ret = -1;
	struct sensorInfo_t devinfo;

	for (handle = accel; handle < maxhandle; ++handle) {
		sensor = handle_to_sensor(handle);
		if (sensor < 0) {
			continue;
		}
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		err = mtk_nanohub_set_cmd_to_hub(sensor,
				CUST_ACTION_GET_SENSOR_INFO, &devinfo);
#else
		err = sensor_set_cmd_to_hub(sensor,
				CUST_ACTION_GET_SENSOR_INFO, &devinfo);
#endif
		if (err < 0) {
			DEVINFO_LOG("sensor(%d) not register\n", sensor);
			strlcpy(sensorlist_info[handle].name,
				"NULL",
				sizeof(sensorlist_info[handle].name));
			continue;
		}

		DEVINFO_LOG("sensor(%s) register\n", devinfo.name);
		strlcpy(sensorlist_info[handle].name,
			devinfo.name,
			sizeof(sensorlist_info[handle].name));
		ret = 0;
	}
	return ret;
}

bool is_sensor_available(char *name)
{
	bool find = false;
	int handle = -1;

	for (handle = accel; handle < maxhandle; ++handle) {
		if (name && (!strcmp(sensorlist_info[handle].name, name))) {
			find = true;
			break;
		}
	}

	return find;
}

int get_light_sensor_type(void)
{
	int type = 1;
	const struct fdt_property *light_type_prop = NULL;

	light_type_prop = oplus_get_dts_feature(als, "/odm/light", "als_type");
	if (light_type_prop == NULL) {
		return NORMAL_LIGHT_TYPE;
	} else {
		type = fdt32_to_cpu(*((uint32_t *)light_type_prop->data));
	}

	DEVINFO_LOG("get_light_sensor_type = %d", type);
	return type;
}

int oplus_sensor_cfg_to_hub(uint8_t sensor_id, uint8_t *data, uint8_t count)
{
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	return mtk_nanohub_cfg_to_hub(sensor_id, data, count);
#else
	return sensor_cfg_to_hub(sensor_id, data, count);
#endif
}

static int als_cali_read_func(struct seq_file *s, void *v)
{
	void *p = s->private;

	DEVINFO_LOG("Ptr2UINT32(p) = %d \n", Ptr2UINT32(p));
	switch (Ptr2UINT32(p)) {
	case RED_MAX_LUX:
		seq_printf(s, "%d", gdata->red_max_lux);
		break;
	case GREEN_MAX_LUX:
		seq_printf(s, "%d", gdata->green_max_lux);
		break;
	case BLUE_MAX_LUX:
		seq_printf(s, "%d", gdata->blue_max_lux);
		break;
	case WHITE_MAX_LUX:
		seq_printf(s, "%d", gdata->white_max_lux);
		break;
	case CALI_COE:
		seq_printf(s, "%d", gdata->cali_coe);
		break;
	case ROW_COE:
		seq_printf(s, "%d", gdata->row_coe);
		break;
	default:
		seq_printf(s, "not support\n");
	}
	return 0;
}

static ssize_t als_cali_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64] = {0};
	void *buf_str = buf;
	long val = 0;
	int ret = 0;
	struct seq_file *s = filp->private_data;
	void *p = s->private;
	int node = Ptr2UINT32(p);

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf_str, ubuf, cnt)) {
			return -EFAULT;
		}
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&val);
	DEVINFO_LOG("node1 = %d,val = %d\n", node, val);

	switch (node) {
	case RED_MAX_LUX:
		gdata->red_max_lux = val;
		break;
	case GREEN_MAX_LUX:
		gdata->green_max_lux = val;
		break;
	case BLUE_MAX_LUX:
		gdata->blue_max_lux = val;
		break;
	case WHITE_MAX_LUX:
		gdata->white_max_lux = val;
		break;
	case CALI_COE:
		gdata->cali_coe = val;
		break;
	case ROW_COE:
		gdata->row_coe = val;
		break;
#ifdef DEBUG_BRIGHTNESS
	case BRIGHTNESS:
		if (g_als_info.brightness != val) {
			g_als_info.brightness = val;
			schedule_delayed_work(&lcdinfo_work, msecs_to_jiffies(10));
			DEVINFO_LOG("set brightness %d to scp\n", g_als_info.brightness);
		}
		break;
#endif
	default:
		DEVINFO_LOG("ivalid node type\n");
	}
	return cnt;
}

static int als_cali_open(struct inode *inode, struct file *file)
{
	return single_open(file, als_cali_read_func, PDE_DATA(inode));
}

static const struct file_operations als_cali_para_fops = {
	.owner = THIS_MODULE,
	.open  = als_cali_open,
	.write = als_cali_write,
	.read  = seq_read,
	.release = single_release,
};

static void get_accgyro_cali_version(void)
{
	const struct fdt_property *accgyro_prop = NULL;
	uint32_t acc_thrd_x;
	uint32_t acc_thrd_y;
	uint32_t acc_thrd_z;

	accgyro_prop = oplus_get_dts_feature(accel, "/odm/gsensor", "gyro_cali_version");
	if (accgyro_prop == NULL) {
		gyro_cali_version = 1;
	} else {
		gyro_cali_version = fdt32_to_cpu(*(uint32_t *)accgyro_prop->data);
	}
	DEVINFO_LOG("gyro_cali_version = %d", gyro_cali_version);

	accgyro_prop = oplus_get_dts_feature(accel, "/odm/gsensor", "acc_cali_range");
	if (accgyro_prop == NULL) {
		return;
	} else {
		acc_thrd_x = fdt32_to_cpu(*((uint32_t *)accgyro_prop->data));
		acc_thrd_y = fdt32_to_cpu(*((uint32_t *)accgyro_prop->data + 1));
		acc_thrd_z = fdt32_to_cpu(*((uint32_t *)accgyro_prop->data + 2));
		DEVINFO_LOG("acc range x y z [%u, %u, %u]", acc_thrd_x, acc_thrd_y, acc_thrd_z);
		sprintf(acc_cali_range, "%u %u %u", acc_thrd_x, acc_thrd_y, acc_thrd_z);
	}
}

static void oplus_als_cali_data_init(void)
{
	struct proc_dir_entry *pentry;
	struct oplus_als_cali_data *data = NULL;
	const struct fdt_property *als_ratio_prop = NULL;
	const struct fdt_property *gold_als_factor_prop = NULL;
	const struct fdt_property *gold_rear_als_factor_prop = NULL;

	DEVINFO_LOG("call \n");
	if (gdata) {
		DEVINFO_LOG("has been inited\n");
		return;
	}

	data = kzalloc(sizeof(struct oplus_als_cali_data), GFP_KERNEL);
	if (data == NULL) {
		DEVINFO_LOG("kzalloc fail\n");
		return;
	}
	gdata = data;

	als_ratio_prop = oplus_get_dts_feature(als, "/odm/light", "als_ratio");
	if (als_ratio_prop == NULL) {
		gdata->row_coe = 1000;
	} else {
		gdata->row_coe = fdt32_to_cpu(*(int *)als_ratio_prop->data);
	}
	DEVINFO_LOG("row_coe = %d\n", gdata->row_coe);

	gold_als_factor_prop = oplus_get_dts_feature(als, "/odm/light", "gold_als_factor");
	if (gold_als_factor_prop == NULL) {
		gold_als_factor = 1001;
	} else {
		gold_als_factor = fdt32_to_cpu(*(int *)gold_als_factor_prop->data);
	}
	DEVINFO_LOG("gold_als_factor = %d\n", gold_als_factor);

	gold_rear_als_factor_prop = oplus_get_dts_feature(als, "/odm/rear_cct", "gold_rear_als_factor");
	if (gold_rear_als_factor_prop == NULL) {
		gold_rear_als_factor = 1001;
	} else {
		gold_rear_als_factor = fdt32_to_cpu(*(int *)gold_rear_als_factor_prop->data);
	}
	DEVINFO_LOG("gold_rear_als_factor = %d\n", gold_rear_als_factor);

	if (gdata->proc_oplus_als) {
		DEVINFO_LOG("proc_oplus_als has alread inited\n");
		return;
	}

	sensor_proc_dir = proc_mkdir("sensor", NULL);
	if (!sensor_proc_dir) {
		DEVINFO_LOG("can't create proc_sensor proc\n");
		return;
	}

	gdata->proc_oplus_als =  proc_mkdir("als_cali", sensor_proc_dir);
	if (!gdata->proc_oplus_als) {
		DEVINFO_LOG("can't create proc_oplus_als proc\n");
		return;
	}

	pentry = proc_create_data("red_max_lux", S_IRUGO | S_IWUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(RED_MAX_LUX));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}

	pentry = proc_create_data("green_max_lux", S_IRUGO | S_IWUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(GREEN_MAX_LUX));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}

	pentry = proc_create_data("blue_max_lux", S_IRUGO | S_IWUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(BLUE_MAX_LUX));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}

	pentry = proc_create_data("white_max_lux", S_IRUGO | S_IWUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(WHITE_MAX_LUX));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}

	pentry = proc_create_data("cali_coe", S_IRUGO | S_IWUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(CALI_COE));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}

	pentry = proc_create_data("row_coe", S_IRUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(ROW_COE));
	if (!pentry) {
		DEVINFO_LOG("create red_max_lux proc failed.\n");
		return;
	}
#ifdef DEBUG_BRIGHTNESS
	pentry = proc_create_data("brightness", S_IRUGO, gdata->proc_oplus_als,
		&als_cali_para_fops, UINT2Ptr(BRIGHTNESS));
	if (!pentry) {
		DEVINFO_LOG("create brightness proc failed.\n");
		return;
	}
#endif


	return;
}

static void get_gold_rear_cct(void)
{
	const struct fdt_property *gold_prop = NULL;
	uint32_t gold_rear_cct_r = 0;
	uint32_t gold_rear_cct_g = 0;
	uint32_t gold_rear_cct_b = 0;
	uint32_t gold_rear_cct_c = 0;
	uint32_t gold_rear_cct_w = 0;
	uint32_t gold_rear_cct_f = 0;

    //get 3000k gold ch
	gold_prop = oplus_get_dts_feature(rear_cct, "/odm/rear_cct", "gold_rear_cct_3k");
	if (gold_prop == NULL) {
		return;
	} else {
		gold_rear_cct_r = fdt32_to_cpu(*((uint32_t *)gold_prop->data));
		gold_rear_cct_g = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 1));
		gold_rear_cct_b = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 2));
		gold_rear_cct_c = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 3));
		gold_rear_cct_w = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 4));
		gold_rear_cct_f = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 5));
		DEVINFO_LOG("gold_rear_cct_3k [%u, %u, %u, %u, %u, %u]",
				gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
				gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);

		sprintf(gold_rear_cct_3k, "%u %u %u %u %u %u",
				gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
				gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);
	}
    //get 6000k gold ch
	gold_prop = oplus_get_dts_feature(rear_cct, "/odm/rear_cct", "gold_rear_cct_6k");
	if (gold_prop == NULL) {
		return;
	} else {
		gold_rear_cct_r = fdt32_to_cpu(*((uint32_t *)gold_prop->data));
		gold_rear_cct_g = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 1));
		gold_rear_cct_b = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 2));
		gold_rear_cct_c = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 3));
		gold_rear_cct_w = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 4));
		gold_rear_cct_f = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 5));
		DEVINFO_LOG("gold_rear_cct_6k [%u, %u, %u, %u, %u, %u]",
				gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
				gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);

		sprintf(gold_rear_cct_6k, "%u %u %u %u %u %u",
				gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
				gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);
	}

	gold_prop = oplus_get_dts_feature(rear_cct, "/odm/rear_cct", "gold_rear_cct_factor");
	if (gold_prop == NULL) {
		sprintf(gold_rear_cct_factor, "1001 1001 1001 1001 1001 1001");
		return;
	} else {
		gold_rear_cct_r = fdt32_to_cpu(*((uint32_t *)gold_prop->data));
		gold_rear_cct_g = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 1));
		gold_rear_cct_b = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 2));
		gold_rear_cct_c = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 3));
		gold_rear_cct_w = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 4));
		gold_rear_cct_f = fdt32_to_cpu(*((uint32_t *)gold_prop->data + 5));
		DEVINFO_LOG("gold_rear_cct_factor [%u, %u, %u, %u, %u, %u]",
			gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
			gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);

		sprintf(gold_rear_cct_factor, "%u %u %u %u %u %u",
			gold_rear_cct_r, gold_rear_cct_g, gold_rear_cct_b,
			gold_rear_cct_c, gold_rear_cct_w, gold_rear_cct_f);
	}
}

static void sensor_devinfo_work(struct work_struct *dwork)
{
	int ret = 0;
	int count = 10;
	int index = 0;
	int cfg_data[12] = {0};

	do {
		ret = sensor_read_oplus_custom(g_cali_data);
		if (ret) {
			DEVINFO_LOG("try %d\n", count);
			count--;
			msleep(1000);
		}
	} while (ret && count > 0);

	if (ret) {
		DEVINFO_LOG("fail!\n");
		return;
	}

	/*to make sure scp is up*/
	count = 5;
	do {
		ret = init_sensorlist_info();
		if (ret < 0) {
			DEVINFO_LOG("scp access err , try %d\n", count);
			count--;
			msleep(1000);
		}
	} while (ret < 0 && count > 0);

	/* send cfg to scp*/
	/* dynamic bias: cfg_data[0] ~ cfg_data[2] */
	/* static bias: cfg_data[3] ~ cfg_data[5], now cfg static bias */
	for (index = 3; index < 6; index++) {
		cfg_data[index] = g_cali_data->acc_data[index-3];
	}
	if (!is_support_mtk_origin_cali) {
		ret = oplus_sensor_cfg_to_hub(ID_ACCELEROMETER, (uint8_t *)cfg_data, sizeof(cfg_data));
		if (ret < 0) {
			DEVINFO_LOG("send acc config fail\n");
		}
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*gyro*/
	/* dynamic bias: cfg_data[0] ~ cfg_data[2] */
	/* static bias: cfg_data[3] ~ cfg_data[5], now cfg static bias */
	for (index = 3; index < 6; index++) {
		cfg_data[index] = g_cali_data->gyro_data[index-3];
	}
	if (!is_support_mtk_origin_cali) {
		ret = oplus_sensor_cfg_to_hub(ID_GYROSCOPE, (uint8_t *)cfg_data, sizeof(cfg_data));
		if (ret < 0) {
			DEVINFO_LOG("send gyro config fail\n");
		}
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*ps*/
	for (index = 0; index < 6; index++) {
		cfg_data[index] = g_cali_data->ps_cali_data[index];
	}
	if (!is_support_mtk_origin_cali) {
		ret = oplus_sensor_cfg_to_hub(ID_PROXIMITY, (uint8_t *)cfg_data, sizeof(cfg_data));
		if (ret < 0) {
			DEVINFO_LOG("send ps config fail\n");
		}
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*light*/
	cfg_data[0] = g_cali_data->als_factor;
	if (!is_support_mtk_origin_cali) {
		ret = oplus_sensor_cfg_to_hub(ID_LIGHT, (uint8_t *)cfg_data, sizeof(cfg_data));
		if (ret < 0) {
			DEVINFO_LOG("send light config fail\n");
		}
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*rear cct*/
	for (index = 0; index < 6; index++) {
		cfg_data[index] = g_cali_data->cct_cali_data[index];
	}
	ret = oplus_sensor_cfg_to_hub(ID_REAR_CCT, (uint8_t *)cfg_data, sizeof(cfg_data));
	if (ret < 0) {
		DEVINFO_LOG("send light config fail\n");
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*rear_als*/
	cfg_data[0] = g_cali_data->rear_als_factor;
	ret = oplus_sensor_cfg_to_hub(ID_REAR_ALS, (uint8_t *)cfg_data, sizeof(cfg_data));
	if (ret < 0) {
		DEVINFO_LOG("send rear als config fail\n");
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	/*baro_cali*/
	cfg_data[0] = g_cali_data->baro_cali_offset;
	ret = oplus_sensor_cfg_to_hub(ID_PRESSURE, (uint8_t *)cfg_data, sizeof(cfg_data));
	if (ret < 0) {
		DEVINFO_LOG("send baro cali config fail\n");
	}
	memset(cfg_data, 0, sizeof(int) * 12);

	oplus_als_cali_data_init();
	get_accgyro_cali_version();
	get_gold_rear_cct();
	get_acc_gyro_cali_nv_adapt_q_flag();

	DEVINFO_LOG("success \n");
}

static ssize_t parameter_proc_read(struct file *file, char __user *buf,
    size_t count, loff_t *off)
{
	char page[512];
	int len = 0;

	len = sprintf(page, "{%s,\n %s,\n %s}", para_buf[0], para_buf[1], para_buf[2]);

	if ((para_buf[2][0] == '\0') && (para_buf[1][0] == '\0')) {
		page[len - 4] = ' ';
		page[len - 6] = ' ';
		page[len - 7] = ' ';
	} else if (para_buf[2][0] == '\0') {
		page[len - 4] = ' ';
	}

	if (len > *off) {
		len -= *off;
	} else {
		len = 0;
	}

	if (copy_to_user(buf, page, (len < count ?  len : count))) {
		return -EFAULT;
	}
	*off +=  len < count ?  len : count;
	return (len < count ?  len : count);
}

static const struct file_operations parameter_proc_fops = {
	.read = parameter_proc_read,
	.write = NULL,
};

int get_msensor_parameter(int num)
{
	int offset = 0;
	int len = 0;
	int para_len = 0;
	int index = 0;
	int flt_num = 0;
	int para_num = 0;
	uint32_t *data_addr = NULL;
	uint32_t temp_data;
	void *fdt = initial_boot_params;
	char *libname = NULL;
	char *match_project = NULL;
	char temp_buf[128] = {0}, msensor[16], float_buf[10], mag_para[30];
	char project[10] = {0};
	const struct fdt_property *prop = NULL;

	sprintf(msensor, "/odm/msensor_%d", num + 1);
	offset = fdt_path_offset(fdt, msensor);
	if (offset < 0) {
		DEVINFO_LOG("[oem] get %s offset fail", msensor);
		return -1;
	}

	libname = (char *) fdt_getprop(fdt, offset, "libname", &len);
	if (libname == NULL) {
		DEVINFO_LOG("get libname prop fail");
		return -1;
	}
	DEVINFO_LOG(" %s libname is %s\n", msensor, libname);

	prop = fdt_get_property(fdt, offset, "para_num", &len);
	if (prop == NULL) {
		DEVINFO_LOG("para num is null, no need to match project");
		prop = fdt_get_property(fdt, offset, "soft-mag-parameter", &len);
		if (prop == NULL) {
			DEVINFO_LOG("get soft-mag-parameter prop fail");
			return -1;
		}
		para_len = fdt32_to_cpu(prop->len); /*bytes*/
		data_addr = (uint32_t *)prop->data;
	} else {
		data_addr = (uint32_t *)prop->data;
		para_num = fdt32_to_cpu(*data_addr);
		DEVINFO_LOG(" %s match project start, para_num = %d\n", msensor, para_num);

		sprintf(project, "%u", get_project());
		DEVINFO_LOG("project %s\n", project);

		for (index = 0; index < para_num; index++) {
			sprintf(mag_para, "/odm/msensor_%d/mag_para_%d", num + 1, index + 1);
			offset = fdt_path_offset(fdt, mag_para);
			if (offset < 0) {
				DEVINFO_LOG("[oem] get %s offset fail", mag_para);
				return -1;
			}

			match_project = (char *) fdt_getprop(fdt, offset, "match_projects", &len);
			if (match_project == NULL) {
				DEVINFO_LOG("get match_project prop fail");
				return -1;
			}
			DEVINFO_LOG("soft_magpara_%d match project %s\n", index, match_project);

			if (strstr(match_project, project) != NULL) {
				prop = fdt_get_property(fdt, offset, "soft-mag-parameter", &len);
				if (prop == NULL) {
					DEVINFO_LOG("get soft-mag-parameter prop fail");
					return -1;
				}
				para_len = fdt32_to_cpu(prop->len); /*bytes*/
				data_addr = (uint32_t *)prop->data;

				DEVINFO_LOG("match project success");
				break;
			}
		}
	}

	if (!strcmp(libname, "mmc") || !strcmp(libname, "mxg")) { /*Memsic parameter need analyze*/
		flt_num = fdt32_to_cpu(*data_addr);
		for (index = 0; index < 9; index++) {
			temp_data = fdt32_to_cpu(*(data_addr + (2 * index)));
			sprintf(float_buf, "%c%d.%d%d%d%d", fdt32_to_cpu(*(data_addr + 2 * index + 1)) ? '-' : ' ',
				temp_data / 10000, temp_data % 10000 / 1000, temp_data % 1000 / 100, temp_data % 100 / 10,
				temp_data % 10);
			sprintf(para_buf[num], "%s,%s", temp_buf, float_buf);
			strcpy(temp_buf, para_buf[num]);
		}
		temp_buf[0] = ' ';
		sprintf(para_buf[num], "\"%s\":[%s]", libname, temp_buf);
	} else if (!strcmp(libname, "akm")) {
		for (index = 1; index < para_len / 4; index++) {
			sprintf(para_buf[num], "%s,%d", temp_buf, fdt32_to_cpu(*(data_addr + index)));
			strcpy(temp_buf, para_buf[num]);
		}
		sprintf(para_buf[num], "\"%s\":[%u%s]", libname, fdt32_to_cpu(*data_addr), temp_buf);
	}
	return 0;
}

void  mag_soft_parameter_init()
{
	int ret = -1;
	int index = 0;

	for (index = 0; index < 3; index++) {
		ret = get_msensor_parameter(index);
		if (ret == -1) {
			para_buf[index][0] = '\0';
		} else {
			proc_create("mag_soft_parameter.json", 0666, NULL, &parameter_proc_fops);
		}
	}
}

static int sensor_feature_read_func(struct seq_file *s, void *v)
{
	void *p = s->private;
	int ret = 0;
	int selftest_result = 0;

	DEVINFO_LOG("Ptr2UINT32(p) = %d \n", Ptr2UINT32(p));
	switch (Ptr2UINT32(p)) {
	case IS_SUPPROT_HWCALI:
		if (!strcmp(sensorlist_info[ps].name, "tcs3701")) {
			seq_printf(s, "%d", 1);
		} else if (!strcmp(sensorlist_info[ps].name, "tmd2755x12")) {
			seq_printf(s, "%d", 1);
		} else {
			seq_printf(s, "%d", 0);
		}
		break;
	case IS_IN_FACTORY_MODE:
		seq_printf(s, "%d", 1);
		break;
	case IS_SUPPORT_NEW_ARCH:
		seq_printf(s, "%d", is_support_new_arch);
		break;
	case IS_SUPPORT_MTK_ORIGIN_CALI:
		seq_printf(s, "%d", is_support_mtk_origin_cali);
		break;
	case GYRO_CALI_VERSION:
		seq_printf(s, "%d", gyro_cali_version);
		break;
	case ACC_CALI_RANGE:
		seq_printf(s, "%s", acc_cali_range);
		break;
	case CONFIG_SAR_REG:
		seq_printf(s, "0x%x", g_reg_value);
		break;
	case SAR_NUM:
		seq_printf(s, "0x%x", g_sar_num);
		break;
	case DO_MAG_SELFTEST:
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
		ret = oplus_send_selftest_cmd_to_hub(ID_MAGNETIC_FIELD, &selftest_result);
#else
		ret = oplus_send_selftest_cmd_to_hub(ID_MAGNETIC, &selftest_result);
#endif
		if (ret < 0 || selftest_result < 0) {
			seq_printf(s, "%d", -1);
		} else {
			seq_printf(s, "%d", 0);
		}
		break;
	case GOLD_REAR_CCT_3K:
		seq_printf(s, "%s", gold_rear_cct_3k);
		DEVINFO_LOG("gold_rear_cct_3k = %s \n", gold_rear_cct_3k);
		break;
	case GOLD_REAR_CCT_6K:
		DEVINFO_LOG("gold_rear_cct_6k = %s \n", gold_rear_cct_6k);
		seq_printf(s, "%s", gold_rear_cct_6k);
		break;
	case GYRO_CALI_ADAPT_Q:
		seq_printf(s, "%d", gyro_cali_adapt_q);
		break;
	case ACC_CALI_ADAPT_Q:
		seq_printf(s, "%d", acc_cali_adapt_q);
		break;
	case GOLD_ALS_FACTOR:
		DEVINFO_LOG("gold_als_factor = %d \n", gold_als_factor);
		seq_printf(s, "%d", gold_als_factor);
		break;
	case GOLD_REAR_ALS_FACTOR:
		DEVINFO_LOG("gold_rear_als_factor = %d \n", gold_rear_als_factor);
		seq_printf(s, "%d", gold_rear_als_factor);
		break;
	case GOLD_REAR_CCT_FACTOR:
		DEVINFO_LOG("gold_rear_cct_factor = %s \n", gold_rear_cct_factor);
		seq_printf(s, "%s", gold_rear_cct_factor);
		break;
	default:
		seq_printf(s, "not support chendai\n");
	}
	return 0;
}

static ssize_t sensor_feature_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	void *buf_str = buf;
	long val = 0;
	int ret = 0;
	int result = 0;
	struct seq_file *s = filp->private_data;
	void *p = s->private;
	int node = Ptr2UINT32(p);

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf_str, ubuf, cnt)) {
			return -EFAULT;
		}
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&val);
	DEVINFO_LOG("node1 = %d,val = %d buf = %s\n", node, val, buf);

	switch (node) {
	case IS_SUPPROT_HWCALI:
		break;
	case IS_IN_FACTORY_MODE:
		switch (val) {
		case PS_FACTORY_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_PROXIMITY, 1, &result);
			break;
		case PS_NORMAL_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_PROXIMITY, 0, &result);
			break;
		case GSENSOR_FACTORY_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_ACCELEROMETER, 1, &result);
			break;
		case GSENSOR_NORMAL_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_ACCELEROMETER, 0, &result);
			break;
		case CCT_FACTORY_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_CCT, 1, &result);
			break;
		case CCT_NORMAL_MODE:
			ret = oplus_send_factory_mode_cmd_to_hub(ID_CCT, 0, &result);
			break;
		default:
			DEVINFO_LOG("ivalid sensor mode\n");
		}
		if (ret < 0 || result != 1) {
			DEVINFO_LOG("set_factory_mode fail\n");
		}
		break;
	case CONFIG_SAR_REG:
		sscanf(buf, "%x %x", &g_reg_address, &g_reg_value);
		DEVINFO_LOG("buf %s, g_reg_add = 0x%x g_reg_val = 0x%x,g_sar_num = 0x%x\n", buf, g_reg_address, g_reg_value, g_sar_num);
		ret = oplus_send_reg_config_cmd_to_hub(g_sar_num);
		if (ret < 0) {
			DEVINFO_LOG("send sar config fail\n");
		}
		break;
	case SAR_NUM:
		sscanf(buf, "%x", &val);
		DEVINFO_LOG("sar_num = 0x%x\n", val);
		g_sar_num = val;
		break;
	default:
		DEVINFO_LOG("ivalid node type\n");
	}
	return cnt;
}

static int sensor_feature_open(struct inode *inode, struct file *file)
{
	return single_open(file, sensor_feature_read_func, PDE_DATA(inode));
}

static const struct file_operations Sensor_info_fops = {
	.owner = THIS_MODULE,
	.open  = sensor_feature_open,
	.write = sensor_feature_write,
	.read  = seq_read,
	.release = single_release,
};

static int oplus_sensor_feature_init()
{
	struct proc_dir_entry *p_entry;
	static struct proc_dir_entry *oplus_sensor = NULL;

	oplus_sensor = proc_mkdir("oplusSensorFeature", NULL);
	if (!oplus_sensor) {
		DEVINFO_LOG("proc_mkdir err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("is_support_hwcali", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(IS_SUPPROT_HWCALI));
	if (!p_entry) {
		DEVINFO_LOG("is_support_hwcali err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("is_support_new_arch", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(IS_SUPPORT_NEW_ARCH));
	if (!p_entry) {
		DEVINFO_LOG("is_support_new_arch err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("is_support_mtk_origin_cali", S_IRUGO, oplus_sensor, &Sensor_info_fops,
		UINT2Ptr(IS_SUPPORT_MTK_ORIGIN_CALI));
	if (!p_entry) {
		DEVINFO_LOG("is_support_mtk_origin_cali err\n ");
		return -ENOMEM;
	}
	p_entry = proc_create_data("is_in_factory_mode", S_IRUGO | S_IWUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(IS_IN_FACTORY_MODE));
	if (!p_entry) {
		DEVINFO_LOG("is_in_factory_mode err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("gyro_cali_version", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GYRO_CALI_VERSION));
	if (!p_entry) {
		DEVINFO_LOG("gyro_cali_version err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("acc_cali_range", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(ACC_CALI_RANGE));
	if (!p_entry) {
		DEVINFO_LOG("acc_cali_range err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("do_mag_selftest", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(DO_MAG_SELFTEST));
	if (!p_entry) {
		DEVINFO_LOG("do_mag_selftest err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("config_sar_reg", S_IRUGO | S_IWUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(CONFIG_SAR_REG));
	if (!p_entry) {
		DEVINFO_LOG("config_sar_reg err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("sar_num", S_IRUGO | S_IWUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(SAR_NUM));
	if (!p_entry) {
		DEVINFO_LOG("sar_num err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("gold_rear_cct_3k", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GOLD_REAR_CCT_3K));
	if (!p_entry) {
		DEVINFO_LOG("gold_rear_cct_3k err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("gold_rear_cct_6k", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GOLD_REAR_CCT_6K));
	if (!p_entry) {
		DEVINFO_LOG("gold_rear_cct_6k err\n ");
		return -ENOMEM;
	}
	p_entry = proc_create_data("gyro_cali_adapt_q", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GYRO_CALI_ADAPT_Q));
	if (!p_entry) {
		DEVINFO_LOG("gyro_cali_adapt_q err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("acc_cali_adapt_q", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(ACC_CALI_ADAPT_Q));
	if (!p_entry) {
		DEVINFO_LOG("acc_cali_adapt_q err\n ");
		return -ENOMEM;
	}
	p_entry = proc_create_data("gold_als_factor", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GOLD_ALS_FACTOR));
	if (!p_entry) {
		DEVINFO_LOG("gold_als_factor err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("gold_rear_als_factor", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GOLD_REAR_ALS_FACTOR));
	if (!p_entry) {
		DEVINFO_LOG("gold_rear_als_factor err\n ");
		return -ENOMEM;
	}

	p_entry = proc_create_data("gold_rear_cct_factor", S_IRUGO, oplus_sensor, &Sensor_info_fops,
			UINT2Ptr(GOLD_REAR_CCT_FACTOR));
	if (!p_entry) {
		DEVINFO_LOG("gold_rear_cct_factor err\n ");
		return -ENOMEM;
	}
	return 0;
}
#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
void __attribute__((weak)) scp_A_register_notify(struct notifier_block *nb)
{

}

void __attribute__((weak)) scp_A_unregister_notify(struct notifier_block *nb)
{

}

static int scp_ready_event(struct notifier_block *this,
    unsigned long event, void *ptr)
{

	if (event == SCP_EVENT_READY) {
		DEVINFO_LOG(" receiver SCP_EVENT_READY event send cfg data\n ");
		schedule_delayed_work(&parameter_work, msecs_to_jiffies(100));
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_ready_notifier = {
	.notifier_call = scp_ready_event,
};
#endif
static int scp_utc_sync_pm_event(struct notifier_block *notifier,
    unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		atomic_set(&utc_suspend, 1);
		break;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		atomic_set(&utc_suspend, 0);
		schedule_delayed_work(&utc_work, msecs_to_jiffies(100));
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_utc_sync_notifier = {
	.notifier_call = scp_utc_sync_pm_event,
	.priority = 0,
};

static int lcdinfo_callback(struct notifier_block *nb,
    unsigned long event, void *data)
{
	int val = 0;
	if (!data) {
		return 0;
	}
	switch (event) {
	case LCM_DC_MODE_TYPE:
		val = *(bool*)data;
		if (val != g_als_info.dc_mode) {
			g_als_info.dc_mode = val;
			if (g_als_info.use_lb_algo) {
				schedule_delayed_work(&lcdinfo_work, 0);
			}
		}
		break;
	case LCM_BRIGHTNESS_TYPE:
		val = *(int*)data;
		if (val <= lb_bri_max) {
			val = val / 20 * 20;
		} else {
			val = 2048;
		}
		if (val != g_als_info.brightness) {
			g_als_info.brightness = val;
			if (g_als_info.use_lb_algo) {
				schedule_delayed_work(&lcdinfo_work, 0);
			}
		}

		break;
	default:
		break;
	}
	return 0;
}
static struct notifier_block lcdinfo_notifier = {
	.notifier_call = lcdinfo_callback,
};
static int __init sensor_devinfo_init(void)
{
	int ret = 0;

	mag_soft_parameter_init();
	get_new_arch_info();
	is_support_lb_algo();
	get_mtk_cali_origin_info();
	get_lb_max_brightness();

	ret = oplus_sensor_feature_init();
	if (ret != 0) {
		DEVINFO_LOG("oplus_sensor_feature_init err\n ");
		return -ENOMEM;
	}

	g_cali_data = kzalloc(sizeof(struct cali_data), GFP_KERNEL);
	if (!g_cali_data) {
		DEVINFO_LOG("kzalloc err\n ");
		return -ENOMEM;
	}
	g_als_info.brightness = 10000;
	g_als_info.dc_mode = 0;
    //g_als_info.use_lb_algo = true;
	atomic_set(&utc_suspend, 0);
	INIT_DELAYED_WORK(&utc_work, sync_utc2scp_work);
	schedule_delayed_work(&utc_work, msecs_to_jiffies(2000));
	register_pm_notifier(&scp_utc_sync_notifier);
	register_lcdinfo_notifier(&lcdinfo_notifier);
	INIT_DELAYED_WORK(&lcdinfo_work, transfer_lcdinfo_to_scp);
	/*init parameter*/
	INIT_DELAYED_WORK(&parameter_work, sensor_devinfo_work);
	schedule_delayed_work(&parameter_work, msecs_to_jiffies(100));
	#ifdef CONFIG_OPLUS_SENSOR_MTK68XX
	scp_A_register_notify(&scp_ready_notifier);
	#endif

	return 0;
}

late_initcall(sensor_devinfo_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Murphy");
