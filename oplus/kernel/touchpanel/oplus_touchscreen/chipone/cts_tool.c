#define LOG_TAG         "Tool"

#include "cts_config.h"
#include "cts_core.h"
#include "cts_test.h"

#ifdef CONFIG_CTS_LEGACY_TOOL
#pragma pack(1)
/** Tool command structure */
struct cts_tool_cmd {
    u8 cmd;
    u8 flag;
    u8 circle;
    u8 times;
    u8 retry;
    u32 data_len;
    u8 addr_len;
    u8 addr[2];
    u8 data[PAGE_SIZE];

};
#pragma pack()

#define CTS_TOOL_CMD_HEADER_LENGTH            (12)

enum cts_tool_cmd_code {
    CTS_TOOL_CMD_GET_PANEL_PARAM = 0,
    CTS_TOOL_CMD_GET_DOWNLOAD_STATUS = 2,
    CTS_TOOL_CMD_GET_RAW_DATA = 4,
    CTS_TOOL_CMD_GET_DIFF_DATA = 6,
    CTS_TOOL_CMD_READ_HOSTCOMM = 12,
    CTS_TOOL_CMD_READ_ADC_STATUS = 14,
    CTS_TOOL_CMD_READ_GESTURE_INFO = 16,
    CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE = 18,
    CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE = 20,
    CTS_TOOL_CMD_READ_ICTYPE = 22,
    CTS_TOOL_CMD_I2C_DIRECT_READ = 24,
    CTS_TOOL_CMD_GET_DRIVER_INFO = 26,

    CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM = 1,
    CTS_TOOL_CMD_WRITE_HOSTCOMM = 11,
    CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE = 15,
    CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE = 17,
    CTS_TOOL_CMD_I2C_DIRECT_WRITE = 19,

};

struct cts_test_ioctl_data {
    __u32 ntests;
    struct cts_test_param __user *tests;
};

#define CTS_IOCTL_RDWR_REG_FLAG_RD          (0x0001)
// TODO: Flags can specify DDI level 1/2/3, read/write flag

struct cts_rdwr_reg {
    __u32 addr;
    __u32 flags;
    __u8  __user *data;
    __u32 len;
    __u32 delay_ms;
};

#define CTS_IOCTL_RDWR_REG_TYPE_FW          (1)
#define CTS_IOCTL_RDWR_REG_TYPE_HW          (2)
#define CTS_IOCTL_RDWR_REG_TYPE_DDI         (3)

#define CTS_RDWR_REG_IOCTL_MAX_REGS         (128)

struct cts_rdwr_reg_ioctl_data {
    __u8  reg_type;
    __u32 nregs;
    struct cts_rdwr_reg __user *regs;
};


#define CTS_TOOL_IOCTL_GET_DRIVER_VERSION   _IOR('C', 0x00, unsigned int *)
#define CTS_TOOL_IOCTL_GET_DEVICE_TYPE      _IOR('C', 0x01, unsigned int *)
#define CTS_TOOL_IOCTL_GET_FW_VERSION       _IOR('C', 0x02, unsigned short *)
#define CTS_TOOL_IOCTL_GET_RESOLUTION       _IOR('C', 0x03, unsigned int *) /* X in LSW, Y in MSW */
#define CTS_TOOL_IOCTL_GET_ROW_COL          _IOR('C', 0x04, unsigned int *) /* row in LSW, col in MSW */

#define CTS_TOOL_IOCTL_TEST                 _IOWR('C', 0x10, struct cts_test_ioctl_data *)
#define CTS_TOOL_IOCTL_RDWR_REG             _IOWR('C', 0x20, struct cts_rdwr_reg_ioctl_data *)

#define CTS_DRIVER_VERSION_CODE \
    ((CFG_CTS_DRIVER_MAJOR_VERSION << 16) | \
     (CFG_CTS_DRIVER_MINOR_VERSION << 8) | \
     (CFG_CTS_DRIVER_PATCH_VERSION << 0))

static struct cts_tool_cmd cts_tool_cmd;

#ifdef CONFIG_CTS_I2C_HOST
/* If CFG_CTS_MAX_I2C_XFER_SIZE < 58(PC tool length), this is neccessary */
static u32 cts_tool_direct_access_addr = 0;
#endif /* CONFIG_CTS_I2C_HOST */

static int cts_tool_open(struct inode *inode, struct file *file)
{
    file->private_data = PDE_DATA(inode);
    return 0;
}

