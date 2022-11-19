#define LOG_TAG         "Core"

#include "cts_config.h"
#include "cts_core.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_strerror.h"


#ifdef CONFIG_CTS_I2C_HOST
static int cts_i2c_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8  buff[8];

    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%02x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, b);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writeb invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 1, retry ,delay);
}

static int cts_i2c_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8  buff[8];

    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%04x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, w);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writew invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_i2c_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8  buff[8];

    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%08x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, l);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writel invalid address width %u",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
}

static int cts_i2c_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
    int ret;
    u8 *data;
    size_t max_xfer_size;
    size_t payload_len;
    size_t xfer_len;

    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x len: %zu\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    data = cts_plat_get_i2c_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, data);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, data);
        } else {
            TPD_INFO("<E> Writesb invalid address width %u\n",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_i2c_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                data, xfer_len, retry, delay);
        if (ret) {
            TPD_INFO("<E> Platform i2c write failed %d\n", ret);
            return ret;
        }

        src  += payload_len;
        len  -= payload_len;
        addr += payload_len;
    }

    return 0;
}

static int cts_i2c_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

    TPD_DEBUG("<D> Readb from slave_addr: 0x%02x reg: 0x%0*x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readb invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_i2c_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[2];

    TPD_DEBUG("<D> Readw from slave_addr: 0x%02x reg: 0x%0*x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readw invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0) {
        *w = get_unaligned_le16(buff);
    }

    return ret;
}

static int cts_i2c_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[4];

    TPD_DEBUG("<D> Readl from slave_addr: 0x%02x reg: 0x%0*x\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readl invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0) {
        *l = get_unaligned_le32(buff);
    }

    return ret;
}

static int cts_i2c_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

    TPD_DEBUG("<D> Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu\n",
        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            TPD_INFO("<E> Readsb invalid address width %u\n",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_i2c_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay);
        if (ret) {
            TPD_INFO("<E> Platform i2c read failed %d\n", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }

    return 0;
}
#else
static int cts_spi_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    u8  buff[8];

//    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%02x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, b);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writeb invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }
    buff[cts_dev->rtdata.addr_width] = b;

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr, buff,
        cts_dev->rtdata.addr_width + 1, retry ,delay);
}

static int cts_spi_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8  buff[8];

//    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%04x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, w);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writew invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le16(w, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 2, retry, delay);
}

static int cts_spi_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8  buff[8];

//    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x val: 0x%08x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, l);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, buff);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, buff);
    } else {
        TPD_INFO("<E> Writel invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    put_unaligned_le32(l, buff + cts_dev->rtdata.addr_width);

    return cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            buff, cts_dev->rtdata.addr_width + 4, retry, delay);
}

static int cts_spi_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
#if 1
    int ret;
    u8 *data;
    size_t max_xfer_size;
    size_t payload_len;
    size_t xfer_len;

//    TPD_DEBUG("<D> Write to slave_addr: 0x%02x reg: 0x%0*x len: %zu\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    data = cts_plat_get_spi_xfer_buf(cts_dev->pdata, len);
    while (len) {
        payload_len =
            min((size_t)(max_xfer_size - cts_dev->rtdata.addr_width), len);
        xfer_len = payload_len + cts_dev->rtdata.addr_width;

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, data);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, data);
        } else {
            TPD_INFO("<E> Writesb invalid address width %u\n",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        memcpy(data + cts_dev->rtdata.addr_width, src, payload_len);

        ret = cts_plat_spi_write(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                data, xfer_len, retry, delay);
        if (ret) {
            TPD_INFO("<E> Platform i2c write failed %d\n", ret);
            return ret;
        }

        src  += payload_len;
        len  -= payload_len;
        addr += payload_len;
    }
#endif
    return 0;
}

static int cts_spi_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    u8 addr_buf[4];

//    TPD_DEBUG("<D> Readb from slave_addr: 0x%02x reg: 0x%0*x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readb invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    return cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, b, 1, retry, delay);
}

static int cts_spi_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[2];

//    TPD_DEBUG("<D> Readw from slave_addr: 0x%02x reg: 0x%0*x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readw invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 2, retry, delay);
    if (ret == 0) {
        *w = get_unaligned_le16(buff);
    }

    return ret;
}

static int cts_spi_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8  addr_buf[4];
    u8  buff[4];

//    TPD_DEBUG("<D> Readl from slave_addr: 0x%02x reg: 0x%0*x\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr);

    if (cts_dev->rtdata.addr_width == 2) {
        put_unaligned_be16(addr, addr_buf);
    } else if (cts_dev->rtdata.addr_width == 3) {
        put_unaligned_be24(addr, addr_buf);
    } else {
        TPD_INFO("<E> Readl invalid address width %u\n",
            cts_dev->rtdata.addr_width);
        return -EINVAL;
    }

    ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
            addr_buf, cts_dev->rtdata.addr_width, buff, 4, retry, delay);
    if (ret == 0) {
        *l = get_unaligned_le32(buff);
    }

    return ret;
}

static int cts_spi_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

//    TPD_DEBUG("<D> Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            TPD_INFO("<E> Readsb invalid address width %u\n",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay);
        if (ret) {
            TPD_INFO("<E> Platform i2c read failed %d\n", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }
    return 0;
}

static int cts_spi_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay, int idle)
{
    int ret;
    u8 addr_buf[4];
    size_t max_xfer_size, xfer_len;

//    TPD_DEBUG("<D> Readsb from slave_addr: 0x%02x reg: 0x%0*x len: %zu\n",
//        cts_dev->rtdata.slave_addr, cts_dev->rtdata.addr_width * 2, addr, len);

    max_xfer_size = cts_plat_get_max_spi_xfer_size(cts_dev->pdata);
    while (len) {
        xfer_len = min(max_xfer_size, len);

        if (cts_dev->rtdata.addr_width == 2) {
            put_unaligned_be16(addr, addr_buf);
        } else if (cts_dev->rtdata.addr_width == 3) {
            put_unaligned_be24(addr, addr_buf);
        } else {
            TPD_INFO("<E> Readsb invalid address width %u\n",
                cts_dev->rtdata.addr_width);
            return -EINVAL;
        }

        ret = cts_plat_spi_read_delay_idle(cts_dev->pdata, cts_dev->rtdata.slave_addr,
                addr_buf, cts_dev->rtdata.addr_width, dst, xfer_len, retry, delay, idle);
        if (ret) {
            TPD_INFO("<E> Platform i2c read failed %d\n", ret);
            return ret;
        }

        dst  += xfer_len;
        len  -= xfer_len;
        addr += xfer_len;
    }
    return 0;
}

#endif /* CONFIG_CTS_I2C_HOST */

