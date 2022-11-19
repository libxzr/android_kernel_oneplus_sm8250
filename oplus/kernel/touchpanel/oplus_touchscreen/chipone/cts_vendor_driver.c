#define LOG_TAG "VENDOR"

#include <linux/firmware.h>

#include "cts_config.h"
#include "cts_core.h"
#include "cts_test.h"
#include "cts_strerror.h"
#include "cts_sysfs.h"
#include "../touchpanel_common.h"


struct chipone_ts_data *chipone_ts_data = NULL;
struct touchpanel_data *tsdata = NULL;

bool cts_show_debug_log = false;
module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control\n");


#ifdef CFG_CTS_FW_LOG_REDIRECT
size_t cts_plat_get_max_fw_log_size(struct cts_platform_data *pdata)
{
    return CTS_FW_LOG_BUF_LEN;
}

u8 *cts_plat_get_fw_log_buf(struct cts_platform_data *pdata,
    size_t size)
{
    return pdata->fw_log_buf;
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_I2C_XFER_SIZE;
}

u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata,
    size_t xfer_size)
{
        return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

    struct i2c_msg msg = {
        .addr  = i2c_addr,
        .flags = !I2C_M_RD,
        .len   = len,
        .buf   = (u8 *)src,
    };

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
        if (ret != 1) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

    struct i2c_msg msgs[2] = {
        {
            .addr  = i2c_addr,
            .flags = !I2C_M_RD,
            .buf   = (u8 *)wbuf,
            .len   = wlen
        },
        {
            .addr  = i2c_addr,
            .flags = I2C_M_RD,
            .buf   = (u8 *)rbuf,
            .len   = rlen
        }
    };

    if (wbuf == NULL || wlen == 0) {
        num_msg = 1;
    } else {
        num_msg = 2;
    }

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

        if (ret != num_msg) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_is_i2c_online(struct cts_platform_data *pdata, u8 i2c_addr)
{
    u8 dummy_bytes[2] = {0x00, 0x00};
    int ret;

    ret = cts_plat_i2c_write(pdata, i2c_addr, dummy_bytes, sizeof(dummy_bytes), 5, 2);
    if (ret) {
        TPD_INFO("<E> !!! I2C addr 0x%02x is offline !!!\n", i2c_addr);
        return false;
    } else {
        TPD_DEBUG("<D> I2C addr 0x%02x is online\n", i2c_addr);
        return true;
    }
}
#else

#ifdef CFG_MTK_LEGEND_PLATFORM
struct mt_chip_conf cts_spi_conf_mt65xx = {
    .setuptime = 15,
    .holdtime = 15,
    .high_time = 21, //for mt6582, 104000khz/(4+4) = 130000khz
    .low_time = 21,
    .cs_idletime = 20,
    .ulthgh_thrsh = 0,

    .cpol = 0,
    .cpha = 0,

    .rx_mlsb = 1,
    .tx_mlsb = 1,

    .tx_endian = 0,
    .rx_endian = 0,

    .com_mod = FIFO_TRANSFER,
    .pause = 1,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};

typedef enum {
    SPEED_500KHZ = 500,
    SPEED_1MHZ = 1000,
    SPEED_2MHZ = 2000,
    SPEED_3MHZ = 3000,
    SPEED_4MHZ = 4000,
    SPEED_6MHZ = 6000,
    SPEED_8MHZ = 8000,
    SPEED_KEEP,
    SPEED_UNSUPPORTED
} SPI_SPEED;

void cts_plat_spi_set_mode(struct spi_device *spi, SPI_SPEED speed, int flag)
{
    struct mt_chip_conf *mcc = &cts_spi_conf_mt65xx;
    if (flag == 0) {
        mcc->com_mod = FIFO_TRANSFER;
    } else {
        mcc->com_mod = DMA_TRANSFER;
    }

    switch (speed) {
    case SPEED_500KHZ:
        mcc->high_time = 120;
        mcc->low_time = 120;
        break;
    case SPEED_1MHZ:
        mcc->high_time = 60;
        mcc->low_time = 60;
        break;
    case SPEED_2MHZ:
        mcc->high_time = 30;
        mcc->low_time = 30;
        break;
    case SPEED_3MHZ:
        mcc->high_time = 20;
        mcc->low_time = 20;
        break;
    case SPEED_4MHZ:
        mcc->high_time = 15;
        mcc->low_time = 15;
        break;
    case SPEED_6MHZ:
        mcc->high_time = 10;
        mcc->low_time = 10;
        break;
    case SPEED_8MHZ:
        mcc->high_time = 8;
        mcc->low_time = 8;
        break;
    case SPEED_KEEP:
    case SPEED_UNSUPPORTED:
        break;
    }
    if (spi_setup(spi) < 0) {
        TPD_INFO("<E> Failed to set spi\n");
    }
}

int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;
//  pdata->spi_client->chip_select = 0;
    pdata->spi_client->controller_data = (void *)&cts_spi_conf_mt65xx;
    spi_setup(pdata->spi_client);
    cts_plat_spi_set_mode(pdata->spi_client, pdata->spi_speed, 0);
    return 0;
}
#else
int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;

    spi_setup(pdata->spi_client);
    return 0;
}
#endif

int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len,
        u8 *tx_buffer, u8 *rx_buffer)
{
    struct chipone_ts_data *cts_data;
    struct spi_message msg;
    struct spi_transfer cmd = {
        .cs_change = 0,
        .delay_usecs = 0,
        .speed_hz = pdata->spi_speed * 1000,
        .tx_buf = tx_buffer,
        .rx_buf = rx_buffer,
        .len    = len,
        //.tx_dma = 0,
        //.rx_dma = 0,
        .bits_per_word = 8,
    };
    int ret = 0 ;
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    spi_message_init(&msg);
    spi_message_add_tail(&cmd,  &msg);
    ret = spi_sync(cts_data->spi_client, &msg);
    if (ret) {
        TPD_INFO("<E> spi_sync failed.\n");
    }

    return ret;
}

size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_SPI_XFER_SIZE;
}

u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata,
    size_t xfer_size)
{
    return pdata->spi_cache_buf;
}

int cts_plat_spi_write(struct cts_platform_data *pdata, u8 dev_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;
    u16 crc;
    size_t data_len;

    if (len > CFG_CTS_MAX_SPI_XFER_SIZE) {
        TPD_INFO("<E> write too much data:wlen=%zd\n", len);
        return -EIO;
    }

    if (pdata->cts_dev->rtdata.program_mode) {
        pdata->spi_tx_buf[0] = dev_addr;
        memcpy(&pdata->spi_tx_buf[1], src, len);

        do {
            ret = cts_spi_send_recv(pdata, len + 1, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
            } else {
                return 0;
            }
        } while (++retries < retry);
    }
    else {
        data_len = len - 2;
        pdata->spi_tx_buf[0] = dev_addr;
        pdata->spi_tx_buf[1] = *((u8 *)src + 1);
        pdata->spi_tx_buf[2] = *((u8 *)src);
        put_unaligned_le16(data_len, &pdata->spi_tx_buf[3]);
        crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
        put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
        memcpy(&pdata->spi_tx_buf[7], (char *)src + 2, data_len);
        crc = (u16)cts_crc32((char *)src + 2, data_len);
        put_unaligned_le16(crc, &pdata->spi_tx_buf[7+data_len]);
        do {
            ret = cts_spi_send_recv(pdata, len + 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
            udelay(10 * data_len);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
            } else {
                return 0;
            }
        } while (++retries < retry);
    }
    return ret;
}

int cts_plat_spi_read(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int ret = 0, retries = 0;
    u16 crc;

    if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
        TPD_INFO("<E> write/read too much data:wlen=%zd, rlen=%zd\n", wlen, rlen);
        return -EIO;
    }

    if (pdata->cts_dev->rtdata.program_mode)
    {
        pdata->spi_tx_buf[0] = dev_addr | 0x01;
        memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
        do {
            ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf+5, rlen);
            return 0;
        } while(++retries < retry);
    }
    else {
        do {
            if (wlen != 0) {
                pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
                ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
                if (ret) {
                    if (delay) {
                        mdelay(delay);
                    }
                    continue;
                }
            }
            memset(pdata->spi_tx_buf, 0, 7);
            pdata->spi_tx_buf[0] = dev_addr | 0x01;
            udelay(100);
                ret = cts_spi_send_recv(pdata, rlen + 2, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf, rlen);
            crc = (u16)cts_crc32(pdata->spi_rx_buf, rlen);
            if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc) {
                continue;
            }
            return 0;
        } while (++retries < retry);
    }
    if (retries >= retry) {
        TPD_INFO("<E> cts_plat_spi_read error\n");
    }

    return -ENODEV;
}