static ssize_t cts_tool_read(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
    struct chipone_ts_data *cts_data;
    struct cts_tool_cmd *cmd;
    struct cts_device *cts_dev;
    int ret = 0;

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        TPD_INFO("<E> Read with private_data = NULL\n");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_GET_PANEL_PARAM:
        TPD_INFO("<I> Get panel param len: %u\n", cmd->data_len);
        ret = cts_get_panel_param(cts_dev, cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Get panel param len: %u failed %d\n", cmd->data_len, ret);
        }
        break;

     case CTS_TOOL_CMD_GET_DOWNLOAD_STATUS:
        cmd->data[0] = 100;
        TPD_INFO("<I> Get update status = %hhu\n", cmd->data[0]);
        break;

    case CTS_TOOL_CMD_GET_RAW_DATA:
    case CTS_TOOL_CMD_GET_DIFF_DATA:
        TPD_DEBUG("<D> Get %s data row: %u col: %u len: %u\n",
            cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA ? "raw" : "diff",
            cmd->addr[1], cmd->addr[0], cmd->data_len);

        ret = cts_enable_get_rawdata(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enable read raw/diff data failed %d\n", ret);
            break;
        }
        mdelay(1);

        if (cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA) {
            ret = cts_get_rawdata(cts_dev, cmd->data);
        } else if (cmd->cmd == CTS_TOOL_CMD_GET_DIFF_DATA) {
            ret = cts_get_diffdata(cts_dev, cmd->data);
        }
        if(ret) {
            TPD_INFO("<E> Get %s data failed %d\n",
                cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA ? "raw" : "diff", ret);
            break;
        }

        ret = cts_disable_get_rawdata(cts_dev);
        if (ret) {
            TPD_INFO("<E> Disable read raw/diff data failed %d\n", ret);
            break;
        }

        break;

    case CTS_TOOL_CMD_READ_HOSTCOMM:
        ret = cts_fw_reg_readb(cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data);
        if (ret) {
            TPD_INFO("<E> Read firmware reg addr 0x%04x failed %d\n",
                get_unaligned_le16(cmd->addr), ret);
        } else {
            TPD_DEBUG("<D> Read firmware reg addr 0x%04x, val=0x%02x\n",
                get_unaligned_le16(cmd->addr), cmd->data[0]);
        }
        break;

#ifdef CFG_CTS_GESTURE
    case CTS_TOOL_CMD_READ_GESTURE_INFO:
        ret = cts_get_gesture_info(cts_dev, cmd->data, true);
        if (ret) {
            TPD_INFO("<E> Get gesture info failed %d\n", ret);
        }
        break;
#endif

    case CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE:
        cmd->data_len = min((size_t)cmd->data_len, sizeof(cmd->data));
        ret = cts_fw_reg_readsb(cts_dev, get_unaligned_le16(cmd->addr),
                cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Read firmware reg addr 0x%04x len %u failed %d\n",
                get_unaligned_le16(cmd->addr), cmd->data_len, ret);
        } else {
            TPD_DEBUG("<D> Read firmware reg addr 0x%04x len %u\n",
                get_unaligned_le16(cmd->addr), cmd->data_len);
        }
        break;

    case CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE:
        TPD_DEBUG("<D> Read under program mode addr 0x%06x len %u\n",
            (cmd->flag << 16) | get_unaligned_le16(cmd->addr),
            cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enter program mode failed %d\n", ret);
            break;
        }

        ret = cts_sram_readsb(&cts_data->cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Read under program mode I2C xfer failed %d\n", ret);
            //break;
        }

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enter normal mode failed %d\n", ret);
            break;
        }
        break;

    case CTS_TOOL_CMD_READ_ICTYPE:
        TPD_INFO("<I> Get IC type\n");
        if (cts_dev->hwdata) {
            switch(cts_dev->hwdata->hwid) {
            case CTS_DEV_HWID_ICNL9911: cmd->data[0] = 0x91; break;
            case CTS_DEV_HWID_ICNL9911S: cmd->data[0] = 0x91; break;
            case CTS_DEV_HWID_ICNL9911C: cmd->data[0] = 0x91; break;
            default:  cmd->data[0] = 0x00; break;
            }
        } else {
            cmd->data[0] = 0x10;
        }
        break;