static inline int cts_dev_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writeb(cts_dev, addr, b, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writeb(cts_dev, addr, b, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writew(cts_dev, addr, w, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writew(cts_dev, addr, w, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writel(cts_dev, addr, l, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writel(cts_dev, addr, l, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_writesb(const struct cts_device *cts_dev, u32 addr,
        const u8 *src, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_writesb(cts_dev, addr, src, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_writesb(cts_dev, addr, src, len, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readb(cts_dev, addr, b, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readb(cts_dev, addr, b, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readw(cts_dev, addr, w, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readw(cts_dev, addr, w, retry, delay);;
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readl(cts_dev, addr, l, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readl(cts_dev, addr, l, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readsb(cts_dev, addr, dst, len, retry, delay);
#endif /* CONFIG_CTS_I2C_HOST */
}

static inline int cts_dev_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay, int idle)
{
#ifdef CONFIG_CTS_I2C_HOST
    return cts_i2c_readsb(cts_dev, addr, dst, len, retry, delay);
#else /* CONFIG_CTS_I2C_HOST */
    return cts_spi_readsb_delay_idle(cts_dev, addr, dst, len, retry, delay, idle);
#endif /* CONFIG_CTS_I2C_HOST */
}

static int cts_write_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    int i, ret;
    u8    buff[5];

    for (i = 0; i < len; i++) {
        put_unaligned_le32(addr, buff);
        buff[4] = *(u8 *)src;

        addr++;
        src++;

        ret = cts_dev_writesb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, buff, 5, retry, delay);
        if (ret) {
            TPD_INFO("<E> Write rDEBUG_INTF len=5B failed %d\n",
                    ret);
            return ret;
        }
    }

    return 0;
}

int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writeb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, &b, 1, retry, delay);
    }
}

int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay)
{
    u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writew(cts_dev, addr, w, retry, delay);
    } else {
        put_unaligned_le16(w, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
    }
}

int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay)
{
    u8 buff[4];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writel(cts_dev, addr, l, retry, delay);
    } else {
        put_unaligned_le32(l, buff);

        return cts_write_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
    }
}

int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_writesb(cts_dev, addr, src, len, retry, delay);
    } else {
        return cts_write_sram_normal_mode(cts_dev, addr, src, len, retry, delay);
    }
}

static int cts_calc_sram_crc(const struct cts_device *cts_dev,
    u32 sram_addr, size_t size, u32 *crc)
{
    TPD_INFO("<I> Calc crc from sram 0x%06x size %zu\n", sram_addr, size);

    return cts_dev->hwdata->sfctrl->ops->calc_sram_crc(cts_dev,
        sram_addr, size, crc);
}

int cts_sram_writesb_check_crc_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, u32 crc, int retry)
{
    int ret, retries;

    retries = 0;
    do {
        u32 crc_sram;

        retries++;

        if ((ret = cts_sram_writesb(cts_dev, 0, src, len)) != 0) {
            TPD_INFO("<E> SRAM writesb failed %d\n", ret);
            continue;
        }

        if ((ret = cts_calc_sram_crc(cts_dev, 0, len, &crc_sram)) != 0) {
            TPD_INFO("<E> Get CRC for sram writesb failed %d retries %d\n",
                ret, retries);
            continue;
        }

        if (crc == crc_sram) {
            return 0;
        }

        TPD_INFO("<E> Check CRC for sram writesb mismatch %x != %x retries %d\n",
                crc, crc_sram, retries);
        ret = -EFAULT;
    }while (retries < retry);

    return ret;
}

static int cts_read_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        ret = cts_dev_writel(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF, addr, retry, delay);
        if (ret) {
            TPD_INFO("<E> Write addr to rDEBUG_INTF failed %d\n", ret);
            return ret;
        }

        ret = cts_dev_readb(cts_dev,
                CTS_DEVICE_FW_REG_DEBUG_INTF + 4, (u8 *)dst, retry, delay);
        if (ret) {
            TPD_INFO("<E> Read value from rDEBUG_INTF + 4 failed %d\n",
                ret);
            return ret;
        }

        addr++;
        dst++;
    }

    return 0;
}

int cts_sram_readb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readb(cts_dev, addr, b, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, b, 1, retry, delay);
    }
}

int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay)
{
    int ret;
    u8 buff[2];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readw(cts_dev, addr, w, retry, delay);
    } else {
        ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 2, retry, delay);
        if (ret) {
            TPD_INFO("<E> SRAM readw in normal mode failed %d\n", ret);
            return ret;
        }

        *w = get_unaligned_le16(buff);

        return 0;
    }
}

int cts_sram_readl_retry(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay)
{
    int ret;
    u8 buff[4];

    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readl(cts_dev, addr, l, retry, delay);
    } else {
        ret = cts_read_sram_normal_mode(cts_dev, addr, buff, 4, retry, delay);
        if (ret) {
            TPD_INFO("<E> SRAM readl in normal mode failed %d\n", ret);
            return ret;
        }

        *l = get_unaligned_le32(buff);

        return 0;
    }
}

int cts_sram_readsb_retry(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        return cts_dev_readsb(cts_dev, addr, dst, len, retry, delay);
    } else {
        return cts_read_sram_normal_mode(cts_dev, addr, dst, len, retry, delay);
    }
}

int cts_fw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Writeb to fw reg 0x%04x under program mode\n", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writeb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Writew to fw reg 0x%04x under program mode\n", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writew(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Writel to fw reg 0x%04x under program mode\n", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writel(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Writesb to fw reg 0x%04x under program mode\n", reg_addr);
        return -ENODEV;
    }

    return cts_dev_writesb(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_fw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Readb from fw reg under program mode\n");
        return -ENODEV;
    }

    return cts_dev_readb(cts_dev, reg_addr, b, retry, delay);
}

int cts_fw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Readw from fw reg under program mode\n");
        return -ENODEV;
    }

    return cts_dev_readw(cts_dev, reg_addr, w, retry, delay);
}

int cts_fw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Readl from fw reg under program mode\n");
        return -ENODEV;
    }

    return cts_dev_readl(cts_dev, reg_addr, l, retry, delay);
}

int cts_fw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Readsb from fw reg under program mode\n");
        return -ENODEV;
    }

    return cts_dev_readsb(cts_dev, reg_addr, dst, len, retry, delay);
}
int cts_fw_reg_readsb_retry_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay, int idle)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Readsb from fw reg under program mode\n");
        return -ENODEV;
    }

    return cts_dev_readsb_delay_idle(cts_dev, reg_addr, dst, len, retry, delay, idle);
}


int cts_hw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay)
{
    return cts_sram_writeb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay)
{
    return cts_sram_writew_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay)
{
    return cts_sram_writel_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay)
{
    return cts_sram_writesb_retry(cts_dev, reg_addr, src, len, retry, delay);
}

int cts_hw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay)
{
    return cts_sram_readb_retry(cts_dev, reg_addr, b, retry, delay);
}

int cts_hw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay)
{
    return cts_sram_readw_retry(cts_dev, reg_addr, w, retry, delay);
}

int cts_hw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay)
{
    return cts_sram_readl_retry(cts_dev, reg_addr, l, retry, delay);
}

int cts_hw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay)
{
    return cts_sram_readsb_retry(cts_dev, reg_addr, dst, len, retry, delay);
}

const static struct cts_sfctrl icnl9911_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (80 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9911s_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (64 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

const static struct cts_sfctrl icnl9911c_sfctrl = {
    .reg_base = 0x34000,
    .xchg_sram_base = (64 - 1) * 1024,
    .xchg_sram_size = 1024, /* For non firmware programming */
    .ops = &cts_sfctrlv2_ops
};

#define CTS_DEV_HW_REG_DDI_REG_CTRL     (0x3002Cu)

static int icnl9911_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8  access_flag;

    TPD_INFO("<I> ICNL9911 %s access ddi reg\n", enable ? "enable" : "disable");

    ret = cts_hw_reg_readb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, &access_flag);
    if (ret) {
        TPD_INFO("<E> Read HW_REG_DDI_REG_CTRL failed %d\n", ret);
        return ret;
    }

    access_flag = enable ? (access_flag | 0x01) : (access_flag & (~0x01));
    ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
    if (ret) {
        TPD_INFO("<E> Write HW_REG_DDI_REG_CTRL %02x failed %d\n", access_flag, ret);
        return ret;
    }

    ret = cts_hw_reg_writew(cts_dev, 0x3DFF0, enable ? 0x4BB4 : 0xB44B);
    if (ret) {
        TPD_INFO("<E> Write password failed %d\n");
        goto disable_access_ddi_reg;
    }

    return 0;

disable_access_ddi_reg: {
        int r;

        access_flag = enable ? (access_flag & (~0x01)) : (access_flag | 0x01);
        r = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
        if (r) {
            TPD_INFO("<E> disable_access_ddi_reg\n");
        }
    }

    return ret;
}