int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay, int idle)
{
    int ret = 0, retries = 0;
    u16 crc;

    if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
        TPD_INFO("<E> write/read too much data:wlen=%zd, rlen=%zd\n", wlen, rlen);
        return -EIO;
    }

    if (pdata->cts_dev->rtdata.program_mode)
    {
        pdata->spi_tx_buf[0] = dev_addr | 0x01;
        memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
        do {
            ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf+5, rlen);
            return 0;
        } while(++retries < retry);
    }
    else {
        do {
            if (wlen != 0) {
                pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
                ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
                if (ret) {
                    if (delay) {
                        mdelay(delay);
                    }
                    continue;
                }
            }
            memset(pdata->spi_tx_buf, 0, 7);
            pdata->spi_tx_buf[0] = dev_addr | 0x01;
            udelay(idle);
            ret = cts_spi_send_recv(pdata, rlen + 2, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
                continue;
            }
            memcpy(rbuf, pdata->spi_rx_buf, rlen);
            crc = (u16)cts_crc32(pdata->spi_rx_buf, rlen);
            if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc) {
               continue;
            }
            return 0;
        } while (++retries < retry);
    }
    if (retries >= retry) {
        TPD_INFO("<E> cts_plat_spi_read error\n");
    }

    return -ENODEV;
}

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
    struct chipone_ts_data *cts_data;
    u8 tx_buf[4] = {0};
    u16 fwid;
    u32 addr;
    int ret;

    pdata->cts_dev->rtdata.addr_width = 2;
    pdata->cts_dev->rtdata.program_mode = false;
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    addr = CTS_DEVICE_FW_REG_CHIP_TYPE;
    put_unaligned_be16(addr, tx_buf);
    ret = cts_plat_spi_read(pdata, CTS_DEV_NORMAL_MODE_SPIADDR, tx_buf, 2, &fwid, 2, 3, 10);
    fwid = be16_to_cpu(fwid);
    if (ret || !cts_is_fwid_valid(fwid)) {
        return false;
    }

    return true;
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi)
#endif
{
    int ret = 0;

    TPD_INFO("<I> Init\n");

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
#else
    pdata->spi_client = spi;
#endif

    mutex_init(&pdata->dev_lock);

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
    pdata->i2c_client->irq = pdata->irq;
#else
    pdata->spi_client = spi;
    pdata->spi_client->irq = pdata->irq;
#endif /* CONFIG_CTS_I2C_HOST */

    spin_lock_init(&pdata->irq_lock);

#ifndef CONFIG_CTS_I2C_HOST
    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);
#endif
    return ret;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_reset_device(struct cts_platform_data *pdata)
{
    TPD_INFO("<I> Reset device\n");

    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(1);
    gpio_set_value(pdata->rst_gpio, 0);
    mdelay(10);
    gpio_set_value(pdata->rst_gpio, 1);
    mdelay(40);
    return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    TPD_INFO("<I> Set reset-pin to %s\n", val ? "HIGH" : "LOW");
    if (val) {
        gpio_set_value(pdata->rst_gpio, 1);
    } else {
        gpio_set_value(pdata->rst_gpio, 0);
    }
    return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
    /* MTK Platform can not get INT pin value */
    return -ENOTSUPP;
}

static int cts_get_chip_info(void *chip_data)
{
    TPD_INFO("<E> %s\n", tsdata->panel_data.manufacture_info.version);

    return 0;
}

static int cts_rotative_switch(void *chip_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int direct;
    int ret = 0;

    direct = cts_data->touch_direction;

    TPD_INFO("<I> direct:%d\n", direct);

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_LANDSCAPE_MODE, direct);
    if (ret) {
        TPD_INFO("<E> Set direct: %d, failed! %d(%s)\n",
                direct, ret, cts_strerror(ret));
    }
    return ret;
}

static int cts_mode_switch(void *chip_data, work_mode mode, bool flag)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int ret;

    cts_lock_device(cts_dev);
    switch (mode) {
        case MODE_NORMAL:
            break;
        case MODE_SLEEP:
            TPD_INFO("<I> switch MODE_SLEEP, SUSPEND\n");
            cts_plat_reset_device(cts_data->pdata);
            msleep(50);
            cts_send_command(cts_dev, CTS_CMD_SUSPEND);
            break;
        case MODE_EDGE:
            TPD_INFO("<I> switch MODE_EDGE\n");
            ret = cts_rotative_switch(chip_data);
            if (ret)
                TPD_INFO("<E> Set rotative switch failed %d(%s)\n",
                        ret, cts_strerror(ret));
            break;
        case MODE_GESTURE:
            if (flag) {
                TPD_INFO("<I> switch MODE_GESTURE, SUSPEND_WITH_GESTURE\n");
                cts_plat_reset_device(cts_data->pdata);
                msleep(50);
                cts_send_command(cts_dev, CTS_CMD_SUSPEND_WITH_GESTURE);
            }
            break;
        case MODE_CHARGE:
            TPD_INFO("<I> switch MODE_CHARGE, flag: %d\n", flag);
            ret = cts_set_dev_charger_attached(cts_dev, flag);
            if (ret)
                TPD_INFO("<E> Set charger mode failed %d(%s)\n",
                        ret, cts_strerror(ret));
            break;
        case MODE_GAME:
            TPD_INFO("<I> switch MODE_GAME: %s\n", flag ? "In" : "Out");
            ret = cts_fw_reg_writeb(cts_dev, 0x086E, flag ? 1 : 0);
            if (ret)
                TPD_INFO("<E> Set dev game mode failed %d(%s)\n",
                 ret, cts_strerror(ret));
            break;
        case MODE_HEADSET:
            TPD_INFO("<I> switch MODE_HEADSET, flag: %d\n", flag);
            ret = cts_set_dev_earjack_attached(cts_dev, flag);
            if (ret)
                TPD_INFO("<E> Set earjack mode failed %d(%s)\n",
                        ret, cts_strerror(ret));
            break;
        default:
            break;
    }
    cts_unlock_device(cts_dev);

    return ret;
}

static u8 cts_trigger_reason(void *chip_data, int gesture_enable,
        int is_suspended)
{
    if (gesture_enable && is_suspended) {
        return IRQ_GESTURE;
    } else {
        return IRQ_TOUCH;
    }
}

static int cts_get_touch_points(void *chip_data,
        struct point_info *points, int max_num)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_device_touch_info *touch_info = &cts_dev->pdata->touch_info;
    struct cts_device_touch_msg *msgs = touch_info->msgs;
    int num = touch_info->num_msg;
    int obj_attention = 0;
    int ret = -1;
    int i;

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> IRQ triggered in program mode\n");
        return -EINVAL;
    }

    cts_lock_device(cts_dev);
    ret = cts_get_touchinfo(cts_dev, touch_info);
    cts_unlock_device(cts_dev);
    if (ret < 0) {
        TPD_INFO("<E> Get touch info failed %d(%s)\n", ret, cts_strerror(ret));
        return ret;
    }

    TPD_DEBUG("<D> Process touch %d msgs\n", num);
    if (num == 0 || num > CFG_CTS_MAX_TOUCH_NUM) {
        return 0;
    }

    for (i = 0; i < (num > max_num ? max_num : num); i++) {
        u16 x, y;

        x = le16_to_cpu(msgs[i].x);
        y = le16_to_cpu(msgs[i].y);

        TPD_DEBUG("<D>   Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u\n",
                i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
        if ((msgs[i].id < max_num) &&
                ((msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN) ||
                (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE) ||
                (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY)))
        {
            points[msgs[i].id].x = x;
            points[msgs[i].id].y = y;
            points[msgs[i].id].z = 1;
            points[msgs[i].id].width_major = 0;
            points[msgs[i].id].touch_major = 0;
            points[msgs[i].id].status = 1;
            obj_attention |= (1 << msgs[i].id);
        }
    }

    return obj_attention;
}

static int cts_handle_gesture_info(void *chip_data,
        struct gesture_info *gesture)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_device_gesture_info cts_gesture_info;
    uint32_t gesture_type;
    int ret = -1;

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> IRQ triggered in program mode\n");
        return -EINVAL;
    }

    cts_lock_device(cts_dev);
    ret = cts_get_gesture_info(cts_dev, &cts_gesture_info, false);
    if (ret) {
        TPD_INFO("<W> Get gesture info failed %d(%s)\n", ret, cts_strerror(ret));
        cts_unlock_device(cts_dev);
        return ret;
    }

    /** - Issure another suspend with gesture wakeup command to device
     * after get gesture info.
     */
    TPD_INFO("<I> Set device enter gesture mode\n");
    cts_send_command(cts_dev, CTS_CMD_SUSPEND_WITH_GESTURE);
    cts_unlock_device(cts_dev);

    TPD_INFO("<I> Process gesture, id=0x%02x, num_points=%d\n",
            cts_gesture_info.gesture_id, cts_gesture_info.num_points);

    memset(gesture, 0, sizeof(*gesture));
    switch(cts_gesture_info.gesture_id) {
    case CTS_GESTURE_D_TAP: gesture_type = DouTap; break;
    case CTS_GESTURE_V:     gesture_type = UpVee; break;
    case CTS_GESTURE_M:     gesture_type = Mgestrue; break;
    case CTS_GESTURE_W:     gesture_type = Wgestrue; break;
    case CTS_GESTURE_O:     gesture_type = Circle; break;
    case CTS_GESTURE_RV:    gesture_type = DownVee; break;
    case CTS_GESTURE_UP:    gesture_type = Down2UpSwip; break;
    case CTS_GESTURE_DOWN:  gesture_type = Up2DownSwip; break;
    case CTS_GESTURE_LEFT:  gesture_type = Right2LeftSwip; break;
    case CTS_GESTURE_RIGHT: gesture_type = Left2RightSwip; break;
    case CTS_GESTURE_DDOWN: gesture_type = DouSwip; break;
    case CTS_GESTURE_LR:    gesture_type = RightVee; break;
    case CTS_GESTURE_RR:    gesture_type = LeftVee; break;
    default: gesture_type = UnkownGesture; break;
    }

    gesture->gesture_type = gesture_type;

    if (cts_gesture_info.num_points >= 1) {
        gesture->Point_start.x = cts_gesture_info.points[0].x;
        gesture->Point_start.y = cts_gesture_info.points[0].y;
    }
    if (cts_gesture_info.num_points >= 2) {
        gesture->Point_end.x = cts_gesture_info.points[1].x;
        gesture->Point_end.y = cts_gesture_info.points[1].y;
    }
    if (cts_gesture_info.num_points >= 3) {
        gesture->Point_1st.x = cts_gesture_info.points[2].x;
        gesture->Point_1st.y = cts_gesture_info.points[2].y;
    }
    if (cts_gesture_info.num_points >= 4) {
        gesture->Point_2nd.x = cts_gesture_info.points[3].x;
        gesture->Point_2nd.y = cts_gesture_info.points[3].y;
    }
    if (cts_gesture_info.num_points >= 5) {
        gesture->Point_3rd.x = cts_gesture_info.points[4].x;
        gesture->Point_3rd.y = cts_gesture_info.points[4].y;
    }
    if (cts_gesture_info.num_points >= 6) {
        gesture->Point_4th.x = cts_gesture_info.points[5].x;
        gesture->Point_4th.y = cts_gesture_info.points[5].y;
    }
    return 0;
}

