#ifndef CTS_CORE_H
#define CTS_CORE_H

#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/unaligned/access_ok.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/byteorder/generic.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/syscalls.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "../touchpanel_common.h"
#include "../tp_devices.h"
#include "../util_interface/touch_interfaces.h"


#define TPD_DEVICE "chipone,icnl9911c"
#define TPD_INFO(a, arg...)  pr_err("[TP]"TPD_DEVICE ": " a, ##arg)
#define TPD_DEBUG(a, arg...)\
    do{\
        if (LEVEL_DEBUG == tp_debug)\
            pr_err("[TP]"TPD_DEVICE ": " a, ##arg);\
    }while(0)

#define TPD_DETAIL(a, arg...)\
    do{\
        if (LEVEL_BASIC != tp_debug)\
            pr_err("[TP]"TPD_DEVICE ": " a, ##arg);\
    }while(0)


enum cts_dev_hw_reg {
    CTS_DEV_HW_REG_HARDWARE_ID    = 0x30000u,
    CTS_DEV_HW_REG_CLOCK_GATING   = 0x30004u,
    CTS_DEV_HW_REG_RESET_CONFIG   = 0x30008u,
    CTS_DEV_HW_REG_BOOT_MODE      = 0x30010u,
    CTS_DEV_HW_REG_CURRENT_MODE   = 0x30011u,
};

enum cts_dev_boot_mode {
    CTS_DEV_BOOT_MODE_FLASH       = 1,
    CTS_DEV_BOOT_MODE_I2C_PROGRAM = 2,
    CTS_DEV_BOOT_MODE_SRAM        = 3,
    CTS_DEV_BOOT_MODE_SPI_PROGRAM = 5,
    CTS_DEV_BOOT_MODE_MASK        = 7,
};

/** I2C addresses(7bits), transfer size and bitrate */
#define CTS_DEV_NORMAL_MODE_I2CADDR         (0x48)
#define CTS_DEV_PROGRAM_MODE_I2CADDR        (0x30)
#define CTS_DEV_NORMAL_MODE_ADDR_WIDTH      (2)
#define CTS_DEV_PROGRAM_MODE_ADDR_WIDTH     (3)
#define CTS_DEV_NORMAL_MODE_SPIADDR         (0xF0)
#define CTS_DEV_PROGRAM_MODE_SPIADDR        (0x60)

/** Chipone firmware register addresses under normal mode */
enum cts_device_fw_reg {
    CTS_DEVICE_FW_REG_WORK_MODE = 0x0000,
    CTS_DEVICE_FW_REG_SYS_BUSY = 0x0001,
    CTS_DEVICE_FW_REG_DATA_READY = 0x0002,
    CTS_DEVICE_FW_REG_CMD = 0x0004,
    CTS_DEVICE_FW_REG_POWER_MODE = 0x0005,
    CTS_DEVICE_FW_REG_FW_LIB_MAIN_VERSION = 0x0009,
    CTS_DEVICE_FW_REG_CHIP_TYPE = 0x000A,
    CTS_DEVICE_FW_REG_VERSION = 0x000C,
    CTS_DEVICE_FW_REG_DDI_VERSION = 0x0010,
    CTS_DEVICE_FW_REG_GET_WORK_MODE = 0x003F,
    CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_DONE = 0x0046, /* RO */
    CTS_DEVICE_FW_REG_FW_LIB_SUB_VERSION =  0x0047,
    CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY =  0x004E,

    CTS_DEVICE_FW_REG_TOUCH_INFO = 0x1000,
    CTS_DEVICE_FW_REG_RAW_DATA = 0x2000,
    CTS_DEVICE_FW_REG_DIFF_DATA = 0x3000,
    CTS_DEVICE_FW_REG_BASELINE = 0x6000,
    CTS_DEVICE_FW_REG_GESTURE_INFO = 0x7000,

    CTS_DEVICE_FW_REG_PANEL_PARAM = 0x8000,
    CTS_DEVICE_FW_REG_NUM_TX = 0x8007,
    CTS_DEVICE_FW_REG_NUM_RX = 0x8008,
    CTS_DEVICE_FW_REG_INT_KEEP_TIME = 0x8047,   /* Unit us */
    CTS_DEVICE_FW_REG_RAWDATA_TARGET = 0x8049,
    CTS_DEVICE_FW_REG_X_RESOLUTION = 0x8090,
    CTS_DEVICE_FW_REG_Y_RESOLUTION = 0x8092,
    CTS_DEVICE_FW_REG_SWAP_AXES = 0x8094,
    CTS_DEVICE_FW_REG_GLOVE_MODE = 0x8095,
    CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON = 0x80A3,
    CTS_DEVICE_FW_REG_INT_MODE = 0x80D8,
    CTS_DEVICE_FW_REG_EARJACK_DETECT_SUPP = 0x8113,
    CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_ENABLE = 0x8114,
    CTS_DEVICE_FW_REG_ESD_PROTECTION = 0x8156, /* RW */
    CTS_DEVICE_FW_REG_FLAG_BITS = 0x8158,

    CTS_DEVICE_FW_REG_COMPENSATE_CAP = 0xA000,
    CTS_DEVICE_FW_REG_DEBUG_INTF = 0xF000,

    CTS_DEVICE_FW_REG_GSTR_ONLY_FS_EN = 0x80E1,
    CTS_DEVICE_FW_REG_GSTR_DATA_DBG_EN = 0x80B4,
    CTS_FIRMWARE_WORK_MODE_GSTR_DBG = 0x04,

    CTS_DEVICE_FW_REG_LANDSCAPE_MODE = 0x0801,
};

/** Hardware IDs, read from hardware id register */
enum cts_dev_hwid {
    CTS_DEV_HWID_ICNL9911  = 0x990100u,
    CTS_DEV_HWID_ICNL9911S = 0x990110u,
    CTS_DEV_HWID_ICNL9911C = 0x991110u,

    CTS_DEV_HWID_ANY       = 0,
    CTS_DEV_HWID_INVALID   = 0xFFFFFFFFu,
};

/* Firmware IDs, read from firmware register @ref CTS_DEV_FW_REG_CHIP_TYPE
   under normal mode */
enum cts_dev_fwid {
    CTS_DEV_FWID_ICNL9911  = 0x9911u,
    CTS_DEV_FWID_ICNL9911S = 0x9964u,
    CTS_DEV_FWID_ICNL9911C = 0x9954u,

    CTS_DEV_FWID_ANY       = 0u,
    CTS_DEV_FWID_INVALID   = 0xFFFFu
};

/** Commands written to firmware register @ref CTS_DEVICE_FW_REG_CMD under normal mode */
enum cts_firmware_cmd {
    CTS_CMD_RESET = 1,
    CTS_CMD_SUSPEND = 2,
    CTS_CMD_ENTER_WRITE_PARA_TO_FLASH_MODE = 3,
    CTS_CMD_WRITE_PARA_TO_FLASH = 4,
    CTS_CMD_WRTITE_INT_HIGH = 5,
    CTS_CMD_WRTITE_INT_LOW = 6,
    CTS_CMD_RELASE_INT_TEST = 7,
    CTS_CMD_RECOVERY_TX_VOL = 0x10,
    CTS_CMD_DEC_TX_VOL_1 = 0x11,
    CTS_CMD_DEC_TX_VOL_2 = 0x12,
    CTS_CMD_DEC_TX_VOL_3 = 0x13,
    CTS_CMD_DEC_TX_VOL_4 = 0x14,
    CTS_CMD_DEC_TX_VOL_5 = 0x15,
    CTS_CMD_DEC_TX_VOL_6 = 0x16,
    CTS_CMD_ENABLE_READ_RAWDATA = 0x20,
    CTS_CMD_DISABLE_READ_RAWDATA = 0x21,
    CTS_CMD_SUSPEND_WITH_GESTURE = 0x40,
    CTS_CMD_QUIT_GESTURE_MONITOR = 0x41,
    CTS_CMD_CHARGER_ATTACHED = 0x55,
    CTS_CMD_EARJACK_ATTACHED = 0x57,
    CTS_CMD_EARJACK_DETACHED = 0x58,
    CTS_CMD_CHARGER_DETACHED = 0x66,
    CTS_CMD_ENABLE_FW_LOG_REDIRECT = 0x86,
    CTS_CMD_DISABLE_FW_LOG_REDIRECT = 0x87,
    CTS_CMD_ENABLE_READ_CNEG   = 0x88,
    CTS_CMD_DISABLE_READ_CNEG  = 0x89,
    CTS_CMD_FW_LOG_SHOW_FINISH = 0xE0,

};

#pragma pack(1)
/** Touch message read back from chip */
struct cts_device_touch_msg {
    u8      id;
    __le16  x;
    __le16  y;
    u8      pressure;
    u8      event;

#define CTS_DEVICE_TOUCH_EVENT_NONE         (0)
#define CTS_DEVICE_TOUCH_EVENT_DOWN         (1)
#define CTS_DEVICE_TOUCH_EVENT_MOVE         (2)
#define CTS_DEVICE_TOUCH_EVENT_STAY         (3)
#define CTS_DEVICE_TOUCH_EVENT_UP           (4)

};

/** Touch information read back from chip */
struct cts_device_touch_info {
    u8  vkey_state;
    u8  num_msg;

    struct cts_device_touch_msg msgs[CFG_CTS_MAX_TOUCH_NUM];
};

/** Gesture trace point read back from chip */
struct cts_device_gesture_point {
    __le16  x;
    __le16  y;
    u8      pressure;
    u8      event;
};

/** Gesture information read back from chip */
struct cts_device_gesture_info {
    u8    gesture_id;
#define CTS_GESTURE_UP                  (0x11)
#define CTS_GESTURE_C                   (0x12)
#define CTS_GESTURE_O                   (0x13)
#define CTS_GESTURE_M                   (0x14)
#define CTS_GESTURE_W                   (0x15)
#define CTS_GESTURE_E                   (0x16)
#define CTS_GESTURE_S                   (0x17)
#define CTS_GESTURE_B                   (0x18)
#define CTS_GESTURE_T                   (0x19)
#define CTS_GESTURE_H                   (0x1A)
#define CTS_GESTURE_F                   (0x1B)
#define CTS_GESTURE_X                   (0x1C)
#define CTS_GESTURE_Z                   (0x1D)
#define CTS_GESTURE_V                   (0x1E)
#define CTS_GESTURE_RV                  (0x1F)
#define CTS_GESTURE_LR                  (0x20)
#define CTS_GESTURE_RR                  (0x21)
#define CTS_GESTURE_DOWN                (0x22)
#define CTS_GESTURE_LEFT                (0x23)
#define CTS_GESTURE_RIGHT               (0x24)
#define CTS_GESTURE_DDOWN               (0x25)
#define CTS_GESTURE_D_TAP               (0x50)

    u8  num_points;

#define CTS_CHIP_MAX_GESTURE_TRACE_POINT    (64u)
    struct cts_device_gesture_point points[CTS_CHIP_MAX_GESTURE_TRACE_POINT];

};
#pragma pack()



enum cts_crc_type {
    CTS_CRC16 = 1,
    CTS_CRC32 = 2,
};

enum cts_work_mode {
    CTS_WORK_MODE_UNKNOWN = 0,
    CTS_WORK_MODE_SUSPEND,
    CTS_WORK_MODE_NORMAL_ACTIVE,
    CTS_WORK_MODE_NORMAL_IDLE,
    CTS_WORK_MODE_GESTURE_ACTIVE,
    CTS_WORK_MODE_GESTURE_IDLE,
};


struct cts_device;

/** Chip hardware data, will never change */
struct cts_device_hwdata {
    const char *name;
    u32 hwid;
    u16 fwid;
    u8  num_row;
    u8  num_col;
    u32 sram_size;

    /* Address width under program mode */
    u8  program_addr_width;

    const struct cts_sfctrl *sfctrl;

    int (*enable_access_ddi_reg)(struct cts_device *cts_dev, bool enable);
};

/** Chip firmware data */
struct cts_device_fwdata {
    u16 version;
    u16 res_x;
    u16 res_y;
    u8  rows;
    u8  cols;
    bool flip_x;
    bool flip_y;
    bool swap_axes;
    u8  ddi_version;
    u8  int_mode;
    u8  esd_method;
    u16 lib_version;
    u16 int_keep_time;
    u16 rawdata_target;
    u16 gstr_rawdata_target;
};

/** Chip runtime data */
struct cts_device_rtdata {
    u8   slave_addr;
    int  addr_width;
    bool program_mode;
    bool has_flash;

    bool updating;
    bool testing;

    bool fw_log_redirect_enabled;
    bool glove_mode_enabled;
};

struct cts_firmware {
    const char *name;
    u32 hwid;
    u16 fwid;

    u8 *data;
    size_t size;
};

struct cts_platform_data {
    int irq;
    int int_gpio;
    int rst_gpio;

    struct cts_device *cts_dev;

    struct input_dev *ts_input_dev;

    struct mutex dev_lock;
    struct spinlock irq_lock;
    bool            irq_is_disable;

#ifdef CFG_CTS_FW_LOG_REDIRECT
    u8 fw_log_buf[CTS_FW_LOG_BUF_LEN];
#endif

#ifdef CONFIG_CTS_I2C_HOST
    struct i2c_client *i2c_client;
    u8 i2c_fifo_buf[CFG_CTS_MAX_I2C_XFER_SIZE];
#else
    struct spi_device *spi_client;
    u8 spi_cache_buf[ALIGN(CFG_CTS_MAX_SPI_XFER_SIZE+10,4)];
    u8 spi_rx_buf[ALIGN(CFG_CTS_MAX_SPI_XFER_SIZE+10,4)];
    u8 spi_tx_buf[ALIGN(CFG_CTS_MAX_SPI_XFER_SIZE+10,4)];
    u32 spi_speed;
#endif /* CONFIG_CTS_I2C_HOST */

    struct cts_device_touch_info touch_info;
};

struct cts_device {
    struct cts_platform_data *pdata;

    const struct cts_device_hwdata   *hwdata;
    struct cts_device_fwdata          fwdata;
    struct cts_device_rtdata          rtdata;
    const struct cts_flash           *flash;
    bool enabled;
};

struct chipone_ts_data {
    struct touchpanel_data *tsdata;
    struct cts_firmware vfw;

    int touch_direction;
#ifdef CONFIG_CTS_I2C_HOST
    struct i2c_client *i2c_client;
#else
    struct spi_device *spi_client;
#endif /* CONFIG_CTS_I2C_HOST */

    struct device *device;

    struct cts_device cts_dev;

    struct cts_platform_data *pdata;

    struct workqueue_struct *workqueue;

    void *vendor_data;

#ifdef CONFIG_CTS_LEGACY_TOOL
    struct proc_dir_entry *procfs_entry;
#endif /* CONFIG_CTS_LEGACY_TOOL */

};

/* Platform part */
#ifdef CONFIG_CTS_I2C_HOST
extern size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata);
extern u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata,
        size_t xfer_size);