static int icnl9911s_set_access_ddi_reg(struct cts_device *cts_dev, bool enable)
{
    int ret;
    u8  access_flag;

    TPD_INFO("<I> ICNL9911S %s access ddi reg\n", enable ? "enable" : "disable");

    ret = cts_hw_reg_readb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, &access_flag);
    if (ret) {
        TPD_INFO("<E> Read HW_REG_DDI_REG_CTRL failed %d\n", ret);
        return ret;
    }

    access_flag = enable ? (access_flag | 0x01) : (access_flag & (~0x01));
    ret = cts_hw_reg_writeb(cts_dev, CTS_DEV_HW_REG_DDI_REG_CTRL, access_flag);
    if (ret) {
        TPD_INFO("<E> Write HW_REG_DDI_REG_CTRL %02x failed %d\n", access_flag, ret);
        return ret;
    }

    ret = cts_hw_reg_writeb(cts_dev, 0x30074, enable ? 1 : 0);
    if (ret) {
        TPD_INFO("<E> Write 0x30074 failed %d\n", access_flag, ret);
        return ret;
    }

    ret = cts_hw_reg_writew(cts_dev, 0x3DFF0, enable ? 0x595A : 0x5A5A);
    if (ret) {
        TPD_INFO("<E> Write password to F0 failed %d\n");
        return ret;
    }
    ret = cts_hw_reg_writew(cts_dev, 0x3DFF4, enable ? 0xA6A5 : 0x5A5A);
    if (ret) {
        TPD_INFO("<E> Write password to F1 failed %d\n");
        return ret;
    }

    return 0;
}

const static struct cts_device_hwdata cts_device_hwdatas[] = {
    {
        .name = "ICNL9911",
        .hwid = CTS_DEV_HWID_ICNL9911,
        .fwid = CTS_DEV_FWID_ICNL9911,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 80 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911_sfctrl,
        .enable_access_ddi_reg = icnl9911_set_access_ddi_reg,
    },
    {
        .name = "ICNL9911S",
        .hwid = CTS_DEV_HWID_ICNL9911S,
        .fwid = CTS_DEV_FWID_ICNL9911S,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 64 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911s_sfctrl,
        .enable_access_ddi_reg = icnl9911s_set_access_ddi_reg,
    },
    {
        .name = "ICNL9911C",
        .hwid = CTS_DEV_HWID_ICNL9911C,
        .fwid = CTS_DEV_FWID_ICNL9911C,
        .num_row = 32,
        .num_col = 18,
        .sram_size = 64 * 1024,

        .program_addr_width = 3,

        .sfctrl = &icnl9911c_sfctrl,
        .enable_access_ddi_reg = icnl9911s_set_access_ddi_reg,
    }
};

static int cts_init_device_hwdata(struct cts_device *cts_dev,
        u32 hwid, u16 fwid)
{
    int i;

    TPD_INFO("<I> Init hardware data hwid: %06x fwid: %04x\n", hwid, fwid);

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (hwid == cts_device_hwdatas[i].hwid ||
            fwid == cts_device_hwdatas[i].fwid) {
            cts_dev->hwdata = &cts_device_hwdatas[i];
            return 0;
        }
    }

    return -EINVAL;
}

const char *cts_work_mode2str(enum cts_work_mode work_mode)
{
#define case_work_mode(mode) \
    case CTS_WORK_MODE_ ## mode: return #mode "_MODE"

    switch (work_mode) {
        case_work_mode(UNKNOWN);
        case_work_mode(SUSPEND);
        case_work_mode(NORMAL_ACTIVE);
        case_work_mode(NORMAL_IDLE);
        case_work_mode(GESTURE_ACTIVE);
        case_work_mode(GESTURE_IDLE);
        default: return "INVALID";
    }

#undef case_work_mode
}

void cts_lock_device(const struct cts_device *cts_dev)
{
    TPD_DEBUG("<D> *** Lock ***\n");

    mutex_lock(&cts_dev->pdata->dev_lock);
}

void cts_unlock_device(const struct cts_device *cts_dev)
{
    TPD_DEBUG("<D> ### Un-Lock ###\n");

    mutex_unlock(&cts_dev->pdata->dev_lock);
}

int cts_set_work_mode(const struct cts_device *cts_dev, u8 mode)
{
    TPD_INFO("<I> Set work mode to %u\n", mode);

    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_work_mode(const struct cts_device *cts_dev, u8 *mode)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
}

int cts_get_firmware_version(const struct cts_device *cts_dev, u16 *version)
{
    int ret = cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_VERSION, version);

    if (ret) {
        *version = 0;
    } else {
        *version = be16_to_cpup(version);
    }

    return ret;
}

int cts_get_ddi_version(const struct cts_device *cts_dev, u8 *version)
{
    int ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_DDI_VERSION, version);

    if (ret) {
        *version = 0;
    }
    return ret;
}

int cts_get_lib_version(const struct cts_device *cts_dev, u16 *lib_version)
{
    u8  main_version, sub_version;
    int ret;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FW_LIB_MAIN_VERSION, &main_version);
    if (ret) {
        TPD_INFO("<E> Get fw lib main version failed %d\n", ret);
        return ret;
    }

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FW_LIB_SUB_VERSION, &sub_version);
    if (ret) {
        TPD_INFO("<E> Get fw lib sub version failed %d\n", ret);
        return ret;
    }

    *lib_version = (main_version << 8) + sub_version;
    return 0;
}


int cts_get_data_ready_flag(const struct cts_device *cts_dev, u8 *flag)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, flag);
}

int cts_clr_data_ready_flag(const struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_DATA_READY, 0);
}

int cts_send_command(const struct cts_device *cts_dev, u8 cmd)
{
    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Send command %u while chip in program mode\n", cmd);
        return -ENODEV;
    }

    return cts_fw_reg_writeb_retry(cts_dev, CTS_DEVICE_FW_REG_CMD, cmd, 3, 0);
}

int cts_get_touchinfo(const struct cts_device *cts_dev,
        struct cts_device_touch_info *touch_info)
{
    TPD_DEBUG("<D> Get touch info\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Get touch info in program mode\n");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO,
            touch_info, sizeof(*touch_info));
}

int cts_get_panel_param(const struct cts_device *cts_dev,
        void *param, size_t size)
{
    TPD_INFO("<I> Get panel parameter\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Get panel parameter in program mode\n");
        return -ENODEV;
    }

    return cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_set_panel_param(const struct cts_device *cts_dev,
        const void *param, size_t size)
{
    TPD_INFO("<I> Set panel parameter\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Set panel parameter in program mode\n");
        return -ENODEV;
    }
    return cts_fw_reg_writesb(cts_dev,
            CTS_DEVICE_FW_REG_PANEL_PARAM, param, size);
}

int cts_get_x_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_X_RESOLUTION, resolution);
}

int cts_get_y_resolution(const struct cts_device *cts_dev, u16 *resolution)
{
    return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_Y_RESOLUTION, resolution);
}

int cts_get_num_rows(const struct cts_device *cts_dev, u8 *num_rows)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_TX, num_rows);
}

int cts_get_num_cols(const struct cts_device *cts_dev, u8 *num_cols)
{
    return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_NUM_RX, num_cols);
}

#define CTS_DEV_FW_ESD_PROTECTION_ON    (3)
#define CTS_DEV_FW_ESD_PROTECTION_OFF   (1)