#ifdef CONFIG_CTS_I2C_HOST
    case CTS_TOOL_CMD_I2C_DIRECT_READ:
        {
            u32 addr_width;
            char *wr_buff = NULL;
            u8 addr_buff[4];
            size_t left_size, max_xfer_size;
            u8 *data;

            if (cmd->addr[0] != CTS_DEV_PROGRAM_MODE_I2CADDR) {
                cmd->addr[0] = CTS_DEV_NORMAL_MODE_I2CADDR;
                addr_width = 2;
            } else {
                addr_width = cts_dev->hwdata->program_addr_width;
            }

            TPD_DEBUG("<D> Direct read from i2c_addr 0x%02x addr 0x%06x size %u\n",
                cmd->addr[0], cts_tool_direct_access_addr, cmd->data_len);

            left_size = cmd->data_len;
            max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
            data = cmd->data;
            while (left_size) {
                size_t xfer_size = min(left_size, max_xfer_size);
                ret = cts_plat_i2c_read(cts_data->pdata, cmd->addr[0],
                        wr_buff, addr_width, data, xfer_size, 1, 0);
                if (ret) {
                    TPD_INFO("<E> Direct read i2c_addr 0x%02x addr 0x%06x len %zu failed %d\n",
                        cmd->addr[0], cts_tool_direct_access_addr, xfer_size, ret);
                    break;
                }

                left_size -= xfer_size;
                if (left_size) {
                    data += xfer_size;
                    cts_tool_direct_access_addr += xfer_size;
                    if (addr_width == 2) {
                        put_unaligned_be16(cts_tool_direct_access_addr, addr_buff);
                    } else if (addr_width == 3) {
                        put_unaligned_be24(cts_tool_direct_access_addr, addr_buff);
                    }
                    wr_buff = addr_buff;
                }
            }
        }
        break;
#endif
    case CTS_TOOL_CMD_GET_DRIVER_INFO:
        break;

    default:
        TPD_INFO("<W> Read unknown command %u\n", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    if (ret == 0) {
        if(cmd->cmd == CTS_TOOL_CMD_I2C_DIRECT_READ) {
            ret = copy_to_user(buffer + CTS_TOOL_CMD_HEADER_LENGTH,
                                cmd->data, cmd->data_len);
        } else {
            ret = copy_to_user(buffer, cmd->data, cmd->data_len);
        }
        if (ret) {
            TPD_INFO("<E> Copy data to user buffer failed %d\n", ret);
            return 0;
        }

        return cmd->data_len;
    }

    return 0;
}

static ssize_t cts_tool_write(struct file *file,
        const char __user * buffer, size_t count, loff_t * ppos)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_tool_cmd *cmd;
    int ret = 0;

    if (count < CTS_TOOL_CMD_HEADER_LENGTH || count > PAGE_SIZE) {
        TPD_INFO("<E> Write len %zu invalid\n", count);
        return -EFAULT;
    }

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        TPD_INFO("<E> Write with private_data = NULL\n");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    ret = copy_from_user(cmd, buffer, CTS_TOOL_CMD_HEADER_LENGTH);
    if (ret) {
        TPD_INFO("<E> Copy command header from user buffer failed %d\n", ret);
        return -EIO;
    } else {
        ret = CTS_TOOL_CMD_HEADER_LENGTH;
    }

    if (cmd->data_len > PAGE_SIZE) {
        TPD_INFO("<E> Write with invalid count %d\n", cmd->data_len);
        return -EIO;
    }

    if(cmd->cmd & BIT(0)) {
        if(cmd->data_len) {
            ret = copy_from_user(cmd->data,
                    buffer + CTS_TOOL_CMD_HEADER_LENGTH, cmd->data_len);
            if (ret) {
                TPD_INFO("<E> Copy command payload from user buffer len %u failed %d\n",
                    cmd->data_len, ret);
                return -EIO;
            }
        }
    } else {
        /* Prepare for read command */
        TPD_DEBUG("<D> Write read command(%d) header, prepare read size: %d\n",
            cmd->cmd, cmd->data_len);
        return CTS_TOOL_CMD_HEADER_LENGTH + cmd->data_len;
    }

    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM:
        TPD_INFO("<I> Write panel param len %u data\n", cmd->data_len);
        ret = cts_fw_reg_writesb(cts_dev, CTS_DEVICE_FW_REG_PANEL_PARAM,
                cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Write panel param failed %d\n", ret);
            break;
        }

        ret = cts_send_command(cts_dev, CTS_CMD_RESET);
        if (ret) {

        }

        ret = cts_set_work_mode(cts_dev, 1);
        if (ret) {

        }

        mdelay(100);

        ret = cts_set_work_mode(cts_dev, 0);
        if (ret) {

        }
        mdelay(100);

        break;

    case CTS_TOOL_CMD_WRITE_HOSTCOMM:
        TPD_DEBUG("<D> Write firmware reg addr: 0x%04x val=0x%02x\n",
            get_unaligned_le16(cmd->addr), cmd->data[0]);

        ret = cts_fw_reg_writeb(cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data[0]);
        if (ret) {
            TPD_INFO("<E> Write firmware reg addr: 0x%04x val=0x%02x failed %d\n",
                get_unaligned_le16(cmd->addr), cmd->data[0], ret);
        }
        break;

    case CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE:
        TPD_DEBUG("<D> Write firmare reg addr: 0x%04x len %u\n",
            get_unaligned_le16(cmd->addr), cmd->data_len);
        ret = cts_fw_reg_writesb(cts_dev, get_unaligned_le16(cmd->addr),
                cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Write firmare reg addr: 0x%04x len %u failed %d\n",
                get_unaligned_le16(cmd->addr), cmd->data_len, ret);
        }
        break;

    case CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE:
        TPD_DEBUG("<D> Write to addr 0x%06x size %u under program mode\n",
            (cmd->flag << 16) | (cmd->addr[1] << 8) | cmd->addr[0],
            cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enter program mode failed %d\n", ret);
            break;
        }

        ret = cts_sram_writesb(cts_dev,
                (cmd->flag << 16) | (cmd->addr[1] << 8) | cmd->addr[0],
                cmd->data, cmd->data_len);
        if (ret) {
            TPD_INFO("<E> Write program mode multibyte failed %d\n", ret);
            //break;
        }

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            TPD_INFO("<E> Enter normal mode failed %d\n", ret);
            break;
        }

        break;

