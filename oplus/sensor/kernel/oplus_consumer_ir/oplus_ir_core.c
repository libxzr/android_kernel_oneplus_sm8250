/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/regulator/consumer.h>
#include "oplus_ir_core.h"

#define IR_BYTE_POS_INDEX                0
#define IR_BIT_POS_INDEX                 1
#define IR_POS_NUM                       2
#define IR_HIGHT_HW_DATA_INDEX           2
#define BITS_PER_BYTE                    8
#define IR_DEFAULT_VDD_TYPE              0
#define IR_EXTERNAL_VDD_TYPE             1
#define IR_PARAM_MAX_SIZE                256*1024

#ifdef OPLUS_FEATURE_CAMERA_COMMON
typedef enum {
		EXT_NONE = -1,
		EXT_LDO1,
		EXT_LDO2,
		EXT_LDO3,
		EXT_LDO4,
		EXT_LDO5,
		EXT_LDO6,
		EXT_LDO7,
		EXT_MAX
} EXT_SELECT;

extern int wl2868c_check_ldo_status(void);
extern int wl2868c_ldo_enable(EXT_SELECT ldonum, unsigned int value);
extern int wl2868c_ldo_disable(EXT_SELECT ldonum, unsigned int value);
#endif

struct hw_core_config_t {
	int vdd_type;
	int vdd_min_vol;
	int vdd_max_vol;
	struct regulator *vdd_3v0;
};

struct ir_core {
	struct hw_core_config_t core_config;
	struct platform_device *dev;
	struct miscdevice misc_dev;
	enum ir_interface inf; /*SIP,UART,PWM...*/
	struct device_ops *ops;
	void *priv;
	char *name;
	struct mutex tx_mutex;
};

struct ir_convert_data_t {
	u8* hw_data;
	u32 hw_data_num;
	u32 byte_pos;
	u32 bit_pos;
	u32 bit_num;
	u8 high;
	u32 *pos_record;
};

struct ir_convert_hl_data_t {
	u8* hw_data;
	u32 hw_data_num;
	u32 byte_pos;
	u32 bit_pos;
	u32 hw_data_high_num;
	u32 hw_data_low_num;
	u8 high;
};

static struct ir_core* g_ir = NULL;

static int parse_hw_core_config(struct device *dev, struct ir_core* ir_core)
{
	int retval = 0;
	u32 value = 0;
	struct device_node *np = dev->of_node;

	if (!ir_core) {
		pr_err("oplus_ir_core: parse_hw_core_config error, ir_core is null!\n");
		return -EINVAL;
	} else {
		retval = of_property_read_u32(np, "vdd-min-vol", &value);
		if (retval < 0) {
			ir_core->core_config.vdd_min_vol = IR_DEFAULT_VDD_MIN_VOL;
		} else {
			ir_core->core_config.vdd_min_vol = value;
		}

		retval = of_property_read_u32(np, "vdd-max-vol", &value);
		if (retval < 0) {
			ir_core->core_config.vdd_max_vol = IR_DEFAULT_VDD_MAX_VOL;
		} else {
			ir_core->core_config.vdd_max_vol = value;
		}

		retval = of_property_read_u32(np, "vdd-type", &value);
		if (retval < 0) {
			ir_core->core_config.vdd_type = IR_DEFAULT_VDD_TYPE;
		} else {
			ir_core->core_config.vdd_type = value;
		}

		if (ir_core->core_config.vdd_type == IR_EXTERNAL_VDD_TYPE) {
			ir_core->core_config.vdd_3v0 = NULL;
			pr_info("oplus_ir_core: %s: ir_core->core_config.vdd_3v0 is NULL\n", __func__);
		} else {
			ir_core->core_config.vdd_3v0 = regulator_get(dev, "vdd");
			if (ir_core->core_config.vdd_3v0 != NULL) {
				regulator_set_voltage(ir_core->core_config.vdd_3v0,
					ir_core->core_config.vdd_min_vol, ir_core->core_config.vdd_max_vol);
					pr_info("oplus_ir_core: %s: ir_core->core_config.vdd_3v0 is not NULL\n", __func__);
			} else {
				pr_info("oplus_ir_core: %s: ir_core->core_config.vdd_3v0 is NULL\n", __func__);
			}
		}

		pr_info("oplus_ir_core: %s: vdd-type is %u\n", __func__, ir_core->core_config.vdd_type);
		pr_info("oplus_ir_core: %s: vdd-min-vol is %u\n", __func__, ir_core->core_config.vdd_max_vol);
		pr_info("oplus_ir_core: %s: vdd-max-vol is %u\n", __func__, ir_core->core_config.vdd_min_vol);
	}

	return 0;
}

