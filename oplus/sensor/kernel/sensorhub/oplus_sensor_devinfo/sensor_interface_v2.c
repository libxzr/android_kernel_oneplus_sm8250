
#include "sensor_interface.h"
#include "sensor_list.h"
#include "hf_sensor_type.h"
#include <linux/rtc.h>
#include "hf_manager.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <sensor_comm.h>
#include "scp.h"

static struct sensor_info sensor_list[SENSOR_TYPE_SENSOR_MAX];

extern int sensor_list_get_list(struct sensor_info *list, unsigned int num);

static oplus_send_comm_to_hub(int sensor_type, int cmd, void *data, uint8_t length)
{       
    int ret = 0;
    struct sensor_comm_ctrl *ctrl = NULL;
    
    ctrl = kzalloc(sizeof(*ctrl) + length, GFP_KERNEL);
    ctrl->sensor_type = sensor_type;
    ctrl->command = cmd;
    ctrl->length = length;
    if (length){
        memcpy(ctrl->data, data, length);
    }
    ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
    kfree(ctrl);
    return ret;
}

static int oplus_send_factory_mode_cmd_to_hub(int sensor_type, int mode, void *result)
{
    return oplus_send_comm_to_hub(sensor_type, OPLUS_ACTION_SET_FACTORY_MODE, &mode, sizeof(mode));
}

static int oplus_send_selftest_cmd_to_hub(int sensor_type, void *testresult)
{
    return oplus_send_comm_to_hub(sensor_type, OPLUS_ACTION_SELF_TEST, NULL, 0);
}

static int send_utc_time_to_hub(void)
{
    struct timespec64 tv={0};
    struct rtc_time tm;
    uint32_t utc_data[4] = {0};

    ktime_get_real_ts64(&tv);
    rtc_time64_to_tm(tv.tv_sec, &tm);

    utc_data[0] = (uint32_t)tm.tm_mday;
    utc_data[1] = (uint32_t)tm.tm_hour;
    utc_data[2] = (uint32_t)tm.tm_min;
    utc_data[3] = (uint32_t)tm.tm_sec;
    return oplus_send_comm_to_hub(0, OPLUS_ACTION_SCP_SYNC_UTC, utc_data, sizeof(utc_data));
   
}

static int send_lcdinfo_to_hub(struct als_info *lcd_info)
{
    uint32_t data = 0;
    DEVINFO_LOG("send lcd info to scp brightness %d, dc_mode %d", (uint32_t)lcd_info->brightness, (uint32_t)lcd_info->dc_mode);
    data = (uint32_t)lcd_info->brightness << 16 | lcd_info->dc_mode;
    return oplus_send_comm_to_hub(SENSOR_TYPE_RGBW, OPLUS_ACTION_SET_LCD_INFO, &data, sizeof(data));
}

static void init_sensorlist(void)
{
   int ret = 0;
   ret = sensor_list_get_list(sensor_list, SENSOR_TYPE_SENSOR_MAX);
   if (ret < 0) {
        DEVINFO_LOG("get sensor list\n");
   }
}

bool is_sensor_available(char *name)
{
    bool find = false;
    int handle = 0;

    for (handle = 0; handle < SENSOR_TYPE_SENSOR_MAX; ++handle) {
        if (name && (strstr(sensor_list[handle].name, name))) {
            find = true;
            break;
        }
    }

    return find;
}

struct sensorhub_interface sensorhub_v2 = {
    .send_factory_mode = oplus_send_factory_mode_cmd_to_hub,
    .send_selft_test = oplus_send_selftest_cmd_to_hub,
    .send_reg_config = NULL,
    .send_cfg = NULL,
    .send_utc_time = send_utc_time_to_hub,
    .send_lcdinfo = send_lcdinfo_to_hub,
    .init_sensorlist = init_sensorlist,
    .is_sensor_available = is_sensor_available,
};

void init_sensorhub_interface(struct sensorhub_interface **si)
{
    *si = &sensorhub_v2;
}