#ifdef CONFIG_CTS_I2C_HOST
    case CTS_TOOL_CMD_I2C_DIRECT_WRITE:
        {
            u32 addr_width;
            size_t left_payload_size;       /* Payload exclude address field */
            size_t max_xfer_size;
            char *payload;

            if (cmd->addr[0] != CTS_DEV_PROGRAM_MODE_I2CADDR) {
                cmd->addr[0] = CTS_DEV_NORMAL_MODE_I2CADDR;
                addr_width = 2;
                cts_tool_direct_access_addr = get_unaligned_be16(cmd->data);
            } else {
                addr_width = cts_dev->hwdata->program_addr_width;
                cts_tool_direct_access_addr = get_unaligned_be24(cmd->data);
            }

            if (cmd->data_len < addr_width) {
                TPD_INFO("<E> Direct write too short %d < address width %d\n",
                    cmd->data_len, addr_width);
                ret = -EINVAL;
                break;
            }

            TPD_DEBUG("<D> Direct write to i2c_addr 0x%02x addr 0x%06x size %u\n",
                cmd->addr[0], cts_tool_direct_access_addr, cmd->data_len);

            left_payload_size = cmd->data_len - addr_width;
            max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
            payload = cmd->data + addr_width;
            do {
                size_t xfer_payload_size = min(left_payload_size,
                        max_xfer_size - addr_width);
                size_t xfer_len = xfer_payload_size + addr_width;

                ret = cts_plat_i2c_write(cts_data->pdata, cmd->addr[0],
                        payload - addr_width, xfer_len, 1, 0);
                if (ret) {
                    TPD_INFO("<E> Direct write i2c_addr 0x%02x addr 0x%06x len %zu failed %d\n",
                        cmd->addr[0], cts_tool_direct_access_addr, xfer_len, ret);
                    break;
                }

                left_payload_size -= xfer_payload_size;
                if (left_payload_size) {
                    payload += xfer_payload_size;
                    cts_tool_direct_access_addr += xfer_payload_size;
                    if (addr_width == 2) {
                        put_unaligned_be16(cts_tool_direct_access_addr, payload - addr_width);
                    } else if (addr_width == 3) {
                        put_unaligned_be24(cts_tool_direct_access_addr, payload - addr_width);
                    }
                }
            } while (left_payload_size);
        }
        break;
#endif
    default:
        TPD_INFO("<W> Write unknown command %u\n", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    return ret ? 0 : cmd->data_len + CTS_TOOL_CMD_HEADER_LENGTH;
}