extern int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *src, size_t len, int retry, int delay);
extern int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay);
extern int cts_plat_is_i2c_online(struct cts_platform_data *pdata, u8 i2c_addr);
#else /* CONFIG_CTS_I2C_HOST */
extern size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata);
extern u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size);
extern int cts_plat_spi_write(struct cts_platform_data *pdata, u8 i2c_addr, const void *src,
        size_t len, int retry, int delay);
extern int cts_plat_spi_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay);
extern int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay, int idle);
extern int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len , u8 *tx_buffer, u8 *rx_buffer);
#endif /* CONFIG_CTS_I2C_HOST */

#ifdef CONFIG_CTS_I2C_HOST
extern int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client);
#else
extern int cts_plat_is_normal_mode(struct cts_platform_data *pdata);
extern int cts_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi);
extern int cts_plat_spi_set_mode(struct cts_platform_data *pdata);
#endif

#ifdef CFG_CTS_HAS_RESET_PIN
extern int cts_plat_reset_device(struct cts_platform_data *pdata);
#else
static inline int cts_plat_reset_device(struct cts_platform_data *pdata) {return 0;}
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
extern size_t cts_plat_get_max_fw_log_size(struct cts_platform_data *pdata);
extern u8 *cts_plat_get_fw_log_buf(struct cts_platform_data *pdata, size_t size);
#endif