/* Used In Factory Mode, Not Need TP Work!!! */
static int cts_ftm_process(void *chip_data) {
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int ret;

    cts_lock_device(cts_dev);
    ret = cts_send_command(cts_dev, CTS_CMD_SUSPEND);
    cts_unlock_device(cts_dev);
    if (ret)
        TPD_INFO("<E> Send CMD_SUSPEND failed %d(%s)\n",
                ret, cts_strerror(ret));
    return 0;
}

static int cts_read_fw_ddi_version(void *chip_data, struct touchpanel_data *ts)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 fw_ver;
    u8 ddi_ver;
    char *version;
    int ret = -1;

    version = ts->panel_data.manufacture_info.version;
    if (version) {
        ret = cts_get_ddi_version(cts_dev, &ddi_ver);
        if (ret)
            TPD_INFO("<E> get ddi version failed %d(%s)\n",
                    ret, cts_strerror(ret));

        ret = cts_get_firmware_version(cts_dev, &fw_ver);
        if (ret)
            TPD_INFO("<E> get firmware version failed %d(%s)\n",
                    ret, cts_strerror(ret));

        scnprintf(version, 16, "TXD_9911C_%X%02X", ddi_ver, fw_ver);
        TPD_INFO("<I> %s\n", version);
    }

    return ret;
}

static fw_update_state cts_fw_update(void *chip_data, const struct firmware *fw,
        bool force)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int ret;

    /* If fw was not NULL, then need to update vfw.data buf. */
    if (fw) {
        if (cts_data->vfw.data) {
            memcpy(cts_data->vfw.data, fw->data, fw->size);
            cts_data->vfw.name = "ICNL9911.bin";
            cts_data->vfw.hwid = CTS_DEV_HWID_ICNL9911C;
            cts_data->vfw.fwid = CTS_DEV_FWID_ICNL9911C;
            cts_data->vfw.size = fw->size;
        }
    }

    cts_lock_device(cts_dev);
    ret = cts_update_firmware(cts_dev, &cts_data->vfw, false);
    cts_unlock_device(cts_dev);
    if (ret < 0) {
        return FW_UPDATE_ERROR;
    }

    if (cts_enter_normal_mode(cts_dev) < 0) {
        return FW_UPDATE_ERROR;
    }

    cts_lock_device(cts_dev);
    ret = cts_read_fw_ddi_version(chip_data, tsdata);
    cts_unlock_device(cts_dev);
    if (ret)
        TPD_INFO("<E> get fw_ddi_version failed %d(%s)\n",
                ret, cts_strerror(ret));

    return FW_UPDATE_SUCCESS;
}


static int cts_get_vendor(void *chip_data, struct panel_info  *panel_data)
{
    return 0;
}

static int cts_reset(void *chip_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    int ret;

    cts_plat_reset_device(cts_data->pdata);

    ret = cts_fw_update(chip_data, NULL, false);
    if (ret == FW_UPDATE_SUCCESS) {
        TPD_INFO("<I> fw_update success!\n");
    } else if (ret == FW_UPDATE_ERROR) {
        TPD_INFO("<E> fw_update failed!\n");
    }

    return 0;
}

static void cts_tp_touch_release(struct touchpanel_data *ts)
{
#ifdef TYPE_B_PROTOCOL
    int i = 0;

    if (ts->report_flow_unlock_support) {
        mutex_lock(&ts->report_mutex);
    }
    for (i = 0; i < ts->max_num; i++) {
        input_mt_slot(ts->input_dev, i);
        input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
    }
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
    input_sync(ts->input_dev);
    if (ts->report_flow_unlock_support) {
        mutex_unlock(&ts->report_mutex);
    }
#else
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
    input_mt_sync(ts->input_dev);
    input_sync(ts->input_dev);
#endif
    TPD_INFO("<I> release all touch point\n");
    ts->view_area_touched = 0;
    ts->touch_count = 0;
    ts->irq_slot = 0;
}

static int cts_esd_handle(void* chip_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    int retry = 5;
    int ret;

    TPD_DEBUG("<D> ESD protection work\n");

    cts_lock_device(cts_dev);
    /* ret==0:means esd hanppened */
    ret = cts_plat_is_normal_mode(cts_data->pdata);
    cts_unlock_device(cts_dev);
    if (!ret) {
        TPD_INFO("<E> Handle ESD event!\n");
        do {
            if (cts_reset(chip_data))
                TPD_INFO("<E> Reset chip and update fw failed!\n");
            else
                break;
        } while (retry--);
        ret = -1;
        cts_tp_touch_release(tsdata);
    } else {
        ret = 0;
        TPD_DEBUG("<D> None ESD event!\n");
    }

    return ret;
}

static fw_check_state cts_fw_check(void *chip_data,
        struct resolution_info *resolution_info,
        struct panel_info *panel_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    u16 fwid;

    TPD_INFO("<I> Enter %s!\n", __func__);

    if (cts_get_fwid(cts_dev, &fwid) < 0) {
        return FW_ABNORMAL;
    }
    if (fwid == CTS_DEV_FWID_INVALID) {
        return FW_ABNORMAL;
    }

    return FW_NORMAL;
}

static int cts_power_control(void *chip_data, bool enable)
{
    return 0;
}


#pragma pack(1)
struct cts_range_threshold {
    char  header[7];
    char  version;
    struct {
        unsigned baseline_int_pin:1;
        unsigned baseline_reset_pin:1;
        unsigned baseline_rawdata:1;
        unsigned baseline_noise:1;
        unsigned baseline_open:1;
        unsigned baseline_short:1;
        unsigned baseline_comp_cap:1;
        unsigned black_rawdata:1;
        unsigned black_lp_rawdata:1;
        unsigned black_noise:1;
        unsigned black_lp_noise:1;
        unsigned :4;
        unsigned threshold_type :1;
    };
    short baseline_raw_frames;
    short baseline_raw_min;
    short baseline_raw_max;
    short baseline_noise_frames;
    short baseline_noise_max;
    short baseline_open_min;
    short baseline_short_min;
    short baseline_compcap_min;
    short baseline_compcap_max;
    short black_raw_frames;
    short black_raw_min;
    short black_raw_max;
    short black_lp_raw_frames;
    short black_lp_raw_min;
    short black_lp_raw_max;
    short black_noise_frames;
    short black_noise_max;
    short black_lp_noise_frames;
    short black_lp_noise_max;
};
#pragma pack()

/** Baseline Test Part */
struct cts_vendor_data {
    bool test_reset_pin;
    bool test_int_pin;
    bool test_rawdata;
    bool test_noise;
    bool test_open;
    bool test_short;
    bool test_comp_cap;

    u32  rawdata_test_frames;
    int  rawdata_min;
    int  rawdata_max;
    u32  noise_test_frames;
    int  noise_max;
    int  open_min;
    int  short_min;
    int  comp_cap_min;
    int  comp_cap_max;

    bool black_rawdata;
    bool black_lp_rawdata;
    bool black_noise;
    bool black_lp_noise;

    int  black_raw_frames;
    int  black_raw_min;
    int  black_raw_max;
    int  black_lp_raw_frames;
    int  black_lp_raw_min;
    int  black_lp_raw_max;
    int  black_noise_frames;
    int  black_noise_max;
    int  black_lp_noise_frames;
    int  black_lp_noise_max;


    int  reset_pin_test_result;
    int  int_pin_test_result;

    int  rawdata_test_result;
    void *rawdata_test_data;
    int  rawdata_test_data_buf_size;
    int  rawdata_test_data_size;
    char *rawdata_data_filepath;