static int cts_ioctl_test(struct cts_device *cts_dev,
    u32 ntests, struct cts_test_param *tests)
{
    u32 num_nodes = 0;
    int i, ret = 0;

    TPD_INFO("<I> ioctl test total %u items\n", ntests);

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;

    for (i = 0; i < ntests; i++) {
        bool validate_data = false;
        bool validate_data_per_node = false;
        bool validate_min = false;
        bool validate_max = false;
        bool skip_invalid_node = false;
        bool stop_test_if_validate_fail = false;
        bool dump_test_data_to_console = false;
        bool dump_test_data_to_user = false;
        bool dump_test_data_to_file = false;
        bool dump_test_data_to_file_append = false;
        u32 __user *user_min_threshold = NULL;
        u32 __user *user_max_threshold = NULL;
        u32 __user *user_invalid_nodes = NULL;
        int  __user *user_test_result = NULL;
        void __user *user_test_data = NULL;
        int  __user *user_test_data_wr_size = NULL;
        const char __user *user_test_data_filepath = NULL;
        void __user *user_priv_param = NULL;
        int test_result = 0;
        int test_data_wr_size = 0;

        TPD_INFO("<I> ioctl test item %d: %d(%s) flags: %08x priv param size: %d\n",
            i, tests[i].test_item, cts_test_item_str(tests[i].test_item),
            tests[i].flags, tests[i].priv_param_size);
        /*
         * Validate arguement
         */
        validate_data =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_DATA);
        validate_data_per_node =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
        validate_min =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_MIN);
        validate_max =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_MAX);
        skip_invalid_node =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_SKIP_INVALID_NODE);
        stop_test_if_validate_fail =
            !!(tests[i].flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);
        dump_test_data_to_user =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
        dump_test_data_to_file =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
        dump_test_data_to_file_append =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND);

        if (tests[i].test_result == NULL) {
            TPD_INFO("<E> Result pointer = NULL\n");
            return -EFAULT;
        }

        if (validate_data) {
            TPD_INFO("<I>   - Flag: Validate data\n");

            if (validate_data_per_node) {
                TPD_INFO("<I>   - Flag: Validate data per-node\n");
            }
            if (validate_min) {
                TPD_INFO("<I>   - Flag: Validate min threshold\n");
            }
            if (validate_max) {
                TPD_INFO("<I>   - Flag: Validate max threshold\n");
            }
            if (stop_test_if_validate_fail) {
                TPD_INFO("<I>   - Flag: Stop test if validate fail\n");
            }

            if (validate_min && tests[i].min == NULL) {
                TPD_INFO("<E> Min threshold pointer = NULL\n");
                ret = -EINVAL;
                goto store_result;
            }
            if (validate_max && tests[i].max == NULL) {
                TPD_INFO("<E> Max threshold pointer = NULL\n");
                ret = -EINVAL;
                goto store_result;
            }
            if (skip_invalid_node) {
                TPD_INFO("<I>   - Flag: Skip invalid node\n");

                if (tests[i].num_invalid_node == 0 ||
                    tests[i].num_invalid_node >= num_nodes) {
                    TPD_INFO("<E> Num invalid node %u out of range[0, %u]\n",
                        tests[i].num_invalid_node, num_nodes);
                    ret = -EINVAL;
                    goto store_result;
                }

                if (tests[i].invalid_nodes == NULL) {
                    TPD_INFO("<E> Invalid nodes pointer = NULL\n");
                    ret = -EINVAL;
                    goto store_result;
                }
            }
        }

        if (dump_test_data_to_console) {
            TPD_INFO("<I>   - Flag: Dump test data to console\n");
        }

        if (dump_test_data_to_user) {
            TPD_INFO("<I>   - Flag: Dump test data to user, size: %d\n",
                tests[i].test_data_buf_size);

            if (tests[i].test_data_buf == NULL) {
                TPD_INFO("<E> Test data pointer = NULL\n");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].test_data_wr_size == NULL) {
                TPD_INFO("<E> Test data write size pointer = NULL\n");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].test_data_buf_size < num_nodes) {
                TPD_INFO("<E> Test data size %d too small < %u\n",
                    tests[i].test_data_buf_size, num_nodes);
                ret = -EINVAL;
                goto store_result;
            }
        }

        if (dump_test_data_to_file) {
            TPD_INFO("<I>   - Flag: Dump test data to file%s\n",
                dump_test_data_to_file_append ? "[Append]" : "");

            if (tests[i].test_data_filepath == NULL) {
                TPD_INFO("<E> Test data filepath = NULL\n");
                ret = -EINVAL;
                goto store_result;
            }
        }

        /*
         * Dump input parameter from user,
         * Aallocate memory for output,
         * Replace __user pointer with kernel pointer.
         */
        user_test_result = (int __user *)tests[i].test_result;
        tests[i].test_result = &test_result;

        if (validate_data) {
            int num_threshold = validate_data_per_node ? num_nodes : 1;
            int threshold_size = num_threshold * sizeof(tests[i].min[0]);

            if (validate_min) {
                user_min_threshold = (int __user *)tests[i].min;
                tests[i].min = memdup_user(user_min_threshold, threshold_size);
                if (IS_ERR(tests[i].min)) {
                    ret = PTR_ERR(tests[i].min);
                    tests[i].min = NULL;
                    TPD_INFO("<E> Memdup min threshold from user failed %d\n", ret);
                    goto store_result;
                }
            } else {
                tests[i].min = NULL;
            }
            if (validate_max) {
                user_max_threshold = (int __user *)tests[i].max;
                tests[i].max = memdup_user(user_max_threshold, threshold_size);
                if (IS_ERR(tests[i].max)) {
                    ret = PTR_ERR(tests[i].max);
                    tests[i].max = NULL;
                    TPD_INFO("<E> Memdup max threshold from user failed %d\n", ret);
                    goto store_result;
                }
            } else {
                tests[i].max = NULL;
            }
            if (skip_invalid_node) {
                user_invalid_nodes = (u32 __user *)tests[i].invalid_nodes;
                tests[i].invalid_nodes = memdup_user(user_invalid_nodes,
                    tests[i].num_invalid_node * sizeof(tests[i].invalid_nodes[0]));
                if (IS_ERR(tests[i].invalid_nodes)) {
                    ret = PTR_ERR(tests[i].invalid_nodes);
                    tests[i].invalid_nodes = NULL;
                    TPD_INFO("<E> Memdup invalid node from user failed %d\n", ret);
                    goto store_result;
                }
            }
        }

        if (dump_test_data_to_user) {
            user_test_data = (void __user *)tests[i].test_data_buf;
            tests[i].test_data_buf = kmalloc(tests[i].test_data_buf_size, GFP_KERNEL);
            if (tests[i].test_data_buf == NULL) {
                ret = -ENOMEM;
                TPD_INFO("<E> Alloc test data mem failed\n");
                goto store_result;
            }
            user_test_data_wr_size = (int __user *)tests[i].test_data_wr_size;
            tests[i].test_data_wr_size = &test_data_wr_size;
        }

        if (dump_test_data_to_file) {
            user_test_data_filepath = (const char __user *)tests[i].test_data_filepath;
            tests[i].test_data_filepath = strndup_user(user_test_data_filepath, PATH_MAX);
            if (tests[i].test_data_filepath == NULL) {
                TPD_INFO("<E> Strdup test data filepath failed\n");
                goto store_result;
            }
        }

        if (tests[i].priv_param_size && tests[i].priv_param) {
            user_priv_param = (void __user *)tests[i].priv_param;
            tests[i].priv_param = memdup_user(user_priv_param, tests[i].priv_param_size);
            if (IS_ERR(tests[i].priv_param)) {
                ret = PTR_ERR(tests[i].priv_param);
                tests[i].priv_param = NULL;
                TPD_INFO("<E> Memdup priv param from user failed %d\n", ret);
                goto store_result;
            }
        }

        /*
        * Do test
        */
        switch (tests[i].test_item) {
        case CTS_TEST_RESET_PIN:
            ret = cts_test_reset_pin(cts_dev, &tests[i]);
            break;
        case CTS_TEST_INT_PIN:
            ret = cts_test_int_pin(cts_dev, &tests[i]);
            break;
        case CTS_TEST_RAWDATA:
            ret = cts_test_rawdata(cts_dev, &tests[i]);
            break;
        case CTS_TEST_NOISE:
            ret = cts_test_noise(cts_dev, &tests[i]);
            break;
        case CTS_TEST_OPEN:
            ret = cts_test_open(cts_dev, &tests[i]);
            break;
        case CTS_TEST_SHORT:
            ret = cts_test_short(cts_dev, &tests[i]);
            break;
        case CTS_TEST_COMPENSATE_CAP:
            ret = cts_test_compensate_cap(cts_dev, &tests[i]);
            break;
        default:
            ret = ENOTSUPP;
            TPD_INFO("<E> Un-supported test item\n");
            break;
        }

        /*
         * Copy result and test data back to userspace.
         */
