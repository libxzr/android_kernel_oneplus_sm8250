#define LOG_TAG         "Test"

#include "cts_config.h"
#include "cts_core.h"
#include "cts_strerror.h"
#include "cts_test.h"

const char *cts_test_item_str(int test_item)
{
#define case_test_item(item) \
    case CTS_TEST_ ## item: return #item "-TEST"

    switch (test_item) {
        case_test_item(RESET_PIN);
        case_test_item(INT_PIN);
        case_test_item(RAWDATA);
        case_test_item(NOISE);
        case_test_item(OPEN);
        case_test_item(SHORT);
        case_test_item(COMPENSATE_CAP);

        default: return "INVALID";
    }
#undef case_test_item
}

#define CTS_FIRMWARE_WORK_MODE_NORMAL   (0x00)
#define CTS_FIRMWARE_WORK_MODE_FACTORY  (0x01)
#define CTS_FIRMWARE_WORK_MODE_CONFIG   (0x02)
#define CTS_FIRMWARE_WORK_MODE_TEST     (0x03)

#define CTS_TEST_SHORT                  (0x01)
#define CTS_TEST_OPEN                   (0x02)

#define CTS_SHORT_TEST_UNDEFINED        (0x00)
#define CTS_SHORT_TEST_BETWEEN_COLS     (0x01)
#define CTS_SHORT_TEST_BETWEEN_ROWS     (0x02)
#define CTS_SHORT_TEST_BETWEEN_GND      (0x03)

#define TEST_RESULT_BUFFER_SIZE(cts_dev) \
    (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

#define RAWDATA_BUFFER_SIZE(cts_dev) \
        (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

int disable_fw_monitor_mode(struct cts_device *cts_dev)
{
    int ret;
    u8 value;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &value);
    if (ret) {
        return ret;
    }

    if (value & BIT(0)) {
        return cts_fw_reg_writeb(cts_dev,
            CTS_DEVICE_FW_REG_FLAG_BITS, value & (~BIT(0)));
    }

    return 0;
}

static int disable_fw_auto_compensate(struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev,
        CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_ENABLE, 0);
}

int set_fw_work_mode(struct cts_device *cts_dev, u8 mode)
{
    int ret, retries;
    u8  pwr_mode;

    TPD_INFO("<I> Set firmware work mode to %u\n", mode);

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
    if (ret) {
        TPD_INFO("<E> Write firmware work mode register failed %d\n", ret);
        return ret;
    }

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_POWER_MODE,
        &pwr_mode);
    if (ret) {
        TPD_INFO("<E> Read firmware power mode register failed %d\n", ret);
        return ret;
    }

    if (pwr_mode == 1) {
        ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
        if (ret) {
            TPD_INFO("<E> Send cmd QUIT_GESTURE_MONITOR failed %d\n", ret);
            return ret;
        }

        msleep(50);
    }

    retries = 0;
    do {
        u8 sys_busy, curr_mode;

        msleep(10);

        ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_SYS_BUSY,
            &sys_busy);
        if (ret) {
            TPD_INFO("<E> Read firmware system busy register failed %d\n", ret);
            //return ret;
            continue;
        }
        if (sys_busy)
            continue;

        ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_GET_WORK_MODE,
            &curr_mode);
        if (ret) {
            TPD_INFO("<E> Read firmware current work mode failed %d\n", ret);
            //return ret;
            continue;
        }

        TPD_INFO("<E> curr_mode:%d, mode:%d\n", curr_mode, mode);
        if (curr_mode == mode /*|| curr_mode == 0xFF*/) {
            break;
        }
    } while (retries++ < 1000);

    return (retries >= 1000 ? -ETIMEDOUT : 0);
}