    int  noise_test_result;
    void *noise_test_data;
    int  noise_test_data_buf_size;
    int  noise_test_data_size;
    char *noise_data_filepath;

    int  open_test_result;
    void *open_test_data;
    int  open_test_data_buf_size;
    int  open_test_data_size;
    char *open_data_filepath;

    int  short_test_result;
    void *short_test_data;
    int  short_test_data_buf_size;
    int  short_test_data_size;
    char *short_data_filepath;

    int  comp_cap_test_result;
    void *comp_cap_test_data;
    int  comp_cap_test_data_buf_size;
    int  comp_cap_test_data_size;
    char *comp_cap_data_filepath;

    struct chipone_ts_data *cts_data;
};

#define ALLOC_TEST_DATA_MEM(type, size) \
    do { \
        if (vdata->test_##type) { \
            if (vdata->type##_test_data == NULL) { \
                TPD_INFO("<I> - Alloc " #type " test data mem size %d\n", size); \
                vdata->type##_test_data = vmalloc(size); \
                if (vdata->type##_test_data == NULL) { \
                    TPD_INFO("<E> Alloc " #type " test data mem failed\n"); \
                    return -ENOMEM; \
                } \
                vdata->type##_test_data_size = size; \
            } \
            memset(vdata->type##_test_data, 0, size); \
        } \
    } while (0)

#define FREE_TEST_DATA_MEM(type) \
    do { \
        if (vdata->type##_test_data) { \
            TPD_INFO("<I> - Free " #type " test data mem\n"); \
            vfree(vdata->type##_test_data); \
            vdata->type##_test_data = NULL; \
        } \
    } while(0)

static int alloc_baseline_test_data_mem(struct cts_vendor_data *vdata, int nodes)
{
    TPD_INFO("<I> Alloc baseline test data mem\n");

    ALLOC_TEST_DATA_MEM(rawdata,
        nodes * 2 * vdata->rawdata_test_frames);
    ALLOC_TEST_DATA_MEM(noise,
        nodes * 2 * vdata->noise_test_frames + 1);
    ALLOC_TEST_DATA_MEM(open, nodes * 2);
    ALLOC_TEST_DATA_MEM(short, nodes * 2 * 7);
    ALLOC_TEST_DATA_MEM(comp_cap, nodes);

    return 0;
}

static void free_baseline_test_data_mem(struct cts_vendor_data *vdata)
{
    TPD_INFO("<I> Free baseline test data mem\n");

    FREE_TEST_DATA_MEM(rawdata);
    FREE_TEST_DATA_MEM(noise);
    FREE_TEST_DATA_MEM(open);
    FREE_TEST_DATA_MEM(short);
    FREE_TEST_DATA_MEM(comp_cap);
}
#undef ALLOC_TEST_DATA_MEM
#undef FREE_TEST_DATA_MEM

static int init_test_param(struct cts_vendor_data *vdata)
{
    struct cts_range_threshold threshold;
    const struct firmware *fw = NULL;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int ret = 0;

    ret = request_firmware(&fw, tsdata->panel_data.test_limit_name, tsdata->dev);
    if (ret < 0) {
        TPD_INFO("<E> Request %s failed", tsdata->panel_data.test_limit_name);
        return ret;
    }
    ret = memcmp(header, fw->data, sizeof(header));
    if (ret) {
        TPD_INFO("<E> LIMIT file was not matched");
        release_firmware(fw);
        return ret;
    } else {
        TPD_INFO("Matched limit file!");
    }

    memcpy(&threshold, fw->data, fw->size);
    release_firmware(fw);

    TPD_INFO("<I> Init baseline test param\n");

    vdata->test_int_pin             = !!threshold.baseline_int_pin;
    vdata->test_reset_pin           = !!threshold.baseline_reset_pin;
    vdata->test_rawdata             = !!threshold.baseline_rawdata;
    vdata->test_noise               = !!threshold.baseline_noise;
    vdata->test_open                = !!threshold.baseline_open;
    vdata->test_short               = !!threshold.baseline_short;
    vdata->test_comp_cap            = !!threshold.baseline_comp_cap;

    vdata->rawdata_test_frames      = threshold.baseline_raw_frames;
    vdata->rawdata_min              = threshold.baseline_raw_min;
    vdata->rawdata_max              = threshold.baseline_raw_max;
    vdata->noise_test_frames        = threshold.baseline_noise_frames;
    vdata->noise_max                = threshold.baseline_noise_max;
    vdata->open_min                 = threshold.baseline_open_min;
    vdata->short_min                = threshold.baseline_short_min;
    vdata->comp_cap_min             = threshold.baseline_compcap_min;
    vdata->comp_cap_max             = threshold.baseline_compcap_max;

    vdata->black_rawdata            = !!threshold.black_rawdata;
    vdata->black_lp_rawdata         = !!threshold.black_lp_rawdata;
    vdata->black_noise              = !!threshold.black_noise;
    vdata->black_lp_noise           = !!threshold.black_lp_noise;

    vdata->black_raw_frames         = threshold.black_raw_frames;
    vdata->black_raw_min            = threshold.black_raw_min;
    vdata->black_raw_max            = threshold.black_raw_max;
    vdata->black_lp_raw_frames      = threshold.black_lp_raw_frames;
    vdata->black_lp_raw_min         = threshold.black_lp_raw_min;
    vdata->black_lp_raw_max         = threshold.black_lp_raw_max;
    vdata->black_noise_frames       = threshold.black_noise_frames;
    vdata->black_noise_max          = threshold.black_noise_max;
    vdata->black_lp_noise_frames    = threshold.black_lp_noise_frames;
    vdata->black_lp_noise_max       = threshold.black_lp_noise_max;

    TPD_INFO("%s %s: %d\n", header, "Limit Bin version", threshold.version);
    TPD_INFO("test_int_pin flag:%d\n", vdata->test_int_pin);
    TPD_INFO("test_reset_pin flag:%d\n", vdata->test_reset_pin);
    TPD_INFO("test_rawdata flag:%d\n", vdata->test_rawdata);
    TPD_INFO("test_noise flag:%d\n", vdata->test_noise);
    TPD_INFO("test_open flag:%d\n", vdata->test_open);
    TPD_INFO("test_short flag:%d\n", vdata->test_short);
    TPD_INFO("test_comp_cap flag:%d\n", vdata->test_comp_cap);

    TPD_INFO("rawdata_test_frames:%d\n", vdata->rawdata_test_frames);
    TPD_INFO("rawdata_min:%d\n", vdata->rawdata_min);
    TPD_INFO("rawdata_max:%d\n", vdata->rawdata_max);
    TPD_INFO("noise_test_frames:%d\n", vdata->noise_test_frames);
    TPD_INFO("noise_max:%d\n", vdata->noise_max);
    TPD_INFO("open_min:%d\n", vdata->open_min);
    TPD_INFO("short_min:%d\n", vdata->short_min);
    TPD_INFO("comp_cap_min:%d\n", vdata->comp_cap_min);
    TPD_INFO("comp_cap_max:%d\n", vdata->comp_cap_max);

    TPD_INFO("black_rawdata flag:%d\n", vdata->black_rawdata);
    TPD_INFO("black_lp_rawdata flag:%d\n", vdata->black_lp_rawdata);
    TPD_INFO("black_noise flag:%d\n", vdata->black_noise);
    TPD_INFO("black_lp_noise flag:%d\n", vdata->black_lp_noise);

    TPD_INFO("black_raw_frames:%d\n", vdata->black_raw_frames);
    TPD_INFO("black_raw_min:%d\n", vdata->black_raw_min);
    TPD_INFO("black_raw_max:%d\n", vdata->black_raw_max);
    TPD_INFO("black_lp_raw_frames:%d\n", vdata->black_lp_raw_frames);
    TPD_INFO("black_lp_raw_min:%d\n", vdata->black_lp_raw_min);
    TPD_INFO("black_lp_raw_max:%d\n", vdata->black_lp_raw_max);
    TPD_INFO("black_noise_frames:%d\n", vdata->black_noise_frames);
    TPD_INFO("black_noise_max:%d\n", vdata->black_noise_max);
    TPD_INFO("black_lp_noise_frames:%d\n", vdata->black_lp_noise_frames);
    TPD_INFO("black_lp_noise_max:%d\n", vdata->black_lp_noise_max);

    return ret;
}

static void do_baseline_test(struct cts_vendor_data *vdata)
{
#define TOUCH_DATA_DIRECTORY_PREFIX     "/sdcard/chipone-tddi/test/baseline_data_"
#define RAWDATA_TEST_DATA_FILENAME      "rawdata.txt"
#define NOISE_TEST_DATA_FILENAME        "noise.txt"
#define OPEN_TEST_DATA_FILENAME         "open.txt"
#define SHORT_TEST_DATA_FILENAME        "short.txt"
#define COMP_CAP_TEST_DATA_FILENAME     "comp-cap.txt"
    struct cts_device *cts_dev = &vdata->cts_data->cts_dev;
    struct cts_test_param reset_pin_test_param = {
        .test_item = CTS_TEST_RESET_PIN,
        .flags = 0,
    };

    struct cts_test_param int_pin_test_param = {
        .test_item = CTS_TEST_INT_PIN,
        .flags = 0,
    };

    struct cts_rawdata_test_priv_param rawdata_test_priv_param = {0};
    struct cts_test_param rawdata_test_param = {
        .test_item = CTS_TEST_RAWDATA,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &rawdata_test_priv_param,
        .priv_param_size = sizeof(rawdata_test_priv_param),
    };
    struct cts_noise_test_priv_param noise_test_priv_param = {0};
    struct cts_test_param noise_test_param = {
        .test_item = CTS_TEST_NOISE,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &noise_test_priv_param,
        .priv_param_size = sizeof(noise_test_priv_param),
    };
    struct cts_test_param open_test_param = {
        .test_item = CTS_TEST_OPEN,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
    };
    struct cts_test_param short_test_param = {
        .test_item = CTS_TEST_SHORT,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
    };
    struct cts_test_param comp_cap_test_param = {
        .test_item = CTS_TEST_COMPENSATE_CAP,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
    };
    struct timeval  ktv;
    struct rtc_time rtc_tm;
    char touch_data_filepath[256];
    ktime_t start_time, end_time, delta_time;

    rawdata_test_priv_param.frames = vdata->rawdata_test_frames;
    rawdata_test_param.min = &vdata->rawdata_min;
    rawdata_test_param.max = &vdata->rawdata_max;
    rawdata_test_param.test_data_buf = vdata->rawdata_test_data;
    rawdata_test_param.test_data_buf_size = vdata->rawdata_test_data_buf_size;
    vdata->rawdata_test_data_size = 0;
    rawdata_test_param.test_data_wr_size = &vdata->rawdata_test_data_size;

    noise_test_priv_param.frames = vdata->noise_test_frames;
    noise_test_param.max = &vdata->noise_max;
    noise_test_param.test_data_buf = vdata->noise_test_data;
    noise_test_param.test_data_buf_size = vdata->noise_test_data_buf_size;
    vdata->noise_test_data_size = 0;
    noise_test_param.test_data_wr_size = &vdata->noise_test_data_size;

    open_test_param.min = &vdata->open_min;
    open_test_param.test_data_buf = vdata->open_test_data;
    open_test_param.test_data_buf_size = vdata->open_test_data_buf_size;
    vdata->open_test_data_size = 0;
    open_test_param.test_data_wr_size = &vdata->open_test_data_size;

    short_test_param.min = &vdata->short_min;
    short_test_param.test_data_buf = vdata->short_test_data;
    short_test_param.test_data_buf_size = vdata->short_test_data_buf_size;
    vdata->short_test_data_size = 0;
    short_test_param.test_data_wr_size = &vdata->short_test_data_size;

    comp_cap_test_param.min = &vdata->comp_cap_min;
    comp_cap_test_param.max = &vdata->comp_cap_max;
    comp_cap_test_param.test_data_buf = vdata->comp_cap_test_data;
    comp_cap_test_param.test_data_buf_size = vdata->comp_cap_test_data_buf_size;
    vdata->short_test_data_size = 0;
    comp_cap_test_param.test_data_wr_size = &vdata->short_test_data_size;

    do_gettimeofday(&ktv);
    ktv.tv_sec -= sys_tz.tz_minuteswest * 60;
    rtc_time_to_tm(ktv.tv_sec, &rtc_tm);

    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        RAWDATA_TEST_DATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    rawdata_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        NOISE_TEST_DATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    noise_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        OPEN_TEST_DATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    open_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        SHORT_TEST_DATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    short_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        COMP_CAP_TEST_DATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    comp_cap_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);

    TPD_INFO("<I> Factory test: touch data dir: "
                TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/\n",
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);

    start_time = ktime_get();

    if (vdata->test_reset_pin) {
        vdata->reset_pin_test_result =
            cts_test_reset_pin(cts_dev, &reset_pin_test_param);
    }

    if (vdata->test_int_pin) {
        vdata->int_pin_test_result =
            cts_test_int_pin(cts_dev, &int_pin_test_param);
    } else {
        vdata->int_pin_test_result = 0;
    }

    if (vdata->test_rawdata) {
        vdata->rawdata_test_result =
            cts_test_rawdata(cts_dev, &rawdata_test_param);
    }
    if (vdata->test_noise) {
        vdata->noise_test_result =
            cts_test_noise(cts_dev, &noise_test_param);
    }
    if (vdata->test_open) {
        vdata->open_test_result =
            cts_test_open(cts_dev, &open_test_param);
    }
    if (vdata->test_short) {
        vdata->short_test_result =
            cts_test_short(cts_dev, &short_test_param);
    }
    if (vdata->test_comp_cap) {
        vdata->comp_cap_test_result =
            cts_test_compensate_cap(cts_dev, &comp_cap_test_param);
    }

    if (rawdata_test_param.test_data_filepath)
        kfree(rawdata_test_param.test_data_filepath);
    if (noise_test_param.test_data_filepath)
        kfree(noise_test_param.test_data_filepath);
    if (open_test_param.test_data_filepath)
        kfree(open_test_param.test_data_filepath);
    if (short_test_param.test_data_filepath)
        kfree(short_test_param.test_data_filepath);
    if (comp_cap_test_param.test_data_filepath)
        kfree(comp_cap_test_param.test_data_filepath);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    TPD_INFO("Baseline test, ELAPSED TIME: %lldms\n", ktime_to_ms(delta_time));
#undef TOUCH_DATA_DIRECTORY_PREFIX
#undef RAWDATA_TEST_DATA_FILENAME
#undef NOISE_TEST_DATA_FILENAME
#undef OPEN_TEST_DATA_FILENAME
#undef SHORT_TEST_DATA_FILENAME
#undef COMP_CAP_TEST_DATA_FILENAME
}

static int cts_do_baseline_test(struct seq_file *s, void *v)
{
    struct touchpanel_data *ts = s->private;
    struct chipone_ts_data *cts_data = NULL;
    struct cts_vendor_data *vdata = NULL;
    int errors = 0;
    int ret;

    cts_data = ts->chip_data;
    if (!ts) {
        TPD_INFO("<E> ts null!\n");
        return 0;
    }

    vdata = cts_data->vendor_data;

    ret = init_test_param(vdata);
    if (ret) {
        TPD_INFO("<E> Init baseline test param failed %d\n", ret);
        return ret;
    }

    ret = alloc_baseline_test_data_mem(vdata,
        cts_data->cts_dev.fwdata.rows * cts_data->cts_dev.fwdata.cols);
    if (ret) {
        TPD_INFO("<E> Alloc baseline test data mem failed\n");
        return 0;
    }

    do_baseline_test(vdata);

    errors = vdata->reset_pin_test_result
            + vdata->int_pin_test_result
            + vdata->rawdata_test_result
            + vdata->noise_test_result
            + vdata->open_test_result
            + vdata->short_test_result
            + vdata->comp_cap_test_result;
    seq_printf(s, "%d errors. %s\n", errors, errors ? "" : "All test passed.");
    TPD_INFO("<I> Baseline Test: %d errors. %s\n", errors,
            errors ? "" : "All test passed.");
    return 0;
}

static int proc_baseline_test_open(struct inode *i, struct file *f)
{
    return single_open(f, cts_do_baseline_test, PDE_DATA(i));
}

static const struct file_operations proc_baseline_test_fops = {
    .owner   = THIS_MODULE,
    .open    = proc_baseline_test_open,
    .read    = seq_read,
    .release = single_release,
};

static int cts_create_proc(struct touchpanel_data *ts)
{
    int ret = 0;
    struct proc_dir_entry *entry = NULL;
    entry = proc_create_data("baseline_test", 0666, ts->prEntry_tp,
            &proc_baseline_test_fops, ts);
    if (entry == NULL) {
        TPD_INFO("<E> Create baseline proc failed\n");
        ret = -ENOMEM;
    }
    return ret;
}

static void cts_black_screen_test(void *chip_data, char *msg)
{
#define TOUCH_DATA_DIRECTORY_PREFIX     "/sdcard/chipone-tddi/test/gesture_data_"
#define GESTURE_RAWDATA_FILENAME        "gesture-rawdata.txt"
#define GESTURE_LP_RAWDATA_FILENAME     "gesture-lp-rawdata.txt"
#define GESTURE_NOISE_FILENAME          "gesture-noise.txt"
#define GESTURE_LP_NOISE_FILENAME       "gesture-lp-noise.txt"
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
	struct cts_vendor_data *vdata;
    struct cts_rawdata_test_priv_param gesture_rawdata_test_priv_param = {
        .frames = 3,
        .work_mode = 1,
    };
    struct cts_test_param gesture_rawdata_test_param = {
        .test_item = CTS_TEST_GESTURE_RAWDATA,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &gesture_rawdata_test_priv_param,
        .priv_param_size = sizeof(gesture_rawdata_test_priv_param),
    };
    struct cts_rawdata_test_priv_param gesture_lp_rawdata_test_priv_param = {
        .frames = 3,
        .work_mode = 0,
    };
    struct cts_test_param gesture_lp_rawdata_test_param = {
        .test_item = CTS_TEST_GESTURE_LP_RAWDATA,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &gesture_lp_rawdata_test_priv_param,
        .priv_param_size = sizeof(gesture_lp_rawdata_test_priv_param),
    };
    struct cts_noise_test_priv_param gesture_noise_test_priv_param = {
        .frames = 3,
        .work_mode = 1,
    };
    struct cts_test_param gesture_noise_test_param = {
        .test_item = CTS_TEST_GESTURE_NOISE,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &gesture_noise_test_priv_param,
        .priv_param_size = sizeof(gesture_noise_test_priv_param),
    };
    struct cts_noise_test_priv_param gesture_lp_noise_test_priv_param = {
        .frames = 3,
        .work_mode = 0,
    };
    struct cts_test_param gesture_lp_noise_test_param = {
        .test_item = CTS_TEST_GESTURE_LP_NOISE,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .priv_param = &gesture_lp_noise_test_priv_param,
        .priv_param_size = sizeof(gesture_lp_noise_test_priv_param),
    };
    struct timeval  ktv;
    struct rtc_time rtc_tm;
    char touch_data_filepath[256];
    ktime_t start_time, end_time, delta_time;

    int gesture_rawdata_test_result = 0;
    int gesture_lp_rawdata_test_result = 0;
    int gesture_noise_test_result = 0;
    int gesture_lp_noise_test_result = 0;
    int errors = 0;

    vdata = cts_data->vendor_data;

    gesture_rawdata_test_priv_param.frames = vdata->black_raw_frames;
    gesture_lp_rawdata_test_priv_param.frames = vdata->black_lp_raw_frames;
    gesture_noise_test_priv_param.frames = vdata->black_noise_frames;
    gesture_lp_noise_test_priv_param.frames = vdata->black_lp_noise_frames;

    gesture_rawdata_test_param.min = &vdata->black_raw_min;
    gesture_rawdata_test_param.max = &vdata->black_raw_max;
    gesture_lp_rawdata_test_param.min = &vdata->black_lp_raw_min;
    gesture_lp_rawdata_test_param.max = &vdata->black_lp_raw_max;
    gesture_noise_test_param.max = &vdata->black_noise_max;
    gesture_lp_noise_test_param.max = &vdata->black_lp_noise_max;

    do_gettimeofday(&ktv);
    ktv.tv_sec -= sys_tz.tz_minuteswest * 60;
    rtc_time_to_tm(ktv.tv_sec, &rtc_tm);

    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        GESTURE_RAWDATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    gesture_rawdata_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        GESTURE_LP_RAWDATA_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    gesture_lp_rawdata_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        GESTURE_NOISE_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    gesture_noise_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/"
        GESTURE_LP_NOISE_FILENAME,
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);
    gesture_lp_noise_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);

    TPD_INFO("<I> Factory test: touch data dir: "
                TOUCH_DATA_DIRECTORY_PREFIX"%04d%02d%02d_%02d%02d%02d/\n",
        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
        rtc_tm.tm_hour, rtc_tm.tm_min,rtc_tm.tm_sec);

    start_time = ktime_get();

    prepare_black_test(cts_dev);

    if (!!vdata->black_rawdata)
        gesture_rawdata_test_result =
            cts_test_gesture_rawdata(cts_dev, &gesture_rawdata_test_param);
    if (!!vdata->black_noise)
        gesture_noise_test_result =
            cts_test_gesture_noise(cts_dev, &gesture_noise_test_param);
    if (!!vdata->black_lp_rawdata)
        gesture_lp_rawdata_test_result =
            cts_test_gesture_rawdata(cts_dev, &gesture_lp_rawdata_test_param);
    if (!!vdata->black_lp_noise)
        gesture_lp_noise_test_result =
            cts_test_gesture_noise(cts_dev, &gesture_lp_noise_test_param);

    if (gesture_rawdata_test_param.test_data_filepath)
        kfree(gesture_rawdata_test_param.test_data_filepath);
    if (gesture_lp_rawdata_test_param.test_data_filepath)
        kfree(gesture_lp_rawdata_test_param.test_data_filepath);
    if (gesture_noise_test_param.test_data_filepath)
        kfree(gesture_noise_test_param.test_data_filepath);
    if (gesture_lp_noise_test_param.test_data_filepath)
        kfree(gesture_lp_noise_test_param.test_data_filepath);

    cts_plat_reset_device(cts_dev->pdata);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    TPD_INFO("Black gesture test, ELAPSED TIME: %lldms\n", ktime_to_ms(delta_time));

    errors = gesture_rawdata_test_result
            + gesture_lp_rawdata_test_result
            + gesture_noise_test_result
            + gesture_lp_noise_test_result;
    sprintf(msg, "%d errors. %s\n", errors, errors ? "" : "All test passed.");
    TPD_INFO("<I> Black Test: %s\n", msg);

#undef TOUCH_DATA_DIRECTORY_PREFIX
#undef GESTURE_RAWDATA_FILENAME
#undef GESTURE_LP_RAWDATA_FILENAME
#undef GESTURE_NOISE_FILENAME
#undef GESTURE_LP_NOISE_FILENAME
}

static uint8_t cts_get_touch_direction(void *chip_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    return cts_data->touch_direction;
}
static void cts_set_touch_direction(void *chip_data, uint8_t dir)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    cts_data->touch_direction = dir;
    TPD_INFO("<I> Set touch_direction:%d\n", dir);
}

static struct oplus_touchpanel_operations cts_tp_ops = {
    .get_chip_info                        = cts_get_chip_info,
    .mode_switch                          = cts_mode_switch,
    .get_touch_points                     = cts_get_touch_points,
    .get_gesture_info                     = cts_handle_gesture_info,
    .ftm_process                          = cts_ftm_process,
    .get_vendor                           = cts_get_vendor,
    .reset                                = cts_reset,
    .reinit_device                        = NULL,
    .fw_check                             = cts_fw_check,
    .fw_update                            = cts_fw_update,
    .power_control                        = cts_power_control,
    .reset_gpio_control                   = NULL,
    .trigger_reason                       = cts_trigger_reason,
    .get_keycode                          = NULL,
    .esd_handle                           = cts_esd_handle,
    .fw_handle                            = NULL,
    .resume_prepare                       = NULL,
    .spurious_fp_check                    = NULL,
    .finger_proctect_data_get             = NULL,
    .exit_esd_mode                        = NULL,
    .register_info_read                   = NULL,
    .write_ps_status                      = NULL,
    .specific_resume_operate              = NULL,
    .get_usb_state                        = NULL,
    .black_screen_test                    = cts_black_screen_test,
    .irq_handle_unlock                    = NULL,
    .async_work                           = NULL,
    .get_face_state                       = NULL,
    .health_report                        = NULL,
    .bootup_test                          = NULL,
    .get_gesture_coord                    = NULL,
    .set_touch_direction                  = cts_set_touch_direction,
    .get_touch_direction                  = cts_get_touch_direction,
};


/* @type true:baseline; false:diffdata */
static void cts_get_basedata(struct seq_file *s, void *chip_data, bool type)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    s16 *rawdata = NULL;
    int ret, r, c;
    char type_str[10];
    bool data_valid = true;

    memcpy(type_str, type ? "baseline" : "diffdata", 8);

    TPD_INFO("<I> Show %s\n", type_str);

    r = cts_dev->fwdata.rows;
    c = cts_dev->fwdata.cols;
    rawdata = kzalloc(r * c * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        TPD_INFO("<E> Allocate memory for %s failed\n", type_str);
        return;
    }

    cts_lock_device(cts_dev);
    ret = cts_enable_get_rawdata(cts_dev);
    if (ret) {
        TPD_INFO("<E> Enable read %s failed\n", type_str);
        goto err_free_rawdata;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
    if (ret) {
        TPD_INFO("<E> Send cmd QUIT_GESTURE_MONITOR failed\n");
        goto err_free_rawdata;
    }
    msleep(50);

    if (type)
        ret = cts_get_rawdata(cts_dev, rawdata);
    else
        ret = cts_get_diffdata(cts_dev, rawdata);
    if(ret) {
        TPD_INFO("<E> Get %s failed\n", type_str);
        data_valid = false;
    }
    ret = cts_disable_get_rawdata(cts_dev);
    if (ret) {
        TPD_INFO("<E> Disable read %s failed\n", type_str);
    }

    if (data_valid) {
        for (r = 0; r < cts_dev->fwdata.rows; r++) {
            for (c = 0; c < cts_dev->fwdata.cols; c++) {
                seq_printf(s, "%5d", rawdata[r * cts_dev->fwdata.cols + c]);
           }
           seq_printf(s, "\n");
        }
    }

err_free_rawdata:
    cts_unlock_device(cts_dev);
    kfree(rawdata);
}

static void cts_baseline_read(struct seq_file *s, void *chip_data)
{
    cts_get_basedata(s, chip_data, true);
}

static void cts_delta_read(struct seq_file *s, void *chip_data)
{
    cts_get_basedata(s, chip_data, false);
}

static void cts_main_register_read(struct seq_file *s, void *chip_data)
{
    return;
}

static void cts_limit_read(struct seq_file *s, struct touchpanel_data *ts)
{
    const struct firmware *fw = NULL;
    struct cts_range_threshold threshold;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int ret = 0;

    ret = request_firmware(&fw, ts->panel_data.test_limit_name, ts->dev);
    if (ret < 0) {
        TPD_INFO("<E> Request %s failed", ts->panel_data.test_limit_name);
        return;
    }
    ret = memcmp(header, fw->data, sizeof(header));
    if (ret) {
        TPD_INFO("<E> LIMIT file was not matched");
        release_firmware(fw);
        return;
    } else {
        TPD_INFO("Matched limit file!");
    }
    memcpy(&threshold, fw->data, fw->size);

    seq_printf(s, "%s %s: %d\n", header, "Limit Bin version", threshold.version);
    seq_printf(s, "%-30s\n", "--------------------------------");
    seq_printf(s, "%-24s %d\n", "test_int_pin flag:", threshold.baseline_int_pin);
    seq_printf(s, "%-24s %d\n", "test_reset_pin flag:", threshold.baseline_reset_pin);
    seq_printf(s, "%-24s %d\n", "test_rawdata flag:", threshold.baseline_rawdata);
    seq_printf(s, "%-24s %d\n", "test_noise flag:", threshold.baseline_noise);
    seq_printf(s, "%-24s %d\n", "test_open flag:", threshold.baseline_open);
    seq_printf(s, "%-24s %d\n", "test_short flag:", threshold.baseline_short);
    seq_printf(s, "%-24s %d\n", "test_comp_cap flag:", threshold.baseline_comp_cap);
    seq_printf(s, "%-24s %d\n", "black_rawdata flag:", threshold.black_rawdata);
    seq_printf(s, "%-24s %d\n", "black_lp_rawdata flag:", threshold.black_lp_rawdata);
    seq_printf(s, "%-24s %d\n", "black_noise flag:", threshold.black_noise);
    seq_printf(s, "%-24s %d\n", "black_lp_noise flag:", threshold.black_lp_noise);
    seq_printf(s, "%-30s\n", "--------------------------------");
    seq_printf(s, "%-24s %d\n", "Baseline raw frames:", threshold.baseline_raw_frames);
    seq_printf(s, "%-24s %d\n", "Baseline raw min:", threshold.baseline_raw_min);
    seq_printf(s, "%-24s %d\n", "Baseline raw max:", threshold.baseline_raw_max);
    seq_printf(s, "%-24s %d\n", "Baseline noise frames:", threshold.baseline_noise_frames);
    seq_printf(s, "%-24s %d\n", "Baseline noise max:", threshold.baseline_noise_max);
    seq_printf(s, "%-24s %d\n", "Baseline open min:", threshold.baseline_open_min);
    seq_printf(s, "%-24s %d\n", "Baseline short min:", threshold.baseline_short_min);
    seq_printf(s, "%-24s %d\n", "Baseline compcap min:", threshold.baseline_compcap_min);
    seq_printf(s, "%-24s %d\n", "Baseline compcap max:", threshold.baseline_compcap_max);
    seq_printf(s, "%-24s %d\n", "Black raw frames:", threshold.black_raw_frames);
    seq_printf(s, "%-24s %d\n", "Black raw min:", threshold.black_raw_min);
    seq_printf(s, "%-24s %d\n", "Black raw max:", threshold.black_raw_max);
    seq_printf(s, "%-24s %d\n", "Black lp raw frames:", threshold.black_lp_raw_frames);
    seq_printf(s, "%-24s %d\n", "Black lp raw min:", threshold.black_lp_raw_min);
    seq_printf(s, "%-24s %d\n", "Black lp raw max:", threshold.black_lp_raw_max);
    seq_printf(s, "%-24s %d\n", "Black noise frames:", threshold.black_noise_frames);
    seq_printf(s, "%-24s %d\n", "Black noise max:", threshold.black_noise_max);
    seq_printf(s, "%-24s %d\n", "Black lp noise frames:", threshold.black_lp_noise_frames);
    seq_printf(s, "%-24s %d\n", "Black lp noise max:", threshold.black_lp_noise_max);

    release_firmware(fw);
    return;
}

static struct debug_info_proc_operations debug_info_proc_ops = {
    .limit_read         = cts_limit_read,
    .baseline_read      = cts_baseline_read,
    .delta_read         = cts_delta_read,
    .main_register_read = cts_main_register_read,
};


static int cts_update_headfile_fw(void *chip_data, struct panel_info *panel_data)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)chip_data;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_firmware cts_firmware;
    int ret;

    TPD_INFO("<I> Enter %s!\n", __func__);

    cts_firmware.name = panel_data->fw_name;
    cts_firmware.hwid = CTS_DEV_HWID_ICNL9911C;
    cts_firmware.fwid = CTS_DEV_FWID_ICNL9911C;
    cts_firmware.data = (u8 *)panel_data->firmware_headfile.firmware_data;
    cts_firmware.size = panel_data->firmware_headfile.firmware_size;

    cts_lock_device(cts_dev);
    ret = cts_update_firmware(cts_dev, &cts_firmware, false);
    cts_unlock_device(cts_dev);
    if (ret < 0) {
        return -1;
    }

    cts_read_fw_ddi_version(chip_data, tsdata);

    if (cts_enter_normal_mode(cts_dev) < 0) {
        return -1;
    }
    return 0;
}