extern int cts_plat_set_reset(struct cts_platform_data *pdata, int val);
extern int cts_plat_get_int_pin(struct cts_platform_data *pdata);



static inline u32 get_unaligned_le24(const void *p)
{
    const u8 *puc = (const u8 *)p;
    return (puc[0] | (puc[1] << 8) | (puc[2] << 16));
}

static inline u32 get_unaligned_be24(const void *p)
{
    const u8 *puc = (const u8 *)p;
    return (puc[2] | (puc[1] << 8) | (puc[0] << 16));
}

static inline void put_unaligned_be24(u32 v, void *p)
{
    u8 *puc = (u8 *)p;

    puc[0] = (v >> 16) & 0xFF;
    puc[1] = (v >> 8 ) & 0xFF;
    puc[2] = (v >> 0 ) & 0xFF;
}

#define wrap(max,x)        ((max) - 1 - (x))

extern void cts_lock_device(const struct cts_device *cts_dev);
extern void cts_unlock_device(const struct cts_device *cts_dev);

extern int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay);
extern int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay);
extern int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay);
extern int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay);
extern int cts_sram_writesb_check_crc_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, u32 crc, int retry);

extern int cts_sram_readb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay);
extern int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay);
extern int cts_sram_readl_retry(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay);
extern int cts_sram_readsb_retry(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay);