store_result:
        if (dump_test_data_to_user) {
            if (user_test_data != NULL && test_data_wr_size > 0) {
                TPD_INFO("<I> Copy test data to user, size: %d\n", test_data_wr_size);
                if (copy_to_user(user_test_data, tests[i].test_data_buf,
                    test_data_wr_size)) {
                    TPD_INFO("<E> Copy test data to user failed\n");
                    test_data_wr_size = 0;
                    // Skip this error
                }
            }

            if (user_test_data_wr_size != NULL) {
                put_user(test_data_wr_size, user_test_data_wr_size);
            }
        }

        if (user_test_result != NULL) {
            put_user(ret, user_test_result);
        } else if (tests[i].test_result != NULL){
            put_user(ret, tests[i].test_result);
        }

        /*
         * Free memory
         */
        if (dump_test_data_to_user) {
            if (user_test_data != NULL && tests[i].test_data_buf != NULL) {
                kfree(tests[i].test_data_buf);
            }
        }
        if (validate_data) {
            if (validate_min && user_min_threshold != NULL && tests[i].min != NULL) {
                kfree(tests[i].min);
            }
            if (validate_max && user_max_threshold != NULL && tests[i].max != NULL) {
                kfree(tests[i].max);
            }
            if (skip_invalid_node) {
                if (user_invalid_nodes != NULL && tests[i].invalid_nodes != NULL) {
                    kfree(tests[i].invalid_nodes);
                }
            }
        }

        if (dump_test_data_to_file &&
            user_test_data_filepath != NULL &&
            tests[i].test_data_filepath != NULL) {
            kfree(tests[i].test_data_filepath);
        }

        if (user_priv_param && tests[i].priv_param) {
            kfree(tests[i].priv_param);
        }

        if (ret && stop_test_if_validate_fail) {
            break;
        }
    }

    kfree(tests);

    return ret;
}