static int cts_vendor_init(struct chipone_ts_data *cts_data)
{
    struct cts_vendor_data *vdata = NULL;

    if (cts_data == NULL) {
        TPD_INFO("<E> Init with cts_data = NULL\n");
        return -EINVAL;
    }

    TPD_INFO("<I> Init\n");

    cts_data->vendor_data = NULL;

    vdata = kzalloc(sizeof(*vdata), GFP_KERNEL);
    if (vdata == NULL) {
        TPD_INFO("<E> Alloc vendor data failed\n");
        return -ENOMEM;
    }

    cts_create_proc(tsdata);

    cts_data->vendor_data = vdata;
    vdata->cts_data = cts_data;

    return 0;
}

static int cts_vendor_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_vendor_data *vdata = NULL;

    if (cts_data == NULL) {
        TPD_INFO("<E> Deinit with cts_data = NULL\n");
        return -EINVAL;
    }

    if (cts_data->vendor_data == NULL) {
        TPD_INFO("<W> Deinit with vendor_data = NULL\n");
        return -EINVAL;
    }

    TPD_INFO("<I> Deinit\n");

    vdata = cts_data->vendor_data;

    free_baseline_test_data_mem(vdata);

    kfree(cts_data->vendor_data);
    cts_data->vendor_data = NULL;

    return 0;
}