static int set_display_state(struct cts_device *cts_dev, bool active)
{
    int ret;
    u8  access_flag;

    TPD_INFO("<I> Set display state to %s\n", active ? "ACTIVE" : "SLEEP");

    ret = cts_hw_reg_readb(cts_dev, 0x3002C, &access_flag);
    if (ret) {
        TPD_INFO("<E> Read display access flag failed %d\n", ret);
        return ret;
    }

    ret = cts_hw_reg_writeb(cts_dev, 0x3002C, access_flag | 0x01);
    if (ret) {
        TPD_INFO("<E> Write display access flag %02x failed %d\n", access_flag, ret);
        return ret;
    }

    if (active) {
        ret = cts_hw_reg_writeb(cts_dev, 0x3C044, 0x55);
        if (ret) {
            TPD_INFO("<E> Write DCS-CMD11 fail\n");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb(cts_dev, 0x3C0A4, 0x55);
        if (ret) {
            TPD_INFO("<E> Write DCS-CMD29 fail\n");
            return ret;
        }

        msleep(100);
    } else {
        ret = cts_hw_reg_writeb(cts_dev, 0x3C0A0, 0x55);
        if (ret) {
            TPD_INFO("<E> Write DCS-CMD28 fail\n");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb(cts_dev, 0x3C040, 0x55);
        if (ret) {
            TPD_INFO("<E> Write DCS-CMD10 fail\n");
            return ret;
        }

        msleep(100);
    }

    ret = cts_hw_reg_writeb(cts_dev, 0x3002C, access_flag);
    if (ret) {
        TPD_INFO("<E> Restore display access flag %02x failed %d\n", access_flag, ret);
        return ret;
    }

    return 0;
}

static int wait_test_complete(struct cts_device *cts_dev, int skip_frames)
{
    int ret, i, j;

    TPD_INFO("<I> Wait test complete skip %d frames\n", skip_frames);

    for (i = 0; i < (skip_frames + 1); i++) {
        u8 ready;

        for (j = 0; j < 1000; j++) {
            mdelay(1);

            ready = 0;
            ret = cts_get_data_ready_flag(cts_dev, &ready);
            if (ret) {
                TPD_INFO("<E> Get data ready flag failed %d\n", ret);
                return ret;
            }

            if (ready) {
                break;
            }
        }

        if (ready == 0) {
            TPD_INFO("<E> Wait test complete timeout\n");
            return -ETIMEDOUT;
        }
        if (i < skip_frames) {
            ret = cts_clr_data_ready_flag(cts_dev);
            if (ret) {
                TPD_INFO("<E> Clr data ready flag failed %d\n", ret);
                return ret;
            }
        }
    }

    return 0;
}

static int get_test_result(struct cts_device *cts_dev, u16 *result)
{
    int ret;

    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA, result,
            TEST_RESULT_BUFFER_SIZE(cts_dev));
    if (ret) {
        TPD_INFO("<E> Get test result data failed %d\n", ret);
        return ret;
    }

    ret = cts_clr_data_ready_flag(cts_dev);
    if (ret) {
        TPD_INFO("<E> Clear data ready flag failed %d\n", ret);
        return ret;
    }

    return 0;
}

static int set_fw_test_type(struct cts_device *cts_dev, u8 type)
{
    int ret, retries = 0;
    u8  sys_busy;
    u8  type_readback;

    TPD_INFO("<I> Set test type %d\n", type);

    ret = cts_fw_reg_writeb(cts_dev, 0x34, type);
    if (ret) {
        TPD_INFO("<E> Write test type register to failed %d\n", ret);
        return ret;
    }

    do {
        msleep(1);

        ret = cts_fw_reg_readb(cts_dev, 0x01, &sys_busy);
        if (ret) {
            TPD_INFO("<E> Read system busy register failed %d\n", ret);
            return ret;
        }
    } while (sys_busy && retries++ < 1000);

    if (retries >= 1000) {
        TPD_INFO("<E> Wait system ready timeout\n");
        return -ETIMEDOUT;
    }

    ret = cts_fw_reg_readb(cts_dev, 0x34, &type_readback);
    if (ret) {
        TPD_INFO("<E> Read test type register failed %d\n", ret);
        return ret;
    }

    if (type != type_readback) {
        TPD_INFO("<E> Set test type %u != readback %u\n", type, type_readback);
        return -EFAULT;
    }

    return 0;
}

static bool set_short_test_type(struct cts_device *cts_dev, u8 type)
{
    static struct fw_short_test_param {
        u8  type;
        u32 col_pattern[2];
        u32 row_pattern[2];
    } param = {
        .type = CTS_SHORT_TEST_BETWEEN_COLS,
        .col_pattern = {0, 0},
        .row_pattern = {0, 0}
    };
    int i, ret;

    TPD_INFO("<I> Set short test type to %u\n", type);

    param.type = type;
    for (i = 0; i < 5; i++) {
        u8 type_readback;

        ret = cts_fw_reg_writesb(cts_dev, 0x5000, &param, sizeof(param));
        if (ret) {
            TPD_INFO("<E> Set short test type to %u failed %d\n", type, ret);
            continue;
        }
        ret = cts_fw_reg_readb(cts_dev, 0x5000, &type_readback);
        if (ret) {
            TPD_INFO("<E> Get short test type failed %d\n", ret);
            continue;
        }
        if (type == type_readback) {
            return 0;
        } else {
            TPD_INFO("<E> Set test type %u != readback %u\n", type, type_readback);
            continue;
        }
    }

    return ret;
}

int cts_write_file(struct file *filp, const void *data, size_t size)
{
    loff_t  pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
}

/* Make directory for filepath
 * If filepath = "/A/B/C/D.file", it will make dir /A/B/C recursive
 * like userspace mkdir -p
 */
int cts_mkdir_for_file(const char *filepath, umode_t mode)
{
    char *dirname = NULL;
    int   dirname_len;
    char *s;
    int   ret;
    mm_segment_t fs;

    if (filepath == NULL) {
        TPD_INFO("<E> Create dir for file with filepath = NULL\n");
        return -EINVAL;
    }

    if (filepath[0] == '\0' || filepath[0] != '/') {
        TPD_INFO("<E> Create dir for file with invalid filepath[0]: %c\n",
            filepath[0]);
        return -EINVAL;
    }

    dirname_len = strrchr(filepath, '/') - filepath;
    if (dirname_len == 0) {
        TPD_INFO("<W> Create dir for file '%s' in root dir\n", filepath);
        return 0;
    }

    dirname = kstrndup(filepath, dirname_len, GFP_KERNEL);
    if (dirname == NULL) {
        TPD_INFO("<E> Create dir alloc mem for dirname failed\n");
        return -ENOMEM;
    }

    TPD_INFO("<I> Create dir '%s' for file '%s'\n", dirname, filepath);

    fs = get_fs();
    set_fs(KERNEL_DS);

    s = dirname + 1;   /* Skip leading '/' */
    while (1) {
        char c = '\0';

        /* Bypass leading non-'/'s and then subsequent '/'s */
        while (*s) {
            if (*s == '/') {
                do {
                    ++s;
                } while (*s == '/');
                c = *s;     /* Save current char */
                *s = '\0';  /* and replace it with nul */
                break;
            }
            ++s;
        }

        TPD_DEBUG("<D>  - Create dir '%s'\n", dirname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
        ret = ksys_mkdir(dirname, 0777);
#else
        ret = sys_mkdir(dirname, 0777);
#endif
        if (ret < 0 && ret != -EEXIST) {
            TPD_INFO("<I> Create dir '%s' failed %d(%s)\n",
                dirname, ret, cts_strerror(ret));
            /* Remove any inserted nul from the path */
            *s = c;
            break;
        }
        /* Reset ret to 0 if return -EEXIST */
        ret = 0;

        if (c) {
            /* Remove any inserted nul from the path */
            *s = c;
        } else {
            break;
        }
    }

    set_fs(fs);

    if (dirname) {
        kfree(dirname);
    }

    return ret;
}

struct file *cts_test_data_filp = NULL;
int cts_start_dump_test_data_to_file(const char *filepath, bool append_to_file)
{
    int ret;

#define START_BANNER \
        ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    TPD_INFO("<I> Start dump test data to file '%s'\n", filepath);

    ret = cts_mkdir_for_file(filepath, 0777);
    if (ret) {
        TPD_INFO("<E> Create dir for test data file failed %d\n", ret);
        return ret;
    }

    cts_test_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_test_data_filp)) {
        ret = PTR_ERR(cts_test_data_filp);
        cts_test_data_filp = NULL;
        TPD_INFO("<E> Open file '%s' for test data failed %d\n",
            cts_test_data_filp, ret);
        return ret;
    }

    cts_write_file(cts_test_data_filp, START_BANNER, strlen(START_BANNER));

    return 0;
#undef START_BANNER
}