static void enable_ir_vdd(struct ir_core *ir_core)
{
	int retval = 0;

	if (ir_core->core_config.vdd_3v0 != NULL) {
		regulator_set_voltage(ir_core->core_config.vdd_3v0,
			ir_core->core_config.vdd_min_vol, ir_core->core_config.vdd_max_vol);
		retval = regulator_enable(ir_core->core_config.vdd_3v0);
		if (retval) {
			pr_err("oplus_ir_core: file_write vdd_3v0 enable fail\n");
		}
	}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	if ((true == wl2868c_check_ldo_status()) && (ir_core->core_config.vdd_type == IR_EXTERNAL_VDD_TYPE)) {
		wl2868c_ldo_enable(EXT_LDO5, ir_core->core_config.vdd_max_vol);
		pr_info("oplus_ir_core:wl2868c config value %d \n", ir_core->core_config.vdd_max_vol);
	} else {
		pr_info("oplus_ir_core:wl2868c error status!\n");
	}
#endif
}

static void disable_ir_vdd(struct ir_core *ir_core)
{
	if (ir_core->core_config.vdd_3v0 != NULL) {
		regulator_disable(ir_core->core_config.vdd_3v0);
	}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	if ((true == wl2868c_check_ldo_status()) && (ir_core->core_config.vdd_type == IR_EXTERNAL_VDD_TYPE)) {
		pr_info("oplus_ir_core:wl2868c disable seq type EXT_LDO5");
		wl2868c_ldo_disable(EXT_LDO5, 0);
	} else {
		pr_info("oplus_ir_core: wl2868c ERROR status\n");
	}
#endif
}

static u32* append_pulse(struct ir_convert_data_t *convert_data, enum ir_interface hw_intf)
{
	u8 b = 0;
	u32 left_bits = 0;
	u32 mask_bits = 0;
	u32 i = 0;
	static u32 s_rets[IR_POS_NUM] = {0, 0};

	while (convert_data->bit_num > 0) {
		if (convert_data->byte_pos >= convert_data->hw_data_num) {
			s_rets[IR_BYTE_POS_INDEX] = convert_data->byte_pos;
			s_rets[1] = convert_data->bit_pos;
			convert_data->pos_record[IR_BYTE_POS_INDEX] = s_rets[IR_BYTE_POS_INDEX];
			convert_data->pos_record[IR_BIT_POS_INDEX] = s_rets[IR_BIT_POS_INDEX];
			return(s_rets);
		}

		b = 0;
		if (convert_data->bit_pos > 0) {
			b = convert_data->hw_data[convert_data->byte_pos];
		}

		left_bits	= BITS_PER_BYTE - (convert_data->bit_pos);
		mask_bits	= (left_bits < convert_data->bit_num) ? left_bits : convert_data->bit_num;
		left_bits	= BITS_PER_BYTE - mask_bits - convert_data->bit_pos;
		if (convert_data->high == true) {
			for (i = 0; i < mask_bits; i++) {
				if (hw_intf == IR_HW_SPI) {
					b |= (1 << (left_bits + i));
				} else {
					if (mask_bits < BITS_PER_BYTE) {
						b |= (1 << (BITS_PER_BYTE - left_bits - mask_bits + i));
					} else {
						b |= (1 << (left_bits + i));
					}
				}
			}
		}

		convert_data->hw_data[convert_data->byte_pos] = b;
		convert_data->bit_pos += mask_bits;
		convert_data->bit_pos %= BITS_PER_BYTE;
		if (convert_data->bit_pos == 0) {
		  convert_data->byte_pos++;
		}

		convert_data->bit_num -= mask_bits;
	}

	s_rets[IR_BYTE_POS_INDEX] = convert_data->byte_pos;
	s_rets[IR_BIT_POS_INDEX] = convert_data->bit_pos;
	convert_data->pos_record[IR_BYTE_POS_INDEX] = s_rets[IR_BYTE_POS_INDEX];
	convert_data->pos_record[IR_BIT_POS_INDEX] = s_rets[IR_BIT_POS_INDEX];
	return(s_rets);
}