int cts_get_dev_esd_protection(struct cts_device *cts_dev, bool *enable)
{
    int ret;
    u8  val;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_ESD_PROTECTION, &val);
    if (ret == 0) {
        *enable = (val == CTS_DEV_FW_ESD_PROTECTION_ON);
    }

    return ret;
}

int cts_set_dev_esd_protection(struct cts_device *cts_dev, bool enable)
{
    TPD_INFO("<I> %s ESD protection\n",  enable ? "Enable" : "Disable");

    return cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_ESD_PROTECTION,
        enable ? CTS_DEV_FW_ESD_PROTECTION_ON : CTS_DEV_FW_ESD_PROTECTION_OFF);
}

int cts_enable_get_rawdata(const struct cts_device *cts_dev)
{
    int i, ret;

    TPD_INFO("<I> Enable get touch data\n");

    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_RAWDATA);
    if (ret) {
        TPD_INFO("<E> Send CMD_ENABLE_READ_RAWDATA failed %d(%s)\n",
            ret, cts_strerror(ret));
        return ret;
    }

    for (i = 0; i < 100; i++) {
        u8 enabled = 0;

        ret = cts_fw_reg_readb(cts_dev, 0x12, &enabled);
        if (ret) {
            TPD_INFO("<E> Get enable get touch data flag failed %d(%s)\n",
                ret, cts_strerror(ret));
            goto delay_and_retry;
        }

        if (enabled) {
            return 0;
        } else {
            TPD_DEBUG("<D> Enable get touch data flag NOT set, try again\n");
        }

delay_and_retry:
        mdelay(1);
    }

    return -EIO;
}

int cts_disable_get_rawdata(const struct cts_device *cts_dev)
{
    int i, ret;

    TPD_INFO("<I> Disable get touch data\n");

    ret = cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_RAWDATA);
    if (ret) {
        TPD_INFO("<E> Send CMD_DISABLE_READ_RAWDATA failed %d(%s)\n",
            ret, cts_strerror(ret));
        return ret;
    }

    for (i = 0; i < 100; i++) {
        u8 enabled = 0;

        ret = cts_fw_reg_readb(cts_dev, 0x12, &enabled);
        if (ret) {
            TPD_INFO("<E> Get enable get touch data flag failed %d(%s)\n",
                ret, cts_strerror(ret));
            goto delay_and_retry;
        }

        if (enabled) {
            TPD_DEBUG("<D> Enable get touch data flag STILL set, try again\n");
        } else {
            return 0;
        }

delay_and_retry:
        mdelay(1);
    }

    return -EIO;

}

static void tsdata_flip_x(void *tsdata, u8 fw_rows, u8 fw_cols)
{
    u8 r, c;
    u16 *data;

    data = (u16 *)tsdata;
    for (r = 0; r < fw_rows; r++) {
        for (c = 0; c < fw_cols / 2; c++) {
            swap(data[r * fw_cols + c],
                 data[r * fw_cols + wrap(fw_cols, c)]);
        }
    }
}

static void tsdata_flip_y(void *tsdata, u8 fw_rows, u8 fw_cols)
{
    u8 r, c;
    u16 *data;

    data = (u16 *)tsdata;
    for (r = 0; r < fw_rows / 2; r++) {
        for (c = 0; c < fw_cols; c++) {
            swap(data[r * fw_cols + c],
                 data[wrap(fw_rows, r) * fw_cols + c]);
        }
    }
}

