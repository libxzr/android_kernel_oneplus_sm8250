#ifndef CTS_CONFIG_H
#define CTS_CONFIG_H

/** Driver version */
#define CFG_CTS_DRIVER_MAJOR_VERSION        1
#define CFG_CTS_DRIVER_MINOR_VERSION        5
#define CFG_CTS_DRIVER_PATCH_VERSION        1
#define CFG_CTS_DRIVER_VERSION              "v1.5.1"

#define CFG_CTS_HAS_RESET_PIN

//#define CONFIG_CTS_I2C_HOST
#ifndef CONFIG_CTS_I2C_HOST
#define CFG_CTS_SPI_SPEED_KHZ               9600
#endif

#ifdef CONFIG_PROC_FS
#define CONFIG_CTS_LEGACY_TOOL
#define CFG_CTS_TOOL_PROC_FILENAME      "icn85xx_tool"
#endif

#ifdef CONFIG_SYSFS
#define CONFIG_CTS_SYSFS
#endif

#define CFG_CTS_MAX_TOUCH_NUM               (10)

#define CFG_CTS_GESTURE

//#define CFG_CTS_FW_LOG_REDIRECT

/* Platform configurations */
#define CFG_CTS_MAX_I2C_XFER_SIZE           (128)

#define CFG_CTS_MAX_SPI_XFER_SIZE           (1400u)

#define CTS_FW_LOG_REDIRECT_SIGN            0x60
#define CTS_FW_LOG_BUF_LEN                  128

#define CFG_CTS_DEVICE_NAME                 "chipone-tddi"
#define CFG_CTS_DRIVER_NAME                 "chipone-tddi"

#define CFG_CTS_OF_DEVICE_ID_NAME           "chipone-tddi"
#define CFG_CTS_OF_INT_GPIO_NAME            "chipone,irq-gpio"
#define CFG_CTS_OF_RST_GPIO_NAME            "chipone,rst-gpio"
#endif /* CTS_CONFIG_H */