static u32* append_pulse_hl(struct ir_convert_hl_data_t *convert_hl_data, enum ir_interface hw_intf)
{
	u32 *rets = NULL;
	u32 pos_record[IR_POS_NUM] = {0, 0};
	struct ir_convert_data_t convert_data;

	if (convert_hl_data->high == true) {
		convert_data.hw_data = convert_hl_data->hw_data;
		convert_data.hw_data_num = convert_hl_data->hw_data_num;
		convert_data.byte_pos = convert_hl_data->byte_pos;
		convert_data.bit_pos = convert_hl_data->bit_pos;
		convert_data.bit_num = convert_hl_data->hw_data_high_num;
		convert_data.high = true;
		convert_data.pos_record = pos_record;
		rets = append_pulse(&convert_data, hw_intf);

		convert_data.hw_data = convert_hl_data->hw_data;
		convert_data.hw_data_num = convert_hl_data->hw_data_num;
		convert_data.byte_pos = pos_record[IR_BYTE_POS_INDEX];
		convert_data.bit_pos = pos_record[IR_BIT_POS_INDEX];
		convert_data.bit_num = convert_hl_data->hw_data_low_num;
		convert_data.high = false;
		convert_data.pos_record = pos_record;
		rets = append_pulse(&convert_data, hw_intf);
	} else {
		convert_data.hw_data = convert_hl_data->hw_data;
		convert_data.hw_data_num = convert_hl_data->hw_data_num;
		convert_data.byte_pos = convert_hl_data->byte_pos;
		convert_data.bit_pos = convert_hl_data->bit_pos;
		convert_data.bit_num = convert_hl_data->hw_data_high_num + convert_hl_data->hw_data_low_num;
		convert_data.high = false;
		convert_data.pos_record = pos_record;
		rets = append_pulse(&convert_data, hw_intf);
	}

	return rets;
}