int cts_get_rawdata(const struct cts_device *cts_dev, void *buf)
{
    int i, ret;
    u8 ready;
    u8 retries = 5;

    TPD_INFO("<I> Get rawdata\n");
    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            TPD_INFO("<E> Get data ready flag failed %d\n", ret);
            goto get_raw_exit;
        }
        if (ready) {
            break;
        }
    }
    if (i == 1000) {
        ret = -ENODEV;
        goto get_raw_exit;
    }
    do {
        ret = cts_fw_reg_readsb_delay_idle(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA,
            buf,cts_dev->fwdata.rows*cts_dev->fwdata.cols*2, 500);
        if (ret) {
            TPD_INFO("<E> Read rawdata failed %d\n", ret);
        }
    } while (--retries > 0 && ret != 0);

    if (cts_dev->fwdata.flip_x) {
        tsdata_flip_x(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }
    if (cts_dev->fwdata.flip_y) {
        tsdata_flip_y(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }

    if (cts_clr_data_ready_flag(cts_dev)) {
        TPD_INFO("<E> Clear data ready flag failed\n");
        ret = -ENODEV;
    }
get_raw_exit:
    return ret;
}

int cts_get_diffdata(const struct cts_device *cts_dev, void *buf)
{
    int i, j, ret;
    u8 ready;
    u8 retries = 5;
    u8 *cache_buf;

    TPD_INFO("<I> Get diffdata\n");
    cache_buf = kzalloc(
      (cts_dev->fwdata.rows + 2) * (cts_dev->fwdata.cols + 2) * 2, GFP_KERNEL);
    if (cache_buf == NULL) {
        TPD_INFO("<E> Get diffdata: malloc error\n");
        ret = -ENOMEM;
        goto get_diff_exit;
    }
    /** - Wait data ready flag set */
    for (i = 0; i < 1000; i++) {
        mdelay(1);
        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            TPD_INFO("<E> Get data ready flag failed %d\n", ret);
            goto get_diff_free_buf;
        }
        if (ready) {
            break;
        }
    }
    if (i == 1000) {
        ret = -ENODEV;
        goto get_diff_free_buf;
    }
    do {
        ret = cts_fw_reg_readsb_delay_idle(cts_dev, CTS_DEVICE_FW_REG_DIFF_DATA,
            cache_buf,(cts_dev->fwdata.rows+2)*(cts_dev->fwdata.cols+2)*2, 500);
        if (ret) {
            TPD_INFO("<E> Read diffdata failed %d\n", ret);
        }
    } while (--retries > 0 && ret != 0);

    for (i = 0; i < cts_dev->fwdata.rows; i++) {
        for (j = 0; j < cts_dev->fwdata.cols; j++) {
            ((u8 *)buf)[2 * (i * cts_dev->fwdata.cols + j)]  =
                cache_buf[2 * ((i+1)*(cts_dev->fwdata.cols+2)+j+1)];
            ((u8 *)buf)[2*(i*cts_dev->fwdata.cols + j)+1] =
                cache_buf[2*((i+1)*(cts_dev->fwdata.cols+2)+j+1)+1];
        }
    }

    if (cts_dev->fwdata.flip_x) {
        tsdata_flip_x(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }
    if (cts_dev->fwdata.flip_y) {
        tsdata_flip_y(buf, cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }

    if (cts_clr_data_ready_flag(cts_dev)) {
        TPD_INFO("<E> Clear data ready flag failed\n");
        ret = -ENODEV;
    }
get_diff_free_buf:
    kfree(cache_buf);
get_diff_exit:
    return ret;
}

int cts_get_baseline(const struct cts_device *cts_dev, void *baseline,
    enum cts_work_mode work_mode, u32 flags, u16 addr, u8 data_width)
{
    u8 rows, cols;
    bool use_another_buf;
    void *buf = NULL;
    int buf_size = 0;
    int i, ret;

    switch (work_mode) {
        case CTS_WORK_MODE_NORMAL_ACTIVE:
            if (addr == 0) {
                addr = CTS_DEVICE_FW_REG_BASELINE;
            }
            if (data_width == 0) {
                data_width = 2;
            }
            break;
        case CTS_WORK_MODE_NORMAL_IDLE:
        case CTS_WORK_MODE_GESTURE_ACTIVE:
        case CTS_WORK_MODE_GESTURE_IDLE:
        case CTS_WORK_MODE_SUSPEND:
            TPD_INFO("<E> Get baseline of work mode %d(%s) NOT supported\n",
                work_mode, cts_work_mode2str(work_mode));
            return -ENOTSUPP;
        default:
            TPD_INFO("<E> Get baseline of work mode %d invalid\n",
                work_mode);
            return -EINVAL;
    }

    rows = cts_dev->fwdata.rows;
    cols = cts_dev->fwdata.cols;
    buf_size = (rows + 2) * (cols + 2) * data_width;

    TPD_INFO("<I> Get baseline of work-mode: %d(%s), flags 0x%x, "
             "addr 0x%04x, data-width %u, rows %u, cols %u\n",
        work_mode, cts_work_mode2str(work_mode), flags,
        addr, data_width, rows, cols);

    if (flags & CTS_GET_TOUCH_DATA_FLAG_ENABLE_GET_TOUCH_DATA_BEFORE) {
        ret = cts_enable_get_rawdata(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enable get touch data failed %d(%s)\n",
                ret, cts_strerror(ret));
            return ret;
        }
    }

    /** - Wait data ready flag set */
    TPD_DEBUG("<D>  - Wait data ready...\n");
    for (i = 0; i < 1000; i++) {
        u8 ready;

        ret = cts_get_data_ready_flag(cts_dev, &ready);
        if (ret) {
            TPD_INFO("<E> Get data ready flag failed %d\n", ret);
            goto free_mem;
        }
        if (ready) {
            goto read_baseline;
        }
        mdelay(1);
    }

    TPD_INFO("<E> Wait data ready timeout\n");
    ret = -ETIMEDOUT;
    goto disable_get_touch_data;

read_baseline:
    if (flags & CTS_GET_TOUCH_DATA_FLAG_REMOVE_TOUCH_DATA_BORDER) {
        buf = kzalloc(buf_size, GFP_KERNEL);
        if (buf == NULL) {
            TPD_INFO("<E> Alloc mem for baseline failed\n");
            ret = -ENOMEM;
            goto clear_data_ready_flag;
        }
        use_another_buf = true;
    } else {
        use_another_buf = false;
    }

    ret = cts_fw_reg_readsb_delay_idle(cts_dev, addr,
        use_another_buf ? buf : baseline, buf_size, 500);
    if (ret) {
        TPD_INFO("<E> Read baseline from 0x%04x size %u failed %d\n",
            addr, buf_size, ret);
        goto free_mem;
    }

    /* Rearrage baseline, remove border rows * cols */
    if (flags & CTS_GET_TOUCH_DATA_FLAG_REMOVE_TOUCH_DATA_BORDER) {
        u8 r;

        TPD_DEBUG("<D>  - Remove border rows & cols\n");

        for (r = 0; r < rows; r++) {
            memcpy(baseline + r * cols * data_width,
                buf + (cols + 3 + r * (cols + 2)) * data_width,
                cols * data_width);
        }
    }

    /* Do flip if neccessary */
    if (flags & CTS_GET_TOUCH_DATA_FLAG_FLIP_TOUCH_DATA) {
        if (cts_dev->fwdata.flip_x) {
            TPD_DEBUG("<D>  - Flip baseline on row\n");
            tsdata_flip_x(baseline, rows, cols);
        }
        if (cts_dev->fwdata.flip_y) {
            TPD_DEBUG("<D>  - Flip baseline on col\n");
            tsdata_flip_y(baseline, rows, cols);
        }
    }

free_mem:
    if (buf) {
        kfree(buf);
    }

clear_data_ready_flag:
    if (flags & CTS_GET_TOUCH_DATA_FLAG_CLEAR_DATA_READY) {
        int r;

        TPD_DEBUG("<D>  - Clear data ready flag\n");
        r = cts_clr_data_ready_flag(cts_dev);
        if (r) {
            TPD_INFO("<E> Clear data ready flag failed %d(%s)\n",
                r, cts_strerror(r));
        }
    }

disable_get_touch_data:
    if (flags & CTS_GET_TOUCH_DATA_FLAG_DISABLE_GET_TOUCH_DATA_AFTER) {
        int r;

        r = cts_disable_get_rawdata(cts_dev);
        if (r) {
            TPD_INFO("<E> Enable get touch data failed %d(%s)\n",
                r, cts_strerror(r));
            // Ignore this error
        }
    }

    return ret;
}

static int cts_get_dev_boot_mode(const struct cts_device *cts_dev,
        u8 *boot_mode)
{
    int ret;

    ret = cts_hw_reg_readb_retry(cts_dev,
        CTS_DEV_HW_REG_CURRENT_MODE, boot_mode, 5, 10);
    if (ret) {
        TPD_INFO("<E> Read boot mode failed %d\n", ret);
        return ret;
    }

    *boot_mode &= CTS_DEV_BOOT_MODE_MASK;

    TPD_INFO("<I> Curr dev boot mode: %u(%s)\n", *boot_mode,
        cts_dev_boot_mode2str(*boot_mode));
    return 0;
}

static int cts_set_dev_boot_mode(const struct cts_device *cts_dev,
        u8 boot_mode)
{
    int ret;

    TPD_INFO("<I> Set dev boot mode to %u(%s)\n", boot_mode,
        cts_dev_boot_mode2str(boot_mode));

    ret = cts_hw_reg_writeb_retry(cts_dev, CTS_DEV_HW_REG_BOOT_MODE,
        boot_mode, 5, 5);
    if (ret) {
        TPD_INFO("<E> Write hw register BOOT_MODE failed %d\n", ret);
        return ret;
    }

    return 0;
}

static int cts_init_fwdata(struct cts_device *cts_dev)
{
    struct cts_device_fwdata *fwdata = &cts_dev->fwdata;
    u8  val;
    int ret;

    TPD_INFO("<I> Init firmware data\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Init firmware data while in program mode\n");
        return -EINVAL;
    }

    ret = cts_get_firmware_version(cts_dev, &fwdata->version);
    if (ret) {
        TPD_INFO("<E> Read firmware version failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %04x\n", "Firmware version", fwdata->version);

    ret = cts_get_lib_version(cts_dev, &fwdata->lib_version);
    if (ret) {
        TPD_INFO("<E> Read firmware Lib version failed %d\n", ret);
    }
    TPD_INFO("<I>   %-24s: v%x.%x\n", "Fimrware lib verion",
        (u8)(fwdata->lib_version >> 8),
        (u8)(fwdata->lib_version));

    ret = cts_get_ddi_version(cts_dev, &fwdata->ddi_version);
    if (ret) {
        TPD_INFO("<E> Read ddi version failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %02x\n", "DDI init code verion", fwdata->ddi_version);

    ret = cts_get_x_resolution(cts_dev, &fwdata->res_x);
    if (ret) {
        TPD_INFO("<E> Read firmware X resoltion failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %u\n", "X resolution", fwdata->res_x);

    ret = cts_get_y_resolution(cts_dev, &fwdata->res_y);
    if (ret) {
        TPD_INFO("<E> Read firmware Y resolution failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %u\n", "Y resolution", fwdata->res_y);

    ret = cts_get_num_rows(cts_dev, &fwdata->rows);
    if (ret) {
        TPD_INFO("<E> Read firmware num TX failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %u\n", "Num rows", fwdata->rows);

    ret = cts_get_num_cols(cts_dev, &fwdata->cols);
    if (ret) {
        TPD_INFO("<E> Read firmware num RX failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %u\n", "Num cols", fwdata->cols);

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &val);
    if (ret) {
        TPD_INFO("<E> Read FW_REG_FLIP_X/Y failed %d\n", ret);
        return ret;
    }
    cts_dev->fwdata.flip_x = !!(val & BIT(2));
    cts_dev->fwdata.flip_y = !!(val & BIT(3));
    TPD_INFO("<I>   %-24s: %s\n", "Flip X",
        cts_dev->fwdata.flip_x ? "True" : "False");
    TPD_INFO("<I>   %-24s: %s\n", "Flip Y",
        cts_dev->fwdata.flip_y ? "True" : "False");

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_SWAP_AXES, &val);
    if (ret) {
        TPD_INFO("<E> Read FW_REG_SWAP_AXES failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %s\n", "Swap axes",
        cts_dev->fwdata.swap_axes ? "True" : "False");

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_INT_MODE, &fwdata->int_mode);
    if (ret) {
        TPD_INFO("<E> Read firmware Int mode failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %s\n", "Int polarity",
        (fwdata->int_mode == 0) ? "LOW" : "HIGH");

    ret = cts_fw_reg_readw(cts_dev,
        CTS_DEVICE_FW_REG_INT_KEEP_TIME, &fwdata->int_keep_time);
    if (ret) {
        TPD_INFO("<E> Read firmware Int keep time failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %d\n", "Int keep time", fwdata->int_keep_time);

    ret = cts_fw_reg_readw(cts_dev,
        CTS_DEVICE_FW_REG_RAWDATA_TARGET, &fwdata->rawdata_target);
    if (ret) {
        TPD_INFO("<E> Read firmware Raw dest value failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %d\n", "Raw target value", fwdata->rawdata_target);


    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_ESD_PROTECTION, &fwdata->esd_method);
    if (ret) {
        TPD_INFO("<E> Read firmware Esd method failed %d\n", ret);
        return ret;
    }
    TPD_INFO("<I>   %-24s: %d\n", "Esd method", fwdata->esd_method);

    return 0;
}

#ifdef CFG_CTS_FW_LOG_REDIRECT
void cts_show_fw_log(struct cts_device *cts_dev)
{
    u8 len, max_len;
    int ret;
    u8 *data;

    max_len = cts_plat_get_max_fw_log_size(cts_dev->pdata);
    data = cts_plat_get_fw_log_buf(cts_dev->pdata, max_len);
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO+1, &len, 1);
    if (ret) {
        TPD_INFO("<E> Get i2c print buf len error\n");
        return;
    }
    if (len >= max_len) {
        len = max_len - 1;
    }
    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO+2, data, len);
    if (ret) {
        TPD_INFO("<E> Get i2c print buf error\n");
        return;
    }
    data[len] = '\0';
    printk("CTS-FW_LOG %s", data);
    cts_fw_log_show_finish(cts_dev);
}
#endif


bool cts_is_device_program_mode(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.program_mode;
}

static inline void cts_init_rtdata_with_normal_mode(struct cts_device *cts_dev)
{
    memset(&cts_dev->rtdata, 0, sizeof(cts_dev->rtdata));

    cts_set_normal_addr(cts_dev);
    cts_dev->rtdata.updating                = false;
    cts_dev->rtdata.testing                 = false;
    cts_dev->rtdata.fw_log_redirect_enabled = false;
    cts_dev->rtdata.glove_mode_enabled      = false;
}

int cts_enter_program_mode(struct cts_device *cts_dev)
{
    const static u8 magic_num[] = {0xCC, 0x33, 0x55, 0x5A};
    u8  boot_mode;
    int ret;

    TPD_INFO("<I> Enter program mode\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Enter program mode while alredy in\n");
        //return 0;
    }

#ifdef CONFIG_CTS_I2C_HOST
    ret = cts_plat_i2c_write(cts_dev->pdata,
            CTS_DEV_PROGRAM_MODE_I2CADDR, magic_num, 4, 5, 10);
    if (ret) {
        TPD_INFO("<E> Write magic number to i2c_dev: 0x%02x failed %d\n",
            CTS_DEV_PROGRAM_MODE_I2CADDR, ret);
        return ret;
    }

    cts_set_program_addr(cts_dev);
    /* Write i2c deglitch register */
    ret = cts_hw_reg_writeb_retry(cts_dev, 0x37001, 0x0F, 5, 1);
    if (ret) {
        TPD_INFO("<E> Write i2c deglitch register failed\n");
        // FALL through
    }
#else
    cts_set_program_addr(cts_dev);
    cts_plat_reset_device(cts_dev->pdata);
    ret = cts_plat_spi_write(cts_dev->pdata,
            0xcc, &magic_num[1], 3, 5, 10);
    if (ret) {
        TPD_INFO("<E> Write magic number to i2c_dev: 0x%02x failed %d\n",
            CTS_DEV_PROGRAM_MODE_SPIADDR, ret);
        return ret;
    }
#endif /* CONFIG_CTS_I2C_HOST */
    ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
    if (ret) {
        TPD_INFO("<E> Read BOOT_MODE failed %d\n", ret);
        return ret;
    }

#ifdef CONFIG_CTS_I2C_HOST
    if (boot_mode != CTS_DEV_BOOT_MODE_I2C_PROGRAM)
#else
    if (boot_mode != CTS_DEV_BOOT_MODE_SPI_PROGRAM)
#endif
    {
        TPD_INFO("<E> BOOT_MODE readback %u != I2C/SPI PROMGRAM mode\n", boot_mode);
        return -EFAULT;
    }

    return 0;
}

const char *cts_dev_boot_mode2str(u8 boot_mode)
{
#define case_boot_mode(mode) \
    case CTS_DEV_BOOT_MODE_ ## mode: return #mode "-BOOT"

    switch (boot_mode) {
        case_boot_mode(FLASH);
        case_boot_mode(I2C_PROGRAM);
        case_boot_mode(SRAM);
        case_boot_mode(SPI_PROGRAM);
        default: return "INVALID";
    }

#undef case_boot_mode
}

int cts_enter_normal_mode(struct cts_device *cts_dev)
{
    int ret = 0;
    u8  boot_mode;
    int retries;

    if (!cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Enter normal mode while already in\n");
        return 0;
    }

    TPD_INFO("<I> Enter normal mode\n");

    cts_set_program_addr(cts_dev);
    ret = cts_set_dev_boot_mode(cts_dev, CTS_DEV_BOOT_MODE_SRAM);
    if (ret) {
        TPD_INFO("<E> Set BOOT_MODE to SRAM_BOOT failed %d(%s)\n",
            ret, cts_strerror(ret));
        // Fall through
    }

    mdelay(50);
    cts_set_normal_addr(cts_dev);

    for(retries = 0; retries < 3; retries++) {
        ret = cts_get_dev_boot_mode(cts_dev, &boot_mode);
        if (ret) {
            TPD_INFO("<E> Get CURR_BOOT_MODE failed %d, try to do reset\n",
                ret, cts_strerror(ret));
            goto reset_device;
        }

        if (boot_mode != CTS_DEV_BOOT_MODE_SRAM &&
            boot_mode != CTS_DEV_BOOT_MODE_FLASH) {
            TPD_INFO("<E> Curr boot mode %u(%s) != SRAM_BOOT/FLASH_BOOT\n",
                boot_mode, cts_dev_boot_mode2str(boot_mode));
            ret = -EIO;
            goto reset_device;
        } else {
            goto init_fwdata;
        }

reset_device:
        cts_plat_reset_device(cts_dev->pdata);
    }

    TPD_INFO("<E> Enter normal mode failed, stay in program mode\n");
    goto stay_in_program_mode;

init_fwdata:
#if 1
    ret = cts_init_fwdata(cts_dev);
    if (ret) {
        TPD_INFO("<E> Init firmware data failed %d(%s)\n",
            ret, cts_strerror(ret));
        return ret;
    }
#endif

    return 0;

stay_in_program_mode:
    cts_set_program_addr(cts_dev);

    return ret;
}

bool cts_is_device_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->enabled;
}

int cts_start_device(struct cts_device *cts_dev)
{
    TPD_INFO("<I> Start device...\n");

    if (cts_is_device_enabled(cts_dev)) {
        TPD_INFO("<W> Start device while already started\n");
        return 0;
    }

    cts_dev->enabled = true;

    TPD_INFO("<I> Start device successfully\n");

    return 0;
}

int cts_stop_device(struct cts_device *cts_dev)
{
    TPD_INFO("<I> Stop device...\n");

    if (!cts_is_device_enabled(cts_dev)) {
        TPD_INFO("<W> Stop device while halted\n");
        return 0;
    }

    if (cts_is_firmware_updating(cts_dev)) {
        TPD_INFO("<W> Stop device while firmware updating, please try again\n");
        return -EAGAIN;
    }

    cts_dev->enabled = false;

    return 0;
}

bool cts_is_fwid_valid(u16 fwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].fwid == fwid) {
            return true;
        }
    }

    return false;
}