void cts_stop_dump_test_data_to_file(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
    int r;

    TPD_INFO("<I> Stop dump test data to file\n");

    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp,
            END_BANNER, strlen(END_BANNER));
        r = filp_close(cts_test_data_filp, NULL);
        if (r) {
            TPD_INFO("<E> Close test data file failed %d\n", r);
        }
        cts_test_data_filp = NULL;
    } else {
        TPD_INFO("<W> Stop dump tsdata to file with filp = NULL\n");
    }
#undef END_BANNER
}

void cts_dump_tsdata(struct cts_device *cts_dev,
        const char *desc, const u16 *data,
        bool to_console, bool is_fs_mode)
{
#define SPLIT_LINE_STR \
    "---------------------------------------------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u8  rows;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[128];
    int count = 0;

    rows = is_fs_mode ? cts_dev->fwdata.rows : 4;

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            u16 val = data[r * cts_dev->fwdata.cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
        TPD_INFO("<I> %s\n", line_buf);
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < cts_dev->fwdata.cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        TPD_INFO("<I> %s\n", line_buf);
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count +=
                scnprintf(line_buf + count, sizeof(line_buf) - count,
                    DATA_FORMAT_STR,
                    data[r * cts_dev->fwdata.cols + c]);
        }
        if (to_console) {
            TPD_INFO("<I> %s\n", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

static bool is_invalid_node(u32 *invalid_nodes, u32 num_invalid_nodes,
    u16 row, u16 col)
{
    int i;

    for (i = 0; i < num_invalid_nodes; i++) {
        if (MAKE_INVALID_NODE(row,col)== invalid_nodes[i]) {
            return true;
        }
    }

    return false;
}

int validate_tsdata(struct cts_device *cts_dev,
    const char *desc, u16 *data,
    u32 *invalid_nodes, u32 num_invalid_nodes,
    bool per_node, int *min, int *max, bool is_fs_mode)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
	u8  rows;
    int failed_cnt = 0;

    rows = is_fs_mode ? cts_dev->fwdata.rows : 4;

    TPD_INFO("<I> %s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]\n",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r,c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
                    TPD_INFO("<I> %s failed nodes:\n", desc);
                }
                failed_cnt++;

                TPD_INFO("<I>   %3d: [%-2d][%-2d] = %u\n",
                    failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
        TPD_INFO("<I> %s test %d node total failed\n", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static int validate_comp_cap(struct cts_device *cts_dev,
    const char *desc, u8 *cap,
    u32 *invalid_nodes, u32 num_invalid_nodes,
    bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    int failed_cnt = 0;

    TPD_INFO("<I> Validate %s data: %s, num invalid node: %u, thresh[0]=[%d, %d]\n",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r,c)) {
                continue;
            }

            if ((min != NULL && cap[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && cap[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
                    TPD_INFO("<I> %s failed nodes:\n", desc);
                }
                failed_cnt++;

                TPD_INFO("<I>   %3d: [%-2d][%-2d] = %u\n",
                    failed_cnt, r, c, cap[offset]);
            }
        }
    }

    if (failed_cnt) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
        TPD_INFO("<I> %s test %d node total failed\n", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static int wait_fw_to_normal_work(struct cts_device *cts_dev)
{
    int i = 0;
    int ret;

    TPD_INFO("<I> Wait fw to normal work\n");

    do {
        u8 work_mode;

        ret = cts_fw_reg_readb(cts_dev,
            CTS_DEVICE_FW_REG_GET_WORK_MODE, &work_mode);
        if (ret) {
            TPD_INFO("<E> Get fw curr work mode failed %d\n", work_mode);
            continue;
        } else {
            if (work_mode == CTS_FIRMWARE_WORK_MODE_NORMAL) {
                return 0;
            }
        }

        mdelay (10);
    } while (++i < 100);

    return -ETIMEDOUT;
}

static int prepare_test(struct cts_device *cts_dev)
{
    int ret;

    TPD_INFO("<I> Prepare test\n");

    cts_plat_reset_device(cts_dev->pdata);

    ret = cts_set_dev_esd_protection(cts_dev, false);
    if (ret) {
        TPD_INFO("<E> Disable firmware ESD protection failed %d\n", ret);
        return ret;
    }

    ret = disable_fw_monitor_mode(cts_dev);
    if (ret) {
        TPD_INFO("<E> Disable firmware monitor mode failed %d\n", ret);
        return ret;
    }

    ret = disable_fw_auto_compensate(cts_dev);
    if (ret) {
        TPD_INFO("<E> Disable firmware auto compensate failed %d\n", ret);
        return ret;
    }

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_CONFIG);
    if (ret) {
        TPD_INFO("<E> Set firmware work mode to WORK_MODE_CONFIG failed %d\n", ret);
        return ret;
    }

    cts_dev->rtdata.testing = true;

    return 0;
}

static void post_test(struct cts_device *cts_dev)
{
    int ret;

    TPD_INFO("<I> Post test\n");

    cts_plat_reset_device(cts_dev->pdata);

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_NORMAL);
    if (ret) {
        TPD_INFO("<E> Set firmware work mode to WORK_MODE_NORMAL failed %d\n", ret);
    }

    ret = wait_fw_to_normal_work(cts_dev);
    if (ret) {
        TPD_INFO("<E> Wait fw to normal work failed %d\n", ret);
        //return ret;
    }

    cts_dev->rtdata.testing = false;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_short(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_if_failed = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  loopcnt;
    int  ret;
    u16 *test_result = NULL;
    bool recovery_display_state = false;
    u8   need_display_on;
    u8   feature_ver;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        TPD_INFO("<E> Short test with invalid param: cts_dev: %p test param: %p\n",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_if_failed =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    TPD_INFO("<I> Short test, flags: 0x%08x,"
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *)param->test_data_buf;
    } else {
        test_result = (u16 *)kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            TPD_INFO("<E> Allocate test result buffer failed\n");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    ret = prepare_test(cts_dev);
    if (ret) {
        TPD_INFO("<E> Prepare test failed %d\n", ret);
        goto unlock_device;
    }

    TPD_INFO("<I> Test short to GND\n");

    ret = cts_sram_readb(cts_dev, 0xE8, &feature_ver);
    if (ret) {
        TPD_INFO("<E> Read firmware feature version failed %d\n", ret);
        goto post_test;
    }
    TPD_INFO("<I> Feature version: %u\n", feature_ver);

    if (feature_ver > 0) {
        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_UNDEFINED);
        if (ret) {
            TPD_INFO("<E> Set short test type to UNDEFINED failed %d\n", ret);
            goto post_test;
        }

        ret = set_fw_test_type(cts_dev, CTS_TEST_SHORT);
        if (ret) {
            TPD_INFO("<E> Set test type to SHORT failed %d\n", ret);
            goto post_test;
        }

        ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
        if (ret) {
            TPD_INFO("<E> Set firmware work mode to WORK_MODE_TEST failed %d\n",
                ret);
            goto post_test;
        }

        if (feature_ver <= 3) {
            u8 val;

            TPD_INFO("<I> Patch short test issue\n");

            ret = cts_hw_reg_readb(cts_dev, 0x350E2, &val);
            if (ret) {
                TPD_INFO("<E> Read 0x350E2 failed %d\n", ret);
                goto post_test;
            }
            if ((val & (BIT(2) | BIT(5))) != 0) {
                ret = cts_hw_reg_writeb(cts_dev, 0x350E2, val & 0xDB);
                if (ret) {
                    TPD_INFO("<E> Write 0x350E2 failed %d\n", ret);
                    goto post_test;
                }
            }
        }

        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_GND);
        if (ret) {
            TPD_INFO("<E> Set short test type to SHORT_TO_GND failed %d\n", ret);
            goto post_test;
        }

        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            TPD_INFO("<E> Wait test complete failed %d\n", ret);
            goto post_test;
        }
    } else {
        ret = cts_send_command(cts_dev, CTS_CMD_RECOVERY_TX_VOL);
        if (ret) {
            TPD_INFO("<E> Send command RECOVERY_TX_VOL failed %d\n", ret);
            goto post_test;
        }

        ret = wait_test_complete(cts_dev, 2);
        if (ret) {
            TPD_INFO("<E> Wait test complete failed %d\n", ret);
            goto post_test;
        }

        // TODO: In factory mode
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        TPD_INFO("<E> Read test result failed %d\n", ret);
        goto post_test;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size = 0;
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d\n", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "GND-short", test_result,
            dump_test_data_to_console, true);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "GND-short",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max, true);
        if (ret) {
            TPD_INFO("<E> Short to GND test failed %d\n", ret);
            if (stop_if_failed) {
                goto stop_dump_test_data_to_file;
            }
        }
    }

    if (dump_test_data_to_user) {
        test_result += num_nodes;
    }

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON, &need_display_on);
    if (ret) {
        TPD_INFO("<E> Read need display on register failed %d\n", ret);
        goto stop_dump_test_data_to_file;
    }

    if (need_display_on == 0) {
        ret = set_display_state(cts_dev, false);
        if (ret) {
            TPD_INFO("<E> Set display state to SLEEP failed %d\n", ret);
            goto stop_dump_test_data_to_file;
        }
        recovery_display_state = true;
    }

    /*
     * Short between colums
     */
    TPD_INFO("<I> Test short between columns\n");

#if 0
    ret = set_fw_test_type(cts_dev, CTS_TEST_SHORT);
    if (ret) {
        TPD_INFO("<E> Set test type to SHORT failed %d\n", ret);
        return ret;
    }
#endif

    ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
    if (ret) {
        TPD_INFO("<E> Set short test type to BETWEEN_COLS failed %d\n", ret);
        goto recovery_display_state;
    }

#if 0
    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
    if (ret) {
        TPD_INFO("<E> Set firmware work mode to WORK_MODE_TEST failed %d\n",
            ret);
        return ret;
    }
#endif

    if (need_display_on == 0) {
        TPD_INFO("<I> Skip first frame data\n");

        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            TPD_INFO("<E> Wait test complete failed %d\n", ret);
            goto recovery_display_state;
        }

        ret = get_test_result(cts_dev, test_result);
        if (ret) {
            TPD_INFO("<E> Read skip test result failed %d\n", ret);
            goto recovery_display_state;
        }

        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
        if (ret) {
            TPD_INFO("<E> Set short test type to BETWEEN_COLS failed %d\n",
                ret);
            goto recovery_display_state;
        }
    }

    ret = wait_test_complete(cts_dev, 0);
    if (ret) {
        TPD_INFO("<E> Wait test complete failed %d\n", ret);
        goto recovery_display_state;
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        TPD_INFO("<E> Read test result failed %d\n", ret);
        goto recovery_display_state;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Col-short", test_result,
            dump_test_data_to_console, true);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Col-short",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max, true);
        if (ret) {
            TPD_INFO("<E> Short between columns test failed %d\n", ret);
            if (stop_if_failed) {
                goto recovery_display_state;
            }
        }
    }

    if (dump_test_data_to_user) {
        test_result += num_nodes;
    }

    /*
     * Short between colums
     */
    TPD_INFO("<I> Test short between rows\n");

    ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_ROWS);
    if (ret) {
        TPD_INFO("<E> Set short test type to BETWEEN_ROWS failed %d\n", ret);
        goto recovery_display_state;
    }

    loopcnt = cts_dev->hwdata->num_row;
    while (loopcnt > 1) {
        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            TPD_INFO("<E> Wait test complete failed %d\n", ret);
            goto recovery_display_state;
        }

        ret = get_test_result(cts_dev, test_result);
        if (ret) {
            TPD_INFO("<E> Read test result failed %d\n", ret);
            goto recovery_display_state;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Row-short", test_result,
                dump_test_data_to_console, true);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Row-short",
                test_result, param->invalid_nodes, param->num_invalid_node,
                validate_data_per_node, param->min, param->max, true);
            if (ret) {
                TPD_INFO("<E> Short between columns test failed %d\n", ret);
                if (stop_if_failed) {
                    goto recovery_display_state;
                }
            }
        }

        if (dump_test_data_to_user) {
            test_result += num_nodes;
        }

        loopcnt += loopcnt % 2;
        loopcnt = loopcnt >> 1;
    }