#ifdef CONFIG_CTS_SYSFS
static ssize_t reset_pin_show(struct device_driver *driver, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_HAS_RESET_PIN
        'Y'
#else
        'N'
#endif
        );
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static DRIVER_ATTR(reset_pin, S_IRUGO, reset_pin_show, NULL);
#else
static DRIVER_ATTR_RO(reset_pin);
#endif

static ssize_t max_touch_num_show(struct device_driver *dev, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_TOUCH_NUM: %d\n",
        CFG_CTS_MAX_TOUCH_NUM);
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static DRIVER_ATTR(max_touch_num, S_IRUGO, max_touch_num_show, NULL);
#else
static DRIVER_ATTR_RO(max_touch_num);
#endif

static ssize_t max_xfer_size_show(struct device_driver *dev, char *buf)
{
#ifdef CONFIG_CTS_I2C_HOST
    return scnprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_I2C_XFER_SIZE: %d\n",
        CFG_CTS_MAX_I2C_XFER_SIZE);
#else
    return scnprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_SPI_XFER_SIZE: %d\n",
        CFG_CTS_MAX_SPI_XFER_SIZE);
#endif
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static DRIVER_ATTR(max_xfer_size, S_IRUGO, max_xfer_size_show, NULL);
#else
static DRIVER_ATTR_RO(max_xfer_size);
#endif