extern int cts_fw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay);
extern int cts_fw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay);
extern int cts_fw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay);
extern int cts_fw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay);

extern int cts_fw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay);
extern int cts_fw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay);
extern int cts_fw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay);
extern int cts_fw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay);
extern int cts_fw_reg_readsb_retry_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay, int idle);

extern int cts_hw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay);
extern int cts_hw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay);
extern int cts_hw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay);
extern int cts_hw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay);

extern int cts_hw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay);
extern int cts_hw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay);
extern int cts_hw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay);
extern int cts_hw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay);

static inline int cts_fw_reg_writeb(const struct cts_device *cts_dev, u32 reg_addr, u8 b)
{
    return cts_fw_reg_writeb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_fw_reg_writew(const struct cts_device *cts_dev, u32 reg_addr, u16 w)
{
    return cts_fw_reg_writew_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_fw_reg_writel(const struct cts_device *cts_dev, u32 reg_addr, u32 l)
{
    return cts_fw_reg_writel_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_fw_reg_writesb(const struct cts_device *cts_dev, u32 reg_addr,
        const void *src, size_t len)
{
    return cts_fw_reg_writesb_retry(cts_dev, reg_addr, src, len, 1, 0);
}

static inline int cts_fw_reg_readb(const struct cts_device *cts_dev, u32 reg_addr, u8 *b)
{
    return cts_fw_reg_readb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_fw_reg_readw(const struct cts_device *cts_dev, u32 reg_addr, u16 *w)
{
    return cts_fw_reg_readw_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_fw_reg_readl(const struct cts_device *cts_dev, u32 reg_addr, u32 *l)
{
    return cts_fw_reg_readl_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_fw_reg_readsb(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len)
{
    return cts_fw_reg_readsb_retry(cts_dev, reg_addr, dst, len, 1, 0);
}

static inline int cts_fw_reg_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int idle)
{
    return cts_fw_reg_readsb_retry_delay_idle(cts_dev, reg_addr, dst, len, 1, 0, idle);
}

static inline int cts_hw_reg_writeb(const struct cts_device *cts_dev, u32 reg_addr, u8 b)
{
    return cts_hw_reg_writeb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_writew(const struct cts_device *cts_dev, u32 reg_addr, u16 w)
{
    return cts_hw_reg_writew_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_hw_reg_writel(const struct cts_device *cts_dev, u32 reg_addr, u32 l)
{
    return cts_hw_reg_writel_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_hw_reg_writesb(const struct cts_device *cts_dev, u32 reg_addr,
        const void *src, size_t len)
{
    return cts_hw_reg_writesb_retry(cts_dev, reg_addr, src, len, 1, 0);
}

static inline int cts_hw_reg_readb(const struct cts_device *cts_dev, u32 reg_addr, u8 *b)
{
    return cts_hw_reg_readb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_readw(const struct cts_device *cts_dev, u32 reg_addr, u16 *w)
{
    return cts_hw_reg_readw_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_hw_reg_readl(const struct cts_device *cts_dev, u32 reg_addr, u32 *l)
{
    return cts_hw_reg_readl_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_hw_reg_readsb(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len)
{
    return cts_hw_reg_readsb_retry(cts_dev, reg_addr, dst, len, 1, 0);
}

static inline int cts_sram_writeb(const struct cts_device *cts_dev, u32 addr, u8 b)
{
    return cts_sram_writeb_retry(cts_dev, addr, b, 1, 0);
}

static inline int cts_sram_writew(const struct cts_device *cts_dev, u32 addr, u16 w)
{
    return cts_sram_writew_retry(cts_dev, addr, w, 1, 0);
}

static inline int cts_sram_writel(const struct cts_device *cts_dev, u32 addr, u32 l)
{
    return cts_sram_writel_retry(cts_dev, addr, l, 1, 0);
}

static inline int cts_sram_writesb(const struct cts_device *cts_dev, u32 addr,
        const void *src, size_t len)
{
    return cts_sram_writesb_retry(cts_dev, addr, src, len, 1, 0);
}

static inline int cts_sram_readb(const struct cts_device *cts_dev, u32 addr, u8 *b)
{
    return cts_sram_readb_retry(cts_dev, addr, b, 1, 0);
}

static inline int cts_sram_readw(const struct cts_device *cts_dev, u32 addr, u16 *w)
{
    return cts_sram_readw_retry(cts_dev, addr, w, 1, 0);
}

static inline int cts_sram_readl(const struct cts_device *cts_dev, u32 addr, u32 *l)
{
    return cts_sram_readl_retry(cts_dev, addr, l, 1, 0);
}

static inline int cts_sram_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len)
{
    return cts_sram_readsb_retry(cts_dev, addr, dst, len, 1, 0);
}

#ifdef CONFIG_CTS_I2C_HOST
static inline void cts_set_program_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr     = CTS_DEV_PROGRAM_MODE_I2CADDR;
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.addr_width   = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;
}

static inline void cts_set_normal_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr     = CTS_DEV_NORMAL_MODE_I2CADDR;
    cts_dev->rtdata.program_mode = false;
    cts_dev->rtdata.addr_width   = CTS_DEV_NORMAL_MODE_ADDR_WIDTH;
}
#else
static inline void cts_set_program_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr     = CTS_DEV_PROGRAM_MODE_SPIADDR;
    cts_dev->rtdata.program_mode   = true;
    cts_dev->rtdata.addr_width     = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;
}

static inline void cts_set_normal_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr     = CTS_DEV_NORMAL_MODE_SPIADDR;
    cts_dev->rtdata.program_mode   = false;
    cts_dev->rtdata.addr_width     = CTS_DEV_NORMAL_MODE_ADDR_WIDTH;
}
#endif


extern bool cts_is_device_program_mode(const struct cts_device *cts_dev);
extern int cts_enter_program_mode(struct cts_device *cts_dev);
extern int cts_enter_normal_mode(struct cts_device *cts_dev);

extern int cts_probe_device(struct cts_device *cts_dev);
extern int cts_set_work_mode(const struct cts_device *cts_dev, u8 mode);
extern int cts_get_work_mode(const struct cts_device *cts_dev, u8 *mode);
extern int cts_get_firmware_version(const struct cts_device *cts_dev, u16 *version);
extern int cts_get_ddi_version(const struct cts_device *cts_dev, u8 *version);
extern int cts_get_lib_version(const struct cts_device *cts_dev, u16 *lib_version);
extern int cts_get_data_ready_flag(const struct cts_device *cts_dev, u8 *flag);
extern int cts_clr_data_ready_flag(const struct cts_device *cts_dev);
extern int cts_send_command(const struct cts_device *cts_dev, u8 cmd);
extern int cts_get_touchinfo(const struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info);
extern int cts_get_panel_param(const struct cts_device *cts_dev,
        void *param, size_t size);
extern int cts_set_panel_param(const struct cts_device *cts_dev,
        const void *param, size_t size);
extern int cts_get_x_resolution(const struct cts_device *cts_dev, u16 *resolution);
extern int cts_get_y_resolution(const struct cts_device *cts_dev, u16 *resolution);
extern int cts_get_num_rows(const struct cts_device *cts_dev, u8 *num_rows);
extern int cts_get_num_cols(const struct cts_device *cts_dev, u8 *num_cols);
extern int cts_get_dev_esd_protection(struct cts_device *cts_dev, bool *enable);
extern int cts_set_dev_esd_protection(struct cts_device *cts_dev, bool enable);

enum cts_get_touch_data_flags {
    CTS_GET_TOUCH_DATA_FLAG_ENABLE_GET_TOUCH_DATA_BEFORE = BIT(0),
    CTS_GET_TOUCH_DATA_FLAG_CLEAR_DATA_READY             = BIT(1),
    CTS_GET_TOUCH_DATA_FLAG_REMOVE_TOUCH_DATA_BORDER     = BIT(2),
    CTS_GET_TOUCH_DATA_FLAG_FLIP_TOUCH_DATA              = BIT(3),
    CTS_GET_TOUCH_DATA_FLAG_DISABLE_GET_TOUCH_DATA_AFTER = BIT(4),
};
extern int cts_enable_get_rawdata(const struct cts_device *cts_dev);
extern int cts_disable_get_rawdata(const struct cts_device *cts_dev);
extern int cts_get_rawdata(const struct cts_device *cts_dev, void *buf);
extern int cts_get_diffdata(const struct cts_device *cts_dev, void *buf);
extern int cts_get_baseline(const struct cts_device *cts_dev, void *baseline,
    enum cts_work_mode work_mode, u32 flags, u16 addr, u8 data_width);
extern int cts_get_compensate_cap(struct cts_device *cts_dev, u8 *cap);
extern int cts_get_fwid(struct cts_device *cts_dev, u16 *fwid);
extern int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid);

#ifdef CFG_CTS_GESTURE
extern int  cts_get_gesture_info(const struct cts_device *cts_dev,
        void *gesture_info, bool trace_point);
#endif

#ifdef CONFIG_CTS_GLOVE
extern int cts_enter_glove_mode(struct cts_device *cts_dev);
extern int cts_exit_glove_mode(struct cts_device *cts_dev);
int cts_is_glove_enabled(const struct cts_device *cts_dev);
#endif

extern int cts_set_dev_charger_attached(struct cts_device *cts_dev, bool attached);
extern int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached);

/* Firmware ops */
#define FIRMWARE_VERSION_OFFSET     0x100
#define FIRMWARE_VERSION(firmware)  \
    get_unaligned_le16((firmware)->data + FIRMWARE_VERSION_OFFSET)
extern u32 cts_crc32(const u8 *data, size_t len);
extern int cts_update_firmware(struct cts_device *cts_dev,
        const struct cts_firmware *firmware, bool to_flash);
extern bool cts_is_firmware_updating(const struct cts_device *cts_dev);


#ifdef CONFIG_CTS_LEGACY_TOOL
extern int  cts_tool_init(struct chipone_ts_data *cts_data);
extern void cts_tool_deinit(struct chipone_ts_data *data);
#else
static inline int  cts_tool_init(struct chipone_ts_data *cts_data) {return 0;}
static inline void cts_tool_deinit(struct chipone_ts_data *data) {}
#endif

extern bool cts_is_device_enabled(const struct cts_device *cts_dev);
extern int cts_start_device(struct cts_device *cts_dev);
extern int cts_stop_device(struct cts_device *cts_dev);

#ifdef CFG_CTS_FW_LOG_REDIRECT
extern int cts_enable_fw_log_redirect(struct cts_device *cts_dev);
extern int cts_disable_fw_log_redirect(struct cts_device *cts_dev);
extern bool cts_is_fw_log_redirect(struct cts_device *cts_dev);
extern int cts_fw_log_show_finish(struct cts_device *cts_dev);
#endif

extern const char *cts_dev_boot_mode2str(u8 boot_mode);
extern bool cts_is_fwid_valid(u16 fwid);

extern struct chipone_ts_data *cts_dev_get_drvdata(struct device *dev);

#endif /* CTS_CORE_H */

