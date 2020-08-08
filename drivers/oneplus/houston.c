#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/poll.h>

#define HT_CLUSTERS 3
#define HT_MONITOR_SIZE 58
#define HT_CTL_NODE "ht_ctl"

static dev_t ht_ctl_dev;
static struct class *driver_class;
static struct cdev cdev;

static unsigned int ht_ctl_poll(struct file *fp, poll_table *wait)
{
	return POLLIN;
}

static long ht_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long __user arg)
{
	return 0;
}

static const struct file_operations ht_ctl_fops = {
	.owner = THIS_MODULE,
	.poll = ht_ctl_poll,
	.unlocked_ioctl = ht_ctl_ioctl,
	.compat_ioctl = ht_ctl_ioctl,
};

static int fps_sync_init(void)
{
	int rc;
	struct device *class_dev;

	rc = alloc_chrdev_region(&ht_ctl_dev, 0, 1, HT_CTL_NODE);
	if (rc < 0) {
		return 0;
	}

	driver_class = class_create(THIS_MODULE, HT_CTL_NODE);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		goto exit_unreg_chrdev_region;
	}
	class_dev = device_create(driver_class, NULL, ht_ctl_dev, NULL, HT_CTL_NODE);
	if (IS_ERR(class_dev)) {
		rc = -ENOMEM;
		goto exit_destroy_class;
	}
	cdev_init(&cdev, &ht_ctl_fops);
	cdev.owner = THIS_MODULE;
	rc = cdev_add(&cdev, MKDEV(MAJOR(ht_ctl_dev), 0), 1);
	if (rc < 0) {
		goto exit_destroy_device;
	}
	return 0;
exit_destroy_device:
	device_destroy(driver_class, ht_ctl_dev);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(ht_ctl_dev, 1);
	return 0;
}

static int ht_log_lv = 1;
module_param_named(log_lv, ht_log_lv, int, 0664);

/* ais */
static int ais_enable = 0;
module_param_named(ais_enable, ais_enable, int, 0664);

static int ai_on = 1;
module_param_named(ai_on, ai_on, int, 0664);

static int render_pid;
module_param_named(render_pid, render_pid, int, 0664);

static int pccore_always_on;
module_param_named(pcc_always_on, pccore_always_on, int, 0664);

/* report skin_temp to ais */
static unsigned int thermal_update_period_hz = 100;
module_param_named(thermal_update_period_hz, thermal_update_period_hz, uint, 0664);

/*
 * filter mechanism
 * base_util: rtg task util threshold
 * rtg_filter_cnt: rtg task called cnt threshold under 1 sec
 */
static unsigned int base_util = 100;
module_param_named(base_util, base_util, uint, 0664);
static unsigned int rtg_filter_cnt = 10;
module_param_named(rtg_filter_cnt, rtg_filter_cnt, uint, 0664);

static bool cpuload_query = false;
module_param_named(cpuload_query, cpuload_query, bool, 0664);

/* battery query, it takes time to query */
static bool bat_query = false;
module_param_named(bat_query, bat_query, bool, 0664);

static bool bat_sample_high_resolution = false;
module_param_named(bat_sample_high_resolution, bat_sample_high_resolution, bool, 0664);

/* force update battery current */
static unsigned long bat_update_period_us = 1000000; // 1 sec
module_param_named(bat_update_period_us, bat_update_period_us, ulong, 0664);

/* fps boost switch */
static bool fps_boost_enable = true;
module_param_named(fps_boost_enable, fps_boost_enable, bool, 0664);

static bool fps_boost_force_enable;
module_param_named(fps_boost_force_enable, fps_boost_force_enable, bool, 0664);

/* trubo boost switch */
static bool tb_enable = true;
module_param_named(tb_enable, tb_enable, bool, 0664);

/* freq hispeed */
static bool cpufreq_hispeed_enable = false;
module_param_named(cpufreq_hispeed_enable, cpufreq_hispeed_enable, bool, 0664);

static unsigned int cpufreq_hispeed[HT_CLUSTERS] = { 1209600, 1612800, 1612800 };
module_param_array_named(cpufreq_hispeed, cpufreq_hispeed, uint, NULL, 0664);

static bool ddrfreq_hispeed_enable = true;
module_param_named(ddrfreq_hispeed_enable, ddrfreq_hispeed_enable, bool, 0664);

static unsigned int ddrfreq_hispeed = 1017;
module_param_named(ddrfreq_hispeed, ddrfreq_hispeed, uint, 0664);

/* choose boost freq to lock or lower bound */
static unsigned int fps_boost_type = 1;
module_param_named(fps_boost_type, fps_boost_type, uint, 0664);

/* filter out too close boost hint */
static unsigned long fps_boost_filter_us = 8000;
module_param_named(fps_boost_filter_us, fps_boost_filter_us, ulong, 0664);

static unsigned long filter_mask = 0;
module_param_named(filter_mask, filter_mask, ulong, 0664);

static unsigned long disable_mask = 0;
module_param_named(disable_mask, disable_mask, ulong, 0664);

static unsigned int report_div[HT_MONITOR_SIZE];
module_param_array_named(div, report_div, uint, NULL, 0664);

static int perf_ready;
module_param_named(perf_ready, perf_ready, int, 0664);

static int fps_boost_strategy;
module_param_named(fps_boost_strategy, fps_boost_strategy, int, 0664);

static int ht_enable;
module_param_named(ht_enable, ht_enable, int, 0664);

static int sample_rate_ms;
module_param_named(sample_rate_ms, sample_rate_ms, int, 0664);

static int ht_fps_boost_store(const char *buf, const struct kernel_param *kp)
{
	return 0;
}

static struct kernel_param_ops ht_fps_boost_ops = {
	.set = ht_fps_boost_store,
};
module_param_cb(fps_boost, &ht_fps_boost_ops, NULL, 0220);

static int tb_ctl_store(const char *buf, const struct kernel_param *kp)
{
	return 0;
}

static struct kernel_param_ops tb_ctl_ops = {
	.set = tb_ctl_store,
};
module_param_cb(tb_ctl, &tb_ctl_ops, NULL, 0220);

static int ht_fps_data_sync_store(const char *buf, const struct kernel_param *kp)
{
	return 0;
}

static struct kernel_param_ops ht_fps_data_sync_ops = {
	.set = ht_fps_data_sync_store,
};
module_param_cb(fps_data_sync, &ht_fps_data_sync_ops, NULL, 0220);

static int ht_reset_store(const char *buf, const struct kernel_param *kp)
{
	return 0;
}

static struct kernel_param_ops ht_reset_ops = {
	.set = ht_reset_store,
};
module_param_cb(reset, &ht_reset_ops, NULL, 0664);

pure_initcall(fps_sync_init);