static bool cts_is_hwid_valid(u32 hwid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cts_device_hwdatas); i++) {
        if (cts_device_hwdatas[i].hwid == hwid) {
            return true;
        }
    }

    return false;
}

int cts_get_fwid(struct cts_device *cts_dev, u16 *fwid)
{
    int ret;

    TPD_INFO("<I> Get device firmware id\n");

    if (cts_dev->hwdata) {
        *fwid = cts_dev->hwdata->fwid;
        return 0;
    }

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<E> Get device firmware id while in program mode\n");
        ret = -ENODEV;
        goto err_out;
    }

    ret = cts_fw_reg_readw_retry(cts_dev,
            CTS_DEVICE_FW_REG_CHIP_TYPE, fwid, 5, 1);
    if (ret) {
        goto err_out;
    }

    *fwid = be16_to_cpu(*fwid);

    TPD_INFO("<I> Device firmware id: %04x\n", *fwid);

    if (!cts_is_fwid_valid(*fwid)) {
        TPD_INFO("<W> Get invalid firmware id %04x\n", *fwid);
        ret = -EINVAL;
        goto err_out;
    }

    return 0;

err_out:
    *fwid = CTS_DEV_FWID_INVALID;
    return ret;
}

int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid)
{
    int ret;

    TPD_INFO("<I> Get device hardware id\n");

    if (cts_dev->hwdata) {
        *hwid = cts_dev->hwdata->hwid;
        return 0;
    }

    TPD_INFO("<I> Device hardware data not initialized, try to read from register\n");

    if (!cts_dev->rtdata.program_mode) {
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enter program mode failed %d\n", ret);
            goto err_out;
        }
    }

    ret = cts_hw_reg_readl_retry(cts_dev, CTS_DEV_HW_REG_HARDWARE_ID, hwid, 5, 0);
    if (ret) {
        goto err_out;
    }

    *hwid = le32_to_cpu(*hwid);
    *hwid &= 0XFFFFFFF0;
    TPD_INFO("<I> Device hardware id: %04x\n", *hwid);

    if (!cts_is_hwid_valid(*hwid)) {
        TPD_INFO("<W> Device hardware id %04x invalid\n", *hwid);
        ret = -EINVAL;
        goto err_out;
    }

    return 0;