static int cts_ioctl_rdwr_reg(struct cts_device *cts_dev,
    u8 reg_type, u32 nregs, struct cts_rdwr_reg *regs)
{
    int i, ret = 0;
    bool fw_esd_protect = false;

    TPD_INFO("<I> ioctl RDWR_REG type: %u total %u regs\n", reg_type, nregs);

    cts_lock_device(cts_dev);

    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI) {
        ret = cts_get_dev_esd_protection(cts_dev, &fw_esd_protect);
        if (ret) {
            TPD_INFO("<E> Get fw esd protection failed %d\n", ret);
            goto unlock_device;
        }

        if (fw_esd_protect) {
            ret = cts_set_dev_esd_protection(cts_dev, false);
            if (ret) {
                TPD_INFO("<E> Set fw esd protection failed %d\n", ret);
                goto unlock_device;
            }
        }

        ret = cts_dev->hwdata->enable_access_ddi_reg(cts_dev, true);
        if (ret) {
            TPD_INFO("<E> Enable access ddi reg failed %d\n", ret);
            goto recovery_fw_esd_protect;
        }
    }

    for (i = 0; i < nregs; i++) {
        struct cts_rdwr_reg *reg = regs + i;
        u8 *data = NULL;

        TPD_DEBUG("<D>   reg: %p flags: 0x%x data: %p len: %u delay: %u\n",
            reg, reg->flags, reg->data, reg->len, reg->delay_ms);

        if (reg->data == NULL || reg->len == 0) {
            TPD_INFO("<E> Rdwr reg(addr: 0x%06x) with data: %p or len: %u\n",
                reg->addr, reg->data, reg->len);
            ret = -EINVAL;
            goto disable_access_ddi_reg;
        }

        if (reg->flags & CTS_IOCTL_RDWR_REG_FLAG_RD) {
            u8 __user *user_data = reg->data;

            data = kmalloc(reg->len, GFP_KERNEL);
            if (data == NULL) {
                TPD_INFO("<E> Alloc mem for read reg(addr: 0x%06x len: %u) data failed\n",
                    reg->addr, reg->len);
                ret = -ENOMEM;
                goto disable_access_ddi_reg;
            }
            if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_FW) {
                ret = cts_fw_reg_readsb(cts_dev,
                    regs->addr, data, reg->len);
            } else {
                ret = cts_hw_reg_readsb(cts_dev,
                    regs->addr, data, reg->len);
            }
            if (ret) {
                kfree(data);
                TPD_INFO("<E> Read reg from addr: 0x%06x len: %u failed %d\n",
                    reg->addr, reg->len, ret);
                goto disable_access_ddi_reg;
            }
            if (copy_to_user(user_data, data, reg->len)) {
                kfree(data);
                TPD_INFO("<E> Copy reg(addr: 0x%06x len: %u) data to user failed\n",
                    reg->addr, reg->len);
            }
            kfree(data);
        } else {
            data = memdup_user(reg->data, reg->len);
            if (IS_ERR(data)) {
                ret = PTR_ERR(data);
                TPD_INFO("<E> Memdup reg(addr: 0x%06x len: %u) data from user failed\n",
                    reg->addr, reg->len);
                goto disable_access_ddi_reg;
            }
            if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_FW) {
                ret = cts_fw_reg_writesb(cts_dev,
                    regs->addr, data, reg->len);
            } else {
                ret = cts_hw_reg_writesb(cts_dev,
                    regs->addr, data, reg->len);
            }
            kfree(data);
            if (ret) {
                TPD_INFO("<E> Write reg from addr 0x%06x len %u failed %d\n",
                    reg->addr, reg->len, ret);
                goto disable_access_ddi_reg;
            }
        }

        if (reg->delay_ms) {
            mdelay(reg->delay_ms);
        }
    }

disable_access_ddi_reg:
    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI) {
        int r = cts_dev->hwdata->enable_access_ddi_reg(cts_dev, false);
        if (r) {
            TPD_INFO("<E> Disable access ddi reg failed %d\n", r);
        }
    }

recovery_fw_esd_protect:
    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI && fw_esd_protect) {
        int r = cts_set_dev_esd_protection(cts_dev, true);
        if (r) {
            TPD_INFO("<E> Re-Enable fw esd protection failed %d\n", r);
        }
    }