recovery_display_state:
    if (recovery_display_state) {
        int r = set_display_state(cts_dev, true);
        if (r) {
            TPD_INFO("<E> Set display state to ACTIVE failed %d\n", r);
        }
    }

stop_dump_test_data_to_file:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

post_test:
    post_test(cts_dev);

unlock_device:
    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        TPD_INFO("<I> Short test has %d nodes FAIL, ELAPSED TIME: %lldms\n",
            ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        TPD_INFO("<I> Short test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Short test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }


    return ret;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_open(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  ret;
    u16 *test_result = NULL;
    bool recovery_display_state = false;
    u8   need_display_on;
    ktime_t start_time, end_time, delta_time;

    *param->test_data_wr_size = 0;

    if (cts_dev == NULL || param == NULL) {
        TPD_INFO("<E> Open test with invalid param: cts_dev: %p test param: %p\n",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    TPD_INFO("<I> Open test, flags: 0x%08x,"
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *)param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            TPD_INFO("<E> Allocate memory for test result faild\n");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    ret = prepare_test(cts_dev);
    if (ret) {
        TPD_INFO("<E> Prepare test failed %d\n", ret);
        goto unlock_device;
    }

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON, &need_display_on);
    if (ret) {
        TPD_INFO("<E> Read need display on register failed %d\n", ret);
        goto post_test;
    }

    if (need_display_on == 0) {
        ret = set_display_state(cts_dev, false);
        if (ret) {
            TPD_INFO("<E> Set display state to SLEEP failed %d\n", ret);
            goto post_test;
        }
        recovery_display_state = true;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_RECOVERY_TX_VOL);
    if (ret) {
        TPD_INFO("<E> Recovery tx voltage failed %d\n", ret);
        goto recovery_display_state;
    }

    ret = set_fw_test_type(cts_dev, CTS_TEST_OPEN);
    if (ret) {
        TPD_INFO("<E> Set test type to OPEN_TEST failed %d\n", ret);
        goto recovery_display_state;
    }

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
    if (ret) {
        TPD_INFO("<E> Set firmware work mode to WORK_MODE_TEST failed %d\n",
            ret);
        goto recovery_display_state;
    }

    ret = wait_test_complete(cts_dev, 2);
    if (ret) {
        TPD_INFO("<E> Wait test complete failed %d\n", ret);
        goto recovery_display_state;
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        TPD_INFO("<E> Read test result failed %d\n", ret);
        goto recovery_display_state;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d\n", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Open-circuit", test_result,
            dump_test_data_to_console, true);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Open-circuit",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max, true);
    }

recovery_display_state:
    if (recovery_display_state) {
        int r = set_display_state(cts_dev, true);
        if (r) {
            TPD_INFO("<E> Set display state to ACTIVE failed %d\n", r);
        }
    }

post_test:
    post_test(cts_dev);

unlock_device:
    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        TPD_INFO("<I> Open test has %d nodes FAIL, ELAPSED TIME: %lldms\n",
            ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        TPD_INFO("<I> Open test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Open test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_test_reset_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        TPD_INFO("<E> Reset-pin test with invalid param: cts_dev: %p test param: %p\n",
            cts_dev, param);
        return -EINVAL;
    }

    TPD_INFO("<I> Reset-Pin test, flags: 0x%08x, "
               "drive log file: '%s' buf size: %d\n",
        param->flags,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);

    cts_plat_set_reset(cts_dev->pdata, 0);
    mdelay(50);
#ifdef CONFIG_CTS_I2C_HOST
    /* Check whether device is in normal mode */
    if (cts_plat_is_i2c_online(cts_dev->pdata,
        CTS_DEV_NORMAL_MODE_I2CADDR)) {
#else
    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
#endif /* CONFIG_CTS_I2C_HOST */
        ret = -EIO;
        TPD_INFO("<E> Device is alive while reset is low\n");
    }
    cts_plat_set_reset(cts_dev->pdata, 1);
    mdelay(50);

    {
        int r = wait_fw_to_normal_work(cts_dev);
        if (r) {
            TPD_INFO("<E> Wait fw to normal work failed %d\n", r);
        }
    }

#ifdef CONFIG_CTS_I2C_HOST
    /* Check whether device is in normal mode */
    if (!cts_plat_is_i2c_online(cts_dev->pdata,
        CTS_DEV_NORMAL_MODE_I2CADDR)) {
#else
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
#endif /* CONFIG_CTS_I2C_HOST */
        ret = -EIO;
        TPD_INFO("<E> Device is offline while reset is high\n");
    }

#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            TPD_INFO("<E> Start device failed %d\n", r);
        }
    }

    if (!cts_dev->rtdata.program_mode) {
        cts_set_normal_addr(cts_dev);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        TPD_INFO("<I> Reset-Pin test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Reset-Pin test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}
#endif

int cts_test_int_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        TPD_INFO("<E> Int-pin test with invalid param: cts_dev: %p test param: %p\n",
            cts_dev, param);
        return -EINVAL;
    }

    TPD_INFO("<I> Int-Pin test, flags: 0x%08x, "
             "drive log file: '%s' buf size: %d\n",
        param->flags,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);

    ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_HIGH);
    if (ret) {
        TPD_INFO("<E> Send command WRTITE_INT_HIGH failed %d\n", ret);
        goto unlock_device;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) == 0) {
        TPD_INFO("<E> INT pin state != HIGH\n");
        ret = -EFAULT;
        goto exit_int_test;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_LOW);
    if (ret) {
        TPD_INFO("<E> Send command WRTITE_INT_LOW failed %d\n", ret);
        goto exit_int_test;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) != 0) {
        TPD_INFO("<E> INT pin state != LOW\n");
        ret = -EFAULT;
        goto exit_int_test;
    }