err_out:
    *hwid = CTS_DEV_HWID_INVALID;
    return ret;
}

int cts_probe_device(struct cts_device *cts_dev)
{
    int  ret, retries = 0;
    u16  fwid = CTS_DEV_FWID_INVALID;
    u32  hwid = CTS_DEV_HWID_INVALID;
    u16  device_fw_ver = 0;

    TPD_INFO("<I> Probe device\n");

read_fwid:
#ifdef CONFIG_CTS_I2C_HOST
    if (!cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR)) {
        TPD_INFO("<W> Normal mode i2c addr is offline\n");
    } else
#else
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        TPD_INFO("<W> Normal mode spi addr is offline\n");
    } else
#endif
    {
        cts_init_rtdata_with_normal_mode(cts_dev);
        ret = cts_get_fwid(cts_dev, &fwid);
        if (ret) {
            TPD_INFO("<E> Get firmware id failed %d, retries %d\n", ret, retries);
        } else {
            ret = cts_fw_reg_readw_retry(cts_dev,
                    CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
            if (ret) {
                TPD_INFO("<E> Read firmware version failed %d\n", ret);
                device_fw_ver = 0;
                // TODO: Handle this error
            } else {
                device_fw_ver = be16_to_cpu(device_fw_ver);
                TPD_INFO("<I> Device firmware version: %04x\n", device_fw_ver);
            }
            goto init_hwdata;
        }
    }

    /** - Try to read hardware id,
        it will enter program mode as normal */
    ret = cts_get_hwid(cts_dev, &hwid);
    if (ret || hwid == CTS_DEV_HWID_INVALID) {
        retries++;

        TPD_INFO("<E> Get hardware id failed %d retries %d\n", ret, retries);

        if (retries < 3) {
            cts_plat_reset_device(cts_dev->pdata);
            goto read_fwid;
        } else {
            return -ENODEV;
        }
    }

init_hwdata:
    ret = cts_init_device_hwdata(cts_dev, hwid, fwid);
    if (ret) {
        TPD_INFO("<E> Device hwid: %06x fwid: %04x not found\n", hwid, fwid);
        return -ENODEV;
    }

    return 0;
}

#ifdef CFG_CTS_GESTURE
int cts_get_gesture_info(const struct cts_device *cts_dev,
        void *gesture_info, bool trace_point)
{
    int ret;

    TPD_INFO("<I> Get gesture info\n");

    if (cts_dev->rtdata.program_mode) {
        TPD_INFO("<W> Get gesture info in program mode\n");
        return -ENODEV;
    }

    ret = cts_fw_reg_readsb(cts_dev,
            CTS_DEVICE_FW_REG_GESTURE_INFO, gesture_info, 2);
    if(ret) {
        TPD_INFO("<E> Read gesture info header failed %d\n", ret);
        return ret;
    }

    if (trace_point) {
        ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_GESTURE_INFO + 2,
            gesture_info + 2,
            (((u8 *)gesture_info))[1] * sizeof(struct cts_device_gesture_point));
        if(ret) {
            TPD_INFO("<E> Read gesture trace points failed %d\n", ret);
            return ret;
        }
    }

    return 0;
}
#endif /* CFG_CTS_GESTURE */

#ifdef CONFIG_CTS_GLOVE
int cts_enter_glove_mode(const struct cts_device *cts_dev)
{
    TPD_INFO("<I> Enter glove mode\n");

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GLOVE_MODE, 1);
    if (ret) {
        TPD_INFO("<E> Enable Glove mode err\n");
    } else {
        cts_dev->rtdata.glove_mode_enabled = true;
    }
    return ret;
}

int cts_exit_glove_mode(const struct cts_device *cts_dev)
{
    TPD_INFO("<I> Exit glove mode\n");

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GLOVE_MODE, 0);
    if (ret) {
        TPD_INFO("<E> Exit Glove mode err\n");
    }
    else {
        cts_dev->rtdata.glove_mode_enabled = false;
    }
    return ret;
}

int cts_is_glove_enabled(const struct cts_device *cts_dev)
{
    return cts_dev->rtdata.glove_mode_enabled;
}
#endif /* CONFIG_CTS_GLOVE */


int cts_set_dev_charger_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;

    ret = cts_send_command(cts_dev,
        attached ? CTS_CMD_CHARGER_ATTACHED : CTS_CMD_CHARGER_DETACHED);
    if (ret) {
        TPD_INFO("<E> Send CMD_CHARGER_%s failed %d\n",
            attached ? "ATTACHED" : "DETACHED", ret);
    }

    return ret;
}