int oplus_ir_to_hw_data(struct ir_to_hw_t *ir_to_hw_config, enum ir_interface hw_intf)
{
	u8 divide = 0;
	u32 bit_pos = 0;
	u32 byte_pos = 0;
	u32 i = 0;
	u32 j = 0;
	u32 ir_wave_num = 0;
	u32 add_hw_low_num = 0;
	u32 *rets = NULL;
	u32 high_hw_num_per_ir = 0;
	u32 low_hw_num_per_ir = 0;
	u32 hw_num_per_ir_int = 0;

	u64 p = 0;
	u64 left_time = 0;
	u64 hw_wave_len_x_gain = 0;
	u64 ir_wave_len_x_gain = 0;
	u64 hw_num_per_ir_double = 0;

	u32 pos_record[IR_POS_NUM] = {0, 0};

	struct ir_convert_data_t convert_data;
	struct ir_convert_hl_data_t convert_hl_data;

	if (!ir_to_hw_config) {
		pr_info("oplus_ir_core: parse_hw_config error, ir_to_hw_config is null!\n");
		return -EINVAL;
	}

	hw_wave_len_x_gain = (u64)((u64)ONESECOND*(u64)IR_CALCUL_WAVE_GAIN / (u64)ir_to_hw_config->hw_frequency);
	ir_wave_len_x_gain = (u64)((u64)ONESECOND*(u64)IR_CALCUL_WAVE_GAIN / (u64)ir_to_hw_config->ir_frequency);
	hw_num_per_ir_double = ir_wave_len_x_gain * IR_CALCUL_WAVE_GAIN / hw_wave_len_x_gain;

	if (hw_intf == IR_HW_PWM) {
		hw_num_per_ir_int = ir_to_hw_config->duty_cycle;
	} else {
		hw_num_per_ir_int = (u32) ((hw_num_per_ir_double + (IR_CALCUL_WAVE_GAIN/2)) / IR_CALCUL_WAVE_GAIN);
	}

	ir_wave_len_x_gain = hw_num_per_ir_int * hw_wave_len_x_gain;

	high_hw_num_per_ir = hw_num_per_ir_int / ir_to_hw_config->duty_cycle;
	low_hw_num_per_ir = hw_num_per_ir_int - high_hw_num_per_ir;

	bit_pos = 0;
	byte_pos = 0;
	left_time = 0;
	for (i = 0; i < ir_to_hw_config->pattern_length; i++) {
		if (i % IR_HIGHT_HW_DATA_INDEX == 0) {
			p = (u64)ir_to_hw_config->pattern[i] * IR_CALCUL_WAVE_GAIN;
			divide = true;
		} else {
			p = (u64)ir_to_hw_config->pattern[i] * IR_CALCUL_WAVE_GAIN + left_time;
			left_time = 0;
			divide = false;
		}

		ir_wave_num = (u32) (p / ir_wave_len_x_gain);
		for (j = 0; j < ir_wave_num; j++) {
			convert_hl_data.hw_data = ir_to_hw_config->hw_data;
			convert_hl_data.hw_data_num = ir_to_hw_config->hw_data_num;
			convert_hl_data.byte_pos = byte_pos;
			convert_hl_data.bit_pos = bit_pos;
			convert_hl_data.hw_data_high_num = high_hw_num_per_ir;
			convert_hl_data.hw_data_low_num = low_hw_num_per_ir;
			convert_hl_data.high = divide;
			rets = append_pulse_hl(&convert_hl_data, hw_intf);
			byte_pos = rets[IR_BYTE_POS_INDEX];
			bit_pos = rets[IR_BIT_POS_INDEX];
		}

		left_time += p - (u64)ir_wave_num * ir_wave_len_x_gain;
		if (i % IR_HIGHT_HW_DATA_INDEX != 0) {
			add_hw_low_num = (u32) (left_time / hw_wave_len_x_gain);
			if (add_hw_low_num > 0) {
				convert_data.hw_data = ir_to_hw_config->hw_data;
				convert_data.hw_data_num = ir_to_hw_config->hw_data_num;
				convert_data.byte_pos = byte_pos;
				convert_data.bit_pos = bit_pos;
				convert_data.bit_num = add_hw_low_num;
				convert_data.high = false;
				convert_data.pos_record = pos_record;
				rets = append_pulse(&convert_data, hw_intf);
				byte_pos = rets[IR_BYTE_POS_INDEX];
				bit_pos = rets[IR_BIT_POS_INDEX];
				left_time -= (u64)add_hw_low_num * hw_wave_len_x_gain;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(oplus_ir_to_hw_data);

static int ir_file_open(struct inode *inode, struct file *file)
{
	file->private_data = g_ir;

	return 0;
}

static int ir_file_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long ir_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ir_core *ir = (struct ir_core *)filp->private_data;
	int ret = 0;
	struct pattern_params header = {0};
	struct pattern_params *params = NULL;
	int param_size = 0;

	pr_info("oplus_ir_core: cmd:0x%x\n", cmd);

	switch(cmd) {
	case IR_GET_INF:
		if (copy_to_user((void __user *)arg, &ir->inf, sizeof(ir->inf))) {
			pr_err("oplus_ir_core: Failed to copy data to user\n");
			return -EFAULT;
		}
		break;
	case IR_SEND_PATTERN:
		if (copy_from_user(&header, (void __user *)arg, sizeof(struct pattern_params))) {
			pr_err("oplus_ir_core: Failed to copy data from user\n");
			return -EFAULT;
		}
		if (header.size > IR_PARAM_MAX_SIZE) {
			pr_err("oplus_ir_core: header.size too large!\n");
			return -ENOMEM;
		}
		param_size = sizeof(header) + header.size * sizeof(int);
		if (param_size > (IR_PARAM_MAX_SIZE * sizeof(int) + sizeof(header))) {
			pr_err("oplus_ir_core: param_size too large!\n");
			return -ENOMEM;
		}
		params = vzalloc(param_size);
		if (!params) {
			pr_err("oplus_ir_core: Failed to allocate memory\n");
			return -ENOMEM;
		}

		if (copy_from_user(params, (void __user *)arg, param_size)) {
			vfree(params);
			return -EFAULT;
		}

		mutex_lock(&ir->tx_mutex);
		if ((ir->inf == IR_HW_SPI) || (ir->inf == IR_HW_PWM)) {
			enable_ir_vdd(ir);
		}

		ret = ir->ops->send(ir->priv, params, ir->inf);
		if (ret < 0) {
			pr_err("oplus_ir_core: Failed to send pattern\n");
		}

		if ((ir->inf == IR_HW_SPI) || (ir->inf == IR_HW_PWM)) {
			disable_ir_vdd(ir);
		}
		mutex_unlock(&ir->tx_mutex);

		vfree(params);
		break;
	default:
		pr_err("oplus_ir_core:unkown cmd\n");
		break;
	}

	return 0;
}

static struct file_operations misc_fops = {
	.open = ir_file_open,
	.release = ir_file_close,
	.unlocked_ioctl = ir_unlocked_ioctl,
};

static void ir_assert_ops(struct device_ops *ops)
{
	assert(!IS_ERR_OR_NULL(ops->send));
}

int ir_register_device(struct device_ops *ops, enum ir_interface inf, void *priv, char *name)
{
	struct ir_core *ir = g_ir;

	if (!ir) {
		return -EPROBE_DEFER;
	}

	if (ir->inf != IR_HW_UNKOWN) {
		pr_err("Already registered, return\n");
		return -EBUSY;
	}

	ir_assert_ops(ops);

	ir->inf = inf;
	ir->ops = ops;
	ir->name = name;
	ir->priv = priv;

	pr_info("%s ir registered, inf %d\n", name, inf);
	return 0;
}
EXPORT_SYMBOL(ir_register_device);

static int ir_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ir_core *ir = NULL;

	ir = (struct ir_core *)kzalloc(sizeof(struct ir_core), GFP_KERNEL);
	if (!ir) {
		pr_info("Failed to alloc memory \n");
		return -ENOMEM;
	}

	ir->dev = pdev;
	platform_set_drvdata(pdev, ir);

	ir->misc_dev.fops = &misc_fops;
	ir->misc_dev.name = OPLUS_CONSUMER_IR_DEVICE_NAME;
	ir->misc_dev.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&ir->misc_dev);
	if (ret < 0) {
		kfree(ir);
		pr_err("Failed to register misc device");
		return -ENODEV;
	}

	/*set to interface to unkown, means not initailized*/
	ir->inf = IR_HW_UNKOWN;

	g_ir = ir;
	mutex_init(&g_ir->tx_mutex);
	if (parse_hw_core_config(&pdev->dev, g_ir) < 0) {
		pr_err("Failed to get dts!");
		return -ENOMEM;
	}

	return 0;
}

static int ir_core_remove(struct platform_device *pdev)
{
	struct ir_core *ir = platform_get_drvdata(pdev);

	if (ir) {
		misc_deregister(&ir->misc_dev);
		kfree(ir);
	}

	return 0;
}

static struct of_device_id ir_core_of_match_table[] = {
	{
		.compatible = "oplus,kookong_ir_core",/*"kookong,ir-core"*/
	},
	{},
};

static struct platform_driver ir_core_driver_platform = {
	.driver = {
		.name = CORE_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ir_core_of_match_table,
	},
	.probe = ir_core_probe,
	.remove = ir_core_remove,
};

module_platform_driver(ir_core_driver_platform);

MODULE_AUTHOR("oplus, Inc.");
MODULE_DESCRIPTION("Oplus Ir Core Module");
MODULE_LICENSE("GPL");