unlock_device:
    cts_unlock_device(cts_dev);

    kfree(regs);

    return ret;
}


static long cts_tool_ioctl(struct file *file, unsigned int cmd,
        unsigned long arg)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;

    TPD_INFO("<I> ioctl, cmd=0x%08x, arg=0x%08lx\n", cmd, arg);

    cts_data = file->private_data;
    if (cts_data == NULL) {
        TPD_INFO("<E> IOCTL with private data = NULL\n");
        return -EFAULT;
    }

    cts_dev = &cts_data->cts_dev;

    switch (cmd) {
    case CTS_TOOL_IOCTL_GET_DRIVER_VERSION:
        return put_user(CTS_DRIVER_VERSION_CODE,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_DEVICE_TYPE:
        return put_user(cts_dev->hwdata->hwid,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_FW_VERSION:
        return put_user(cts_dev->fwdata.version,
                (unsigned short __user *)arg);
    case CTS_TOOL_IOCTL_GET_RESOLUTION:
        return put_user((cts_dev->fwdata.res_y << 16) + cts_dev->fwdata.res_x,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_ROW_COL:
        return put_user((cts_dev->fwdata.cols << 16) + cts_dev->fwdata.rows,
                (unsigned int __user *)arg);

    case CTS_TOOL_IOCTL_TEST:{
        struct cts_test_ioctl_data test_arg;
        struct cts_test_param *tests_pa;

        if (copy_from_user(&test_arg,
                (struct cts_test_ioctl_data __user *)arg,
                sizeof(test_arg))) {
            TPD_INFO("<E> Copy ioctl test arg to kernel failed\n");
            return -EFAULT;
        }

        if (test_arg.ntests > 8) {
            TPD_INFO("<E> ioctl test with too many tests %u\n",
                test_arg.ntests);
            return -EINVAL;
        }

        tests_pa = memdup_user(test_arg.tests,
            test_arg.ntests * sizeof(struct cts_test_param));
        if (IS_ERR(tests_pa)) {
            int ret = PTR_ERR(tests_pa);
            TPD_INFO("<E> Memdump test param to kernel failed %d\n", ret);
            return ret;
        }

        return cts_ioctl_test(cts_dev, test_arg.ntests, tests_pa);
    }
    case CTS_TOOL_IOCTL_RDWR_REG:{
        struct cts_rdwr_reg_ioctl_data ioctl_data;
        struct cts_rdwr_reg *regs_pa;

        if (copy_from_user(&ioctl_data,
                (struct cts_rdwr_reg_ioctl_data __user *)arg,
                sizeof(ioctl_data))) {
            TPD_INFO("<E> Copy ioctl rdwr_reg arg to kernel failed\n");
            return -EFAULT;
        }

        if (ioctl_data.nregs > CTS_RDWR_REG_IOCTL_MAX_REGS) {
            TPD_INFO("<E> ioctl rdwr_reg with too many regs %u\n",
                ioctl_data.nregs);
            return -EINVAL;
        }

        regs_pa = memdup_user(ioctl_data.regs,
            ioctl_data.nregs * sizeof(struct cts_rdwr_reg));
        if (IS_ERR(regs_pa)) {
            int ret = PTR_ERR(regs_pa);
            TPD_INFO("<E> Memdump cts_rdwr_reg to kernel failed %d\n", ret);
            return ret;
        }

        return cts_ioctl_rdwr_reg(cts_dev,
            ioctl_data.reg_type, ioctl_data.nregs, regs_pa);
    }

    default:
        TPD_INFO("<E> Unsupported ioctl cmd=0x%08x, arg=0x%08lx\n", cmd, arg);
        break;
    }

    return -ENOTSUPP;
}

static struct file_operations cts_tool_fops = {
    .owner = THIS_MODULE,
    .llseek = no_llseek,
    .open  = cts_tool_open,
    .read  = cts_tool_read,
    .write = cts_tool_write,
    .unlocked_ioctl = cts_tool_ioctl,
};

int cts_tool_init(struct chipone_ts_data *cts_data)
{
    TPD_INFO("<I> Init\n");

    cts_data->procfs_entry = proc_create_data(CFG_CTS_TOOL_PROC_FILENAME,
            0666, NULL, &cts_tool_fops, cts_data);
    if (cts_data->procfs_entry == NULL) {
        TPD_INFO("<E> Create proc entry failed\n");
        return -EFAULT;
    }

    return 0;
}

void cts_tool_deinit(struct chipone_ts_data *data)
{
    TPD_INFO("<I> Deinit\n");

    if (data->procfs_entry) {
        remove_proc_entry(CFG_CTS_TOOL_PROC_FILENAME, NULL);
    }
}
#endif /* CONFIG_CTS_LEGACY_TOOL */