int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached)
{
    int ret;

    ret = cts_send_command(cts_dev,
        attached ? CTS_CMD_EARJACK_ATTACHED : CTS_CMD_EARJACK_DETACHED);
    if (ret) {
        TPD_INFO("<E> Send CMD_EARJACK_%s failed %d\n",
            attached ? "ATTACHED" : "DETACHED", ret);
    }

    return ret;
}

int cts_enable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    TPD_INFO("<I> Fw log redirect enable\n");
    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_FW_LOG_REDIRECT);
    if (ret) {
        TPD_INFO("<E> Send CTS_CMD_ENABLE_FW_LOG_REDIRECT failed %d\n", ret);
    } else {
        cts_dev->rtdata.fw_log_redirect_enabled = true;
    }
    return ret;
}

int cts_disable_fw_log_redirect(struct cts_device *cts_dev)
{
    int ret;

    TPD_INFO("<I> Fw log redirect disable\n");
    ret = cts_send_command(cts_dev, CTS_CMD_DISABLE_FW_LOG_REDIRECT);
    if (ret) {
        TPD_INFO("<E> Send CTS_CMD_DISABLE_FW_LOG_REDIRECT failed %d\n", ret);
    } else {
        cts_dev->rtdata.fw_log_redirect_enabled = false;
    }
    return ret;
}

bool cts_is_fw_log_redirect(struct cts_device *cts_dev)
{
    return cts_dev->rtdata.fw_log_redirect_enabled;
}

int cts_fw_log_show_finish(struct cts_device *cts_dev)
{
    int ret;

    ret = cts_send_command(cts_dev, CTS_CMD_FW_LOG_SHOW_FINISH);
    if (ret) {
        TPD_INFO("<E> Send CTS_CMD_FW_LOG_SHOW_FINISH failed %d\n", ret);
    }

    return ret;
}

static void flip_comp_cap_on_row(void *cap, u8 rows, u8 cols)
{
    u8 r, c;
    u8 *data;

    data = (u8 *)cap;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols / 2; c++) {
            swap(data[r * cols + c],
                 data[r * cols + wrap(cols, c)]);
        }
    }
}

static void flip_comp_cap_on_col(void *cap, u8 rows, u8 cols)
{
    u8 r, c;
    u8 *data;

    data = (u8 *)cap;
    for (r = 0; r < rows / 2; r++) {
        for (c = 0; c < cols; c++) {
            swap(data[r * cols + c],
                 data[wrap(rows, r) * cols + c]);
        }
    }
}

int cts_get_compensate_cap(struct cts_device *cts_dev, u8 *cap)
{
    int i, ret;
    u8  auto_calib_comp_cap_enabled;

    if (cts_dev == NULL || cap == NULL) {
        TPD_INFO("<E> Get compensate cap with cts_dev(%p) or cap(%p) = NULL\n",
            cts_dev, cap);
        return -EINVAL;
    }

    if (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911 &&
        cts_dev->fwdata.lib_version < 0x0500) {
        TPD_INFO("<E> ICNL9911 lib version 0x%04x < v5.0 "
                "NOT supported get compensate cap\n",
                cts_dev->fwdata.lib_version);
        return -ENOTSUPP;
    }

    TPD_INFO("<I> Get compensate cap\n");

    /* Check whether auto calibrate compensate cap enabled */
    TPD_INFO("<I>  - Get auto calib comp cap enable\n");
    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_ENABLE,
        &auto_calib_comp_cap_enabled);
    if (ret) {
        TPD_INFO("<E> Get auto calib comp cap enable failed %d\n", ret);
        return ret;
    }

    /* Wait auto calibrate compensate cap done if enabled */
    if (auto_calib_comp_cap_enabled) {
        u8 auto_calib_comp_cap_done;

        TPD_INFO("<I>  - Wait auto calib comp cap done...\n");

        i = 0;
        do {
            ret = cts_fw_reg_readb(cts_dev,
                CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_DONE,
                &auto_calib_comp_cap_done);
            if (ret) {
                TPD_INFO("<E> Get auto calib comp cap done failed %d\n", ret);
            } else {
                if (auto_calib_comp_cap_done) {
                    goto enable_read_compensate_cap;
                }
            }

            mdelay(5);
        } while (++i < 100);

        TPD_INFO("<E> Wait auto calib comp cap done timeout\n");
        return -EFAULT;
    }

enable_read_compensate_cap:
    TPD_INFO("<I>  - Enable read comp cap\n");
    ret = cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_CNEG);
    if (ret) {
        TPD_INFO("<E> Enable read comp cap failed %d\n",ret);
        return ret;
    }

    /* Wait compensate cap ready */
    TPD_INFO("<I>  - Wait comp cap ready...\n");
    i = 0;
    do {
        u8 ready;

        mdelay(5);

        ret = cts_fw_reg_readb(cts_dev,
            CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY, &ready);
        if (ret) {
            TPD_INFO("<E> Read comp cap ready failed %d\n", ret);
        } else {
            if (ready) {
                goto read_compensate_cap;
            }
        }
    } while (++i < 100);

    TPD_INFO("<E> Wait comp cap ready timeout\n");
    ret = -ETIMEDOUT;
    goto disable_read_comp_cap;

read_compensate_cap:
    /* Use hardware row & col here */
    TPD_INFO("<I>  - Read comp cap\n");
    ret = cts_fw_reg_readsb_delay_idle(cts_dev,
        CTS_DEVICE_FW_REG_COMPENSATE_CAP, cap,
        cts_dev->hwdata->num_row * cts_dev->hwdata->num_col,
        500);
    if (ret) {
        TPD_INFO("<E> Read comp cap failed %d\n",ret);
        goto disable_read_comp_cap;
    }

    if (cts_dev->fwdata.flip_x) {
        TPD_INFO("<I>  - Flip comp cap on row\n");
        flip_comp_cap_on_row(cap,
            cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }
    if (cts_dev->fwdata.flip_y) {
        TPD_INFO("<I>  - Flip comp cap on col\n");
        flip_comp_cap_on_col(cap,
            cts_dev->fwdata.rows, cts_dev->fwdata.cols);
    }

disable_read_comp_cap:
    TPD_INFO("<I>  - Disable read comp cap\n");
    i = 0;
    do {
        int r;
        u8  ready;

        r = cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_CNEG);
        if (r) {
            TPD_INFO("<E> Send cmd DISABLE_READ_CNEG failed %d\n", r);
            continue;
        }

        mdelay(5);
        r = cts_fw_reg_readb(cts_dev,
                CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY, &ready);
        if (r) {
            TPD_INFO("<E> Re-Check comp cap ready failed %d\n", r);
            continue;
        }

        if (ready) {
            TPD_INFO("<W> Comp cap ready is still set\n");
            continue;
        } else {
            return ret;
        }
    } while (++i < 100);

    TPD_INFO("<W> Disable read comp cap failed, try to do reset!\n");

    /* Try to do hardware reset */
    cts_plat_reset_device(cts_dev->pdata);

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        int r = cts_enter_glove_mode(cts_dev);
        if (r) {
            TPD_INFO("<E> Enter dev glove mode failed %d\n", r);
        }
    }
#endif /* CONFIG_CTS_GLOVE */

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        int r = cts_enable_fw_log_redirect(cts_dev);
        if (r) {
            TPD_INFO("<E> Enable fw log redirect failed %d\n", r);
        }
    }
#endif /* CONFIG_CTS_GLOVE */

    return ret;
}

struct chipone_ts_data *cts_dev_get_drvdata(struct device *dev)
{
    struct touchpanel_data * ts = dev_get_drvdata(dev);
    return (struct chipone_ts_data *)ts->chip_data;
}