exit_int_test:
    {
        int r = cts_send_command(cts_dev, CTS_CMD_RELASE_INT_TEST);
        if (r) {
            TPD_INFO("<E> Send command RELASE_INT_TEST failed %d\n", r);
        }
    }
    mdelay(10);

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            TPD_INFO("<E> Start device failed %d\n", r);
        }
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        TPD_INFO("<I> Int-Pin test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Int-Pin test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}

void cts_dump_comp_cap(struct cts_device *cts_dev, u8 *cap, bool to_console)
{
#define SPLIT_LINE_STR \
            "-----------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%3u "
#define DATA_FORMAT_STR     "%4d"

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[128];
    int count;

    max = min = cap[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            u16 val = cap[r * cts_dev->fwdata.cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
              " Compensate Cap MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
              min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
        TPD_INFO("<I> %s\n", line_buf);
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "      ");
    for (c = 0; c < cts_dev->fwdata.cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                  COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        TPD_INFO("<I> %s\n", line_buf);
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                  ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count += scnprintf(line_buf + count,
                      sizeof(line_buf) - count,
                      DATA_FORMAT_STR,
                      cap[r * cts_dev->fwdata.cols + c]);
        }
        if (to_console) {
            TPD_INFO("<I> %s\n", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }

    if (to_console) {
        TPD_INFO("<I> %s\n", SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    u8 * cap = NULL;
    int  ret = 0;
    ktime_t start_time, end_time, delta_time;

    *param->test_data_wr_size = 0;

    if (cts_dev == NULL || param == NULL) {
        TPD_INFO("<E> Compensate cap test with invalid param: cts_dev: %p test param: %p\n",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    TPD_INFO("<I> Compensate cap test, flags: 0x%08x "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        cap = (u8 *)param->test_data_buf;
    } else {
        cap = (u8 *)kzalloc(num_nodes, GFP_KERNEL);
        if (cap == NULL) {
            TPD_INFO("<E> Alloc mem for compensate cap failed\n");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);
    ret = cts_get_compensate_cap(cts_dev, cap);
    cts_unlock_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Get compensate cap failed %d\n", ret);
        goto start_device;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size = num_nodes;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d\n", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_comp_cap(cts_dev, cap,
            dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_comp_cap(cts_dev, "Compensate-Cap",
            cap, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
    }

start_device:
    {
        int r = cts_start_device(cts_dev);
        if (r) {
            TPD_INFO("<E> Start device failed %d\n", r);
        }
    }

free_mem:
    if (!dump_test_data_to_user && cap) {
        kfree(cap);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        TPD_INFO("<I> Compensate-Cap test has %d nodes FAIL, ELAPSED TIME: %lldms\n",
            ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        TPD_INFO("<I> Compensate-Cap test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Compensate-Cap test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_test_rawdata(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    u16 *rawdata = NULL;
    ktime_t start_time, end_time, delta_time;
    int  i;
    int  ret;

    *param->test_data_wr_size = 0;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        TPD_INFO("<E> Rawdata test with invalid param: priv param: %p size: %d\n",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        TPD_INFO("<I> Rawdata test with too little frame %u\n",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    TPD_INFO("<I> Rawdata test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        rawdata = (u16 *)param->test_data_buf;
    } else {
        rawdata = (u16 *)kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (rawdata == NULL) {
            TPD_INFO("<E> Allocate memory for rawdata failed\n");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    for (i = 0; i < 5; i++) {
        int r;
        u8 val;
        r = cts_enable_get_rawdata(cts_dev);
        if (r) {
            TPD_INFO("<E> Enable get tsdata failed %d\n", r);
            continue;
        }
        mdelay(1);
        r = cts_fw_reg_readb(cts_dev, 0x12, &val);
        if (r) {
            TPD_INFO("<E> Read enable get tsdata failed %d\n", r);
            continue;
        }
        if (val != 0) {
            break;
        }
    }

    if (i >= 5) {
        TPD_INFO("<E> Enable read tsdata failed\n");
        ret = -EIO;
        goto unlock_device;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d\n", r);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;
        int  r;

        //if (cts_dev.fwdata.monitor_mode) {
            r = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
            if (r) {
                TPD_INFO("<E> Send CMD_QUIT_GESTURE_MONITOR failed %d\n", r);
            }
        //}

        for (i = 0; i < 3; i++) {
            r = cts_get_rawdata(cts_dev, rawdata);
            if (r) {
                TPD_INFO("<E> Get rawdata failed %d\n", r);
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            ret = -EIO;
            break;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Rawdata", rawdata,
                dump_test_data_to_console, true);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev,
                "Rawdata", rawdata,
                param->invalid_nodes, param->num_invalid_node,
                validate_data_per_node, param->min, param->max, true);
            if (ret) {
                TPD_INFO("<E> Rawdata test failed %d\n", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_data_to_user) {
            rawdata += num_nodes;
        }
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    for (i = 0; i < 5; i++) {
        int r = cts_disable_get_rawdata(cts_dev);
        if (r) {
            TPD_INFO("<E> Disable get rawdata failed %d\n", r);
            continue;
        } else {
            break;
        }
    }

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            TPD_INFO("<E> Start device failed %d\n", r);
        }
    }

free_mem:
    if (!dump_test_data_to_user && rawdata != NULL) {
        kfree(rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        TPD_INFO("<I> Rawdata test has %d nodes FAIL, ELAPSED TIME: %lldms\n",
            ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        TPD_INFO("<I> Rawdata test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Rawdata test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_test_noise(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    u16 *buffer = NULL;
    int  buf_size = 0;
    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *noise = NULL;
    bool first_frame = true;
    bool data_valid = false;
    ktime_t start_time, end_time, delta_time;
    int  i;
    int  ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        TPD_INFO("<E> Noise test with invalid param: priv param: %p size: %d\n",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames < 2) {
        TPD_INFO("<E> Noise test with too little frame %u\n",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    TPD_INFO("<I> Noise test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    buf_size = (driver_validate_data ? 4 : 1) * tsdata_frame_size;
    buffer = (u16 *)kmalloc(buf_size, GFP_KERNEL);
    if (buffer == NULL) {
        TPD_INFO("<E> Alloc mem for touch data failed\n");
        ret = -ENOMEM;
        goto show_test_result;
    }

    curr_rawdata = buffer;
    if (driver_validate_data) {
        max_rawdata = curr_rawdata + 1 * num_nodes;
        min_rawdata = curr_rawdata + 2 * num_nodes;
        noise       = curr_rawdata + 3 * num_nodes;
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        TPD_INFO("<E> Stop device failed %d\n", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    for (i = 0; i < 5; i++) {
        int r;
        u8 val;
        r = cts_enable_get_rawdata(cts_dev);
        if (r) {
            TPD_INFO("<E> Enable get ts data failed %d\n", r);
            continue;
        }
        mdelay(1);
        r = cts_fw_reg_readb(cts_dev, 0x12, &val);
        if (r) {
            TPD_INFO("<E> Read enable get ts data failed %d\n", r);
            continue;
        }
        if (val != 0) {
            break;
        }
    }

    if (i >= 5) {
        TPD_INFO("<E> Enable read tsdata failed\n");
        ret = -EIO;
        goto unlock_device;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d\n", r);
        }
    }

    msleep(50);

    for (frame = 0; frame < priv_param->frames; frame++) {
        int r;

        r = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
        if (r) {
            TPD_INFO("<E> send quit gesture monitor failed %d\n", r);
            // Ignore this error
        }

        for (i = 0; i < 3; i++) {
            r = cts_get_rawdata(cts_dev, curr_rawdata);
            if (r) {
                TPD_INFO("<E> Get rawdata failed %d\n", r);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            TPD_INFO("<E> Read rawdata failed\n");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise-rawdata", curr_rawdata,
                dump_test_data_to_console, true);
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size = 0;
            memcpy(param->test_data_buf + frame * tsdata_frame_size,
                curr_rawdata, tsdata_frame_size);
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(max_rawdata, curr_rawdata, tsdata_frame_size);
                memcpy(min_rawdata, curr_rawdata, tsdata_frame_size);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }
    }

    for (i = 0; i < num_nodes; i++) {
        noise[i] = max_rawdata[i] - min_rawdata[i];
    }
    memcpy(param->test_data_buf + frame * tsdata_frame_size,
            noise, tsdata_frame_size);
    *param->test_data_wr_size += tsdata_frame_size;


    data_valid = true;

disable_get_tsdata:
    for (i = 0; i < 5; i++) {
        int r = cts_disable_get_rawdata(cts_dev);
        if (r) {
            TPD_INFO("<E> Disable get rawdata failed %d\n", r);
            continue;
        } else {
            break;
        }
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            TPD_INFO("<E> Start device failed %d\n", r);
        }
    }

    if (driver_validate_data && data_valid) {
        for (i = 0; i < num_nodes; i++) {
            noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise", noise,
                dump_test_data_to_console, true);
        }

        ret = validate_tsdata(cts_dev, "Noise test",
            noise, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max, true);
    }

free_mem:
    if (buffer) {
        kfree(buffer);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        TPD_INFO("<I> Noise test has %d nodes FAIL, ELAPSED TIME: %lldms\n",
            ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        TPD_INFO("<I> Noise test FAIL %d(%s), ELAPSED TIME: %lldms\n",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        TPD_INFO("<I> Noise test PASS, ELAPSED TIME: %lldms\n",
            ktime_to_ms(delta_time));
    }

    return ret;
}


static bool set_gesture_raw_type(struct cts_device *cts_dev, u8 type)
{
    u8 val = 0xff, r;

    TPD_INFO("<I> Set gesture raw type: %d\n", type);
    r = cts_fw_reg_writeb(cts_dev, 0x45, type);
    if (r) {
        TPD_INFO("<E> Set gesture raw type failed %d\n", r);
        return false;
    }

    r = cts_fw_reg_readb(cts_dev, 0x45, &val);
    if (r) {
        TPD_INFO("<E> Get gesture raw type failed %d\n", r);
        return false;
    }
    return val == type;
}


int prepare_black_test(struct cts_device *cts_dev)
{
    int ret;
    u8 buf;

    TPD_INFO("<I> Prepare black test\n");

    cts_plat_reset_device(cts_dev->pdata);

    ret = cts_set_dev_esd_protection(cts_dev, false);
    if (ret) {
        TPD_INFO("<E> Disable firmware ESD protection failed %d(%s)\n",
            ret, cts_strerror(ret));
        return ret;
    }

    ret = disable_fw_monitor_mode(cts_dev);
    if (ret) {
        TPD_INFO("<E> Disable firmware monitor mode failed %d(%s)\n",
            ret, cts_strerror(ret));
        return ret;
    }

    /* Disable GSTR ONLY FS Switch */
    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_GSTR_ONLY_FS_EN, &buf);
    if (ret) {
        TPD_INFO("<E> Get GSTR ONLY FS EN failed %d(%s)\n",
                ret, cts_strerror(ret));
        return ret;
    }
    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GSTR_ONLY_FS_EN,
            (buf & 0xFB));
    if (ret) {
        TPD_INFO("<E> Disable GSTR ONLY FS failed %d(%s)\n",
                ret, cts_strerror(ret));
        return ret;
    }

    /* Enable GSTR DATA DBG */
    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_GSTR_DATA_DBG_EN, &buf);
    if (ret) {
        TPD_INFO("<E> get GSTR DATA DBG EN failed %d(%s)\n",
                ret, cts_strerror(ret));
        return ret;
    }

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_GSTR_DATA_DBG_EN,
            (buf | BIT(6)));
    if (ret) {
        TPD_INFO("<E> Enable GSTR DATA DBG failed %d(%s)\n",
                ret, cts_strerror(ret));
        return ret;
    }

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_GSTR_DBG);
    if (ret) {
        TPD_INFO("<E> Set WORK_MODE_GSTR_DBG failed %d(%s)\n",
                ret, cts_strerror(ret));
        return ret;
    }
    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_POWER_MODE, &buf);
    if (ret) {
        TPD_INFO("<E> get POWER MODE failed %d(%s)\n", ret, cts_strerror(ret));
        return ret;
    }

    return 0;
}


int cts_test_gesture_rawdata(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    int  idle_mode;
    u16 *gesture_rawdata = NULL;
    int  i;
    int  ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        TPD_INFO("<E> Gesture rawdata test invalid priv param: %p size: %d\n",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        TPD_INFO("<I> Gesture rawdata test with too little frame %u\n",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    TPD_INFO("<I> Gesture rawdata test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);


    if (dump_test_data_to_user) {
        gesture_rawdata = (u16 *)param->test_data_buf;
    } else {
        gesture_rawdata = (u16 *)kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (gesture_rawdata == NULL) {
            TPD_INFO("<E> Allocate memory for rawdata failed\n");
            return -ENOMEM;
        }
    }

    cts_lock_device(cts_dev);

    idle_mode = priv_param->work_mode;

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d(%s)\n",
                r, cts_strerror(r));
        }
    }

    msleep(50);

    for (i = 0; i < 5; i++) {
        if (set_gesture_raw_type(cts_dev, idle_mode))
            break;
    }

    /* Skip 10 frames */
    for (i = 0; i < 10; i++) {
        int r = cts_get_rawdata(cts_dev, gesture_rawdata);
        if (r) {
            TPD_INFO("<E> Get gesture rawdata failed %d(%s)\n",
                r, cts_strerror(r));
            mdelay(30);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;
        int  r;

        for (i = 0; i < 10; i++) {
            r = cts_get_rawdata(cts_dev, gesture_rawdata);
            if (r) {
                TPD_INFO("<E> Get gesture rawdata failed %d(%s)\n",
                    r, cts_strerror(r));
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            ret = -EIO;
            break;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size = 0;
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev,
                idle_mode ? "Gesture Rawdata" : "Gesture LP Rawdata",
                gesture_rawdata, dump_test_data_to_console, !!idle_mode);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev,
                idle_mode ? "Gesture Rawdata" : "Gesture LP Rawdata",
                gesture_rawdata, param->invalid_nodes, param->num_invalid_node,
                validate_data_per_node, param->min, param->max, !!idle_mode);
            if (ret) {
                TPD_INFO("<E> Gesture Rawdata test failed %d(%s)\n",
                    ret, cts_strerror(ret));
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_data_to_user) {
            gesture_rawdata += num_nodes;
        }
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    cts_unlock_device(cts_dev);

    if (!dump_test_data_to_user && gesture_rawdata != NULL) {
        kfree(gesture_rawdata);
    }

    return ret;
}

int cts_test_gesture_noise(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    int  idle_mode;
    u16 *buffer = NULL;
    int  buf_size = 0;
    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *gesture_noise = NULL;
    bool first_frame = true;
    int  i;
    int  ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        TPD_INFO("<E> Noise test with invalid param: priv param: %p size: %d\n",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames < 2) {
        TPD_INFO("<E> Noise test with too little frame %u\n",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    TPD_INFO("<I> Noise test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d\n",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);


    buf_size = (driver_validate_data ? 4 : 1) * tsdata_frame_size;
    buffer = (u16 *)kmalloc(buf_size, GFP_KERNEL);
    if (buffer == NULL) {
        TPD_INFO("<E> Alloc mem for touch data failed\n");
        return -ENOMEM;
    }

    curr_rawdata = buffer;
    if (driver_validate_data) {
        max_rawdata = curr_rawdata + 1 * num_nodes;
        min_rawdata = curr_rawdata + 2 * num_nodes;
        gesture_noise = curr_rawdata + 3 * num_nodes;
    }

    cts_lock_device(cts_dev);

    idle_mode = priv_param->work_mode;

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            TPD_INFO("<E> Start dump test data to file failed %d(%s)\n",
                r, cts_strerror(r));
        }
    }

    msleep(50);

    for (i = 0; i < 5; i++) {
        if (set_gesture_raw_type(cts_dev, idle_mode))
            break;
    }

    /* Skip 10 frames */
    for (i = 0; i < 10; i++) {
        int r = cts_get_rawdata(cts_dev, curr_rawdata);
        if (r) {
            TPD_INFO("<E> Get rawdata failed %d(%s)\n",
                r, cts_strerror(r));
            mdelay(30);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;
        int r;

        for (i = 0; i < 10; i++) {
            r = cts_get_rawdata(cts_dev, curr_rawdata);
            if (r) {
                TPD_INFO("<E> Get rawdata failed %d(%s)\n",
                    r, cts_strerror(r));
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            TPD_INFO("<E> Read rawdata failed\n");
            ret = -EIO;
            break;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev,
                idle_mode ? "Gstr Noise-Raw" : "Gstr LP Noise-Raw",
                curr_rawdata, dump_test_data_to_console, !!idle_mode);
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size = 0;
            memcpy(param->test_data_buf + *param->test_data_wr_size,
                curr_rawdata, tsdata_frame_size);
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(max_rawdata, curr_rawdata, tsdata_frame_size);
                memcpy(min_rawdata, curr_rawdata, tsdata_frame_size);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }
    }

    if (driver_validate_data) {
        for (i = 0; i < num_nodes; i++) {
            gesture_noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_data_to_user &&
            param->test_data_buf_size >=
                (*param->test_data_wr_size + tsdata_frame_size)) {
            memcpy(param->test_data_buf + *param->test_data_wr_size,
               gesture_noise, tsdata_frame_size);
            *param->test_data_wr_size += tsdata_frame_size;
       }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev,
                idle_mode ? "Gesture Noise" : "Gesture LP Noise",
                gesture_noise, dump_test_data_to_console, !!idle_mode);
        }

        ret = validate_tsdata(cts_dev,
            idle_mode ? "Gesture Noise" : "Gesture LP Noise",
            gesture_noise, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max, !!idle_mode);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    cts_unlock_device(cts_dev);

    if (buffer) {
        kfree(buffer);
    }

    return ret;
}