static ssize_t driver_info_show(struct device_driver *dev, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "Driver version: %s\n", CFG_CTS_DRIVER_VERSION);
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static DRIVER_ATTR(driver_info, S_IRUGO, driver_info_show, NULL);
#else
static DRIVER_ATTR_RO(driver_info);
#endif

static struct attribute *cts_i2c_driver_config_attrs[] = {
    &driver_attr_reset_pin.attr,
    &driver_attr_max_touch_num.attr,
    &driver_attr_max_xfer_size.attr,
    &driver_attr_driver_info.attr,
    NULL
};

static const struct attribute_group cts_i2c_driver_config_group = {
    .name = "config",
    .attrs = cts_i2c_driver_config_attrs,
};

static const struct attribute_group *cts_i2c_driver_config_groups[] = {
    &cts_i2c_driver_config_group,
    NULL,
};
#endif /* CONFIG_CTS_SYSFS */


#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#else
static int cts_driver_probe(struct spi_device *client)
#endif
{
    struct chipone_ts_data *cts_data = NULL;
    int ret = 0;

#ifdef CONFIG_CTS_I2C_HOST
    TPD_INFO("<I> Probe i2c client: name='%s' addr=0x%02x flags=0x%02x irq=%d\n",
        client->name, client->addr, client->flags, client->irq);

#if !defined(CONFIG_MTK_PLATFORM)
    if (client->addr != CTS_DEV_NORMAL_MODE_I2CADDR) {
        TPD_INFO("<E> Probe i2c addr 0x%02x != driver config addr 0x%02x\n",
            client->addr, CTS_DEV_NORMAL_MODE_I2CADDR);
        return -ENODEV;
    };
#endif

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        TPD_INFO("<E> Check functionality failed\n");
        return -ENODEV;
    }
#endif

    cts_data = (struct chipone_ts_data *)kzalloc(sizeof(*cts_data), GFP_KERNEL);
    if (cts_data == NULL) {
        TPD_INFO("<E> Allocate chipone_ts_data failed\n");
        return -ENOMEM;
    }

    cts_data->pdata = (struct cts_platform_data *)kzalloc(
            sizeof(struct cts_platform_data), GFP_KERNEL);
    if (cts_data->pdata == NULL) {
        TPD_INFO("<E> Allocate cts_platform_data failed\n");
        ret = -ENOMEM;
        goto err_free_cts_data;
    }

    chipone_ts_data = cts_data;

#ifdef CONFIG_CTS_I2C_HOST
    cts_data->i2c_client = client;
    cts_data->device = &client->dev;
#else
    cts_data->spi_client = client;
    cts_data->device = &client->dev;
#endif

    ret = cts_init_platform_data(cts_data->pdata, client);
    if (ret) {
        TPD_INFO("<E> cts_init_platform_data err\n");
        goto err_free_pdata;
    }

    cts_data->cts_dev.pdata = cts_data->pdata;
    cts_data->pdata->cts_dev = &cts_data->cts_dev;

    tsdata = common_touch_data_alloc();
    if (tsdata == NULL) {
        TPD_INFO("<E> ts kzalloc error\n");
        goto err_free_pdata;
    }

    cts_data->vfw.data = vmalloc(120 * 1024);
    if (!cts_data->vfw.data) {
        TPD_INFO("<E> Alloc vfw.data failed\n");
        goto err_free_tsdata;
    }

    tsdata->s_client = cts_data->spi_client;
    tsdata->irq = cts_data->pdata->irq;
    tsdata->dev = &cts_data->spi_client->dev;
    tsdata->chip_data = cts_data;
    tsdata->ts_ops = &cts_tp_ops;
    tsdata->debug_info_ops = &debug_info_proc_ops;
    cts_data->tsdata = tsdata;

#ifdef CONFIG_CTS_I2C_HOST
    i2c_set_clientdata(client, cts_data->tsdata);
#else
    spi_set_drvdata(client, cts_data->tsdata);
#endif

    ret = register_common_touch_device(tsdata);
    if (ret < 0) {
        TPD_INFO("<E> register touch device failed: ret=%d\n", ret);
        goto err_free_vfw_data;
    }

    cts_data->pdata->rst_gpio = tsdata->hw_res.reset_gpio;

    ret = cts_plat_reset_device(cts_data->pdata);
    if (ret < 0) {
        TPD_INFO("<E> Reset device failed %d\n", ret);
    }

    ret = cts_probe_device(&cts_data->cts_dev);
    if (ret) {
        TPD_INFO("<E> Probe device failed %d\n", ret);
        //goto err_free_common_touch;
    }

    ret = cts_update_headfile_fw(tsdata->chip_data, &tsdata->panel_data);
    if (ret) {
        TPD_INFO("<E> update firmware in probe failed\n");
        //goto err_free_common_touch;
    }

    ret = cts_tool_init(cts_data);
    if (ret) {
        TPD_INFO("<W> Init tool node failed %d\n", ret);
        //goto err_free_common_touch;
    }

    ret = cts_sysfs_add_device(&client->dev);
    if (ret) {
        TPD_INFO("<W> Add sysfs entry for device failed %d\n", ret);
        //goto err_tool_init;
    }

    ret = cts_vendor_init(cts_data);
    if (ret) {
        TPD_INFO("<E> Init vendor specific failed %d\n", ret);
        //goto err_sysfs_add_device;
    }

    return 0;
/**
THIS PART WILL NEVER RUN, IC ID MUST BE DETECTED
err_sysfs_add_device:
    cts_sysfs_remove_device(&client->dev);
err_tool_init:
    cts_tool_deinit(cts_data);
err_free_common_touch:
*/
err_free_vfw_data:
    tsdata->s_client = NULL;
    spi_set_drvdata(client, NULL);
    vfree(cts_data->vfw.data);
    cts_data->vfw.data = NULL;
err_free_tsdata:
    common_touch_data_free(tsdata);
    tsdata = NULL;
err_free_pdata:
    kfree(cts_data->pdata);
    cts_data->pdata = NULL;
err_free_cts_data:
    kfree(cts_data);
    cts_data = NULL;

    TPD_INFO("<E> Probe failed %d\n", ret);

    // If return ret, kernel will be crashed.
    //return ret;
    return 0;
}

#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_remove(struct i2c_client *client)
#else
static int cts_driver_remove(struct spi_device *client)
#endif
{
    struct chipone_ts_data *cts_data;
    int ret = 0;

    TPD_INFO("<I> Remove\n");

#ifdef CONFIG_CTS_I2C_HOST
    cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
#else
    cts_data = (struct chipone_ts_data *)spi_get_drvdata(client);
#endif
    if (cts_data) {
        ret = cts_stop_device(&cts_data->cts_dev);
        if (ret) {
            TPD_INFO("<W> Stop device failed %d\n", ret);
        }

        cts_vendor_deinit(cts_data);

        cts_sysfs_remove_device(&client->dev);

        cts_tool_deinit(cts_data);

        tsdata->s_client = NULL;
        spi_set_drvdata(client, NULL);

        if (cts_data->vfw.data) {
            vfree(cts_data->vfw.data);
            cts_data->vfw.data = NULL;
        }

        if (tsdata) {
            common_touch_data_free(tsdata);
            tsdata = NULL;
        }

        if (cts_data->pdata) {
            kfree(cts_data->pdata);
            cts_data->pdata = NULL;
        }

        kfree(cts_data);
        cts_data = NULL;
    } else {
        TPD_INFO("<W> Chipone i2c driver remove while NULL chipone_ts_data\n");
        return -EINVAL;
    }

    return ret;
}

static const struct of_device_id cts_i2c_of_match_table[] = {
    {.compatible = CFG_CTS_OF_DEVICE_ID_NAME,},
    { },
};
MODULE_DEVICE_TABLE(of, cts_i2c_of_match_table);

#ifdef CONFIG_CTS_I2C_HOST
static const struct i2c_device_id cts_device_id_table[] = {
    {CFG_CTS_DEVICE_NAME, 0},
    {}
};
#else
static const struct spi_device_id cts_device_id_table[] = {
    {CFG_CTS_DEVICE_NAME, 0},
    {}
};
#endif

static int chipone_tpd_suspend(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);
    TPD_INFO("<I> tp_i2c_suspend\n");
    tp_i2c_suspend(ts);
    return 0;
}

static int chipone_tpd_resume(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);
    TPD_INFO("<I> tp_i2c_resume\n");
    tp_i2c_resume(ts);
    return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
#ifdef CONFIG_FB
    .suspend = chipone_tpd_suspend,
    .resume = chipone_tpd_resume,
#endif
};

#ifdef CONFIG_CTS_I2C_HOST
static struct i2c_driver cts_i2c_driver = {
#else
static struct spi_driver cts_spi_driver = {
#endif
    .probe = cts_driver_probe,
    .remove = cts_driver_remove,
    .driver = {
        .name = CFG_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(cts_i2c_of_match_table),
        .pm = &tp_pm_ops,
#ifdef CONFIG_CTS_SYSFS
        .groups = cts_i2c_driver_config_groups,
#endif
    },
    .id_table = cts_device_id_table,
};

static int __init cts_driver_init(void)
{
    int ret;

    TPD_INFO("<I> Chipone ICNL9911C driver %s\n", CFG_CTS_DRIVER_VERSION);

    /* If matched, means got ic id */
    if (!tp_judge_ic_match(TPD_DEVICE)) {
        TPD_INFO("<E> %s mismatched\n", TPD_DEVICE);
        return -ENODEV;
    }

    TPD_INFO("<I> %s matched\n", TPD_DEVICE);

    get_oem_verified_boot_state();

#ifdef CONFIG_CTS_I2C_HOST
    ret = i2c_add_driver(&cts_i2c_driver);
#else
    ret = spi_register_driver(&cts_spi_driver);
#endif

    if (ret)
        TPD_INFO("<E> Failed to register SPI driver.\n");

    /** Even probe fail, value of 'ret' was always 0 */
    return ret;
}

static void __exit cts_driver_exit(void)
{
    TPD_INFO("<I> Chipone ICNL9911C driver exit\n");

#ifdef CONFIG_CTS_I2C_HOST
    i2c_del_driver(&cts_i2c_driver);
#else
    spi_unregister_driver(&cts_spi_driver);
#endif
}

module_init(cts_driver_init);
module_exit(cts_driver_exit);

MODULE_DESCRIPTION("Chipone ICNL9911C Driver for MTK platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Cai Yang <ycai@chiponeic.com>");
MODULE_LICENSE("GPL");
