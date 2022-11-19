// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mt-plat/mtk_pwm.h>
#include <linux/dma-mapping.h>
#include <linux/pinctrl/consumer.h>

#include "oplus_ir_core.h"
#include "oplus_ir_pwm.h"


#define IR_PWM_SRC_CLOCK_BASE                26000000
#define IR_PWM_BASE_TOTAL_TIME               20000 /* (2^16 +1) * 8 * (1000000/26000000) us */
#define IR_PWM_MIN_FREQ         20000
#define IR_PWM_MAX_FREQ         60000
#define PWM_SCLK_SEL            1
#define PWM_DUTY_CYCLE_DEFAULT  2
#define IR_DEFAULT_PWM_NUM	    1
#define IR_TRAIL_0_VALUE_SIZE   4 * 256

struct hw_pwm_config_t {
	u32 pwm_num;    /*pwm number*/
	u32 duty_cycle; /* 1/2 vule is 2, 1/3 value is 3 */
};

struct opus_pwm_ir_t {
	u8 *tx_buf;
	u32* virt;
	dma_addr_t wave_phy;
	struct device *dev;
	struct pwm_spec_config pwm_setting;
	struct hw_pwm_config_t pwm_config;
};

static int parse_hw_pwm_config(struct device *dev, struct opus_pwm_ir_t *pwm_ir)
{
	int retval = 0;
	u32 value = 0;
	struct device_node *np = dev->of_node;

	if (!pwm_ir) {
		pr_err("oplus_ir_pwm: parse_hw_pwm_config error, pwm_ir is null!\n");
		return -EPERM;
	} else {
		retval = of_property_read_u32(np, "duty-cycle", &value);
		if (retval < 0) {
			pwm_ir->pwm_config.duty_cycle = IR_DEFAULT_DUTY_CYCLE;
		} else {
			pwm_ir->pwm_config.duty_cycle = value;
		}

		retval = of_property_read_u32(np, "pwm-num", &value);
		if (retval < 0) {
			pwm_ir->pwm_config.pwm_num = IR_DEFAULT_PWM_NUM;
		} else {
			pwm_ir->pwm_config.pwm_num = value;
		}
		retval = of_property_read_u32(np, "pwm-dma-mask", &value);
		if (retval < 0) {
			pr_info("oplus_ir_pwm: pwm not config dma mask!\n");
		} else {
			pr_info("oplus_ir_pwm: pwm-dma-mask value is: %u \n", value);
			if (dma_set_coherent_mask(dev, DMA_BIT_MASK(value))) {
				pr_err("oplus_ir_pwm: dma_set_mask fail\n");
			}
		}

		pr_info("oplus_ir_pwm: %s:pwm-num is %u\n", __func__, pwm_ir->pwm_config.pwm_num);
		pr_info("oplus_ir_pwm: %s: duty_cycle is %u\n", __func__, pwm_ir->pwm_config.duty_cycle);
	}

	return 0;
}

static int oplus_ir_pwm_start(struct opus_pwm_ir_t *pwm_ir, u32 pwm_buf0_size, u16 hd_uration, u16 ld_uration)
{
	int ret = 0;

	if (!pwm_ir) {
		return -EINVAL;
	}

	memset(&pwm_ir->pwm_setting, 0, sizeof(struct pwm_spec_config));
	pwm_ir->pwm_setting.pwm_no = pwm_ir->pwm_config.pwm_num;
	pwm_ir->pwm_setting.mode = PWM_MODE_MEMORY;
	pwm_ir->pwm_setting.clk_div = CLK_DIV1;
	pwm_ir->pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
	pwm_ir->pwm_setting.pmic_pad = 0;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.HDURATION = hd_uration;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.LDURATION = ld_uration;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.GDURATION = 0;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.BUF0_SIZE = pwm_buf0_size;
	pwm_ir->pwm_setting.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = pwm_ir->wave_phy;

	ret = pwm_set_spec_config(&pwm_ir->pwm_setting);
	if (ret) {
		pr_err("Fail to pwm_set_spec_config!\n");
	}

	return ret;
}


static int oplus_ir_pwm_stop(struct opus_pwm_ir_t *pwm_ir)
{
	if (!pwm_ir) {
		return -EINVAL;
	}

	mt_pwm_disable(pwm_ir->pwm_setting.pwm_no, pwm_ir->pwm_setting.pmic_pad);

	return 0;
}

int ir_pwm_file_write(void *priv, struct pattern_params *param, enum ir_interface inf)
{
	u16 hd_uration = 0;
	u16 ld_uration = 0;
	u32 pwm_buf0_size = 0;
	int ret = 0;
	int i = 0;
	u32 hw_data_num = 0;
	u32 hw_data_total_num = 0;
	u64 hw_wave_len = 0;
	u32 hw_frequency = 0;
	int carrier_freq = 0;
	u32 total_time = 0;
	struct ir_to_hw_t ir_to_hw;
	struct opus_pwm_ir_t *pwm_ir = (struct opus_pwm_ir_t*)priv;

	if (!pwm_ir) {
		return -EINVAL;
	}

	carrier_freq = param->carrier_freq;
	if ((carrier_freq < IR_PWM_MIN_FREQ) || (carrier_freq > IR_PWM_MAX_FREQ)) {
		return -ELNRNG;
	}

	for (i = 0; i < param->size; i++) {
		total_time += param->pattern[i];
	}

	pr_info("oplus_pwm_ir: transmit for %d uS at %d Hz", total_time, carrier_freq);

	hw_frequency = carrier_freq * IR_DEFAULT_DUTY_CYCLE;
	hd_uration = ((IR_PWM_SRC_CLOCK_BASE * 10) / hw_frequency + 5) / 10;

	hd_uration--;
	ld_uration = hd_uration,

	hw_frequency = ((IR_PWM_SRC_CLOCK_BASE * 10) / (hd_uration + 1) + 5) / 10;
	hw_wave_len = ONESECOND_X100000 / (u64) hw_frequency;
	hw_data_total_num = (u32) (((u64)total_time * IR_CALCUL_WAVE_GAIN) / hw_wave_len);

	hw_data_total_num = ALIGN(hw_data_total_num, BITS_PER_BYTE * sizeof(unsigned int));
	hw_data_num = (hw_data_total_num / BITS_PER_BYTE);
	pwm_buf0_size = hw_data_num;

	/*for ir debug*/
	/*for (i = 0; i < param->size; i ++) {
		pr_info("oplus_pwm_ir:ir_pwm_file_write pattern[%d] = %d\n", i, param->pattern[i]);
	}*/
	pr_info("oplus_pwm_ir: hwDataNum=%d, pwm_buf0_size=%d, hw_data_total_num=%d\n", hw_data_num, pwm_buf0_size, hw_data_total_num);
	pr_info("oplus_pwm_ir: pwm hd_uration = %d, hw_wave_len = %lld, hw_frequency = %d", hd_uration, hw_wave_len, hw_frequency);

	pwm_ir->virt = dma_alloc_coherent(pwm_ir->dev, (hw_data_num + IR_TRAIL_0_VALUE_SIZE), &pwm_ir->wave_phy, GFP_KERNEL);
	if (pwm_ir->virt == NULL) {
		pr_info("oplus_pwm_ir:dma_alloc_coherent error!\n");
		ret = -EINVAL;
		goto exit_free;
	}
	pwm_ir->tx_buf = (u8*)pwm_ir->virt;

	ir_to_hw.duty_cycle = IR_DEFAULT_DUTY_CYCLE;
	ir_to_hw.hw_data = pwm_ir->tx_buf;
	ir_to_hw.hw_data_num = hw_data_num;
	ir_to_hw.hw_frequency = hw_frequency;
	ir_to_hw.ir_frequency = hw_frequency / IR_DEFAULT_DUTY_CYCLE;
	ir_to_hw.pattern = &param->pattern[0];
	ir_to_hw.pattern_length = param->size;
	if (oplus_ir_to_hw_data(&ir_to_hw, IR_HW_PWM) < 0) {
		pr_info("oplus_pwm_ir:oplus_ir_to_hw_data error!\n");
		ret = -EINVAL;
		goto exit_free;
	}

	ret = oplus_ir_pwm_start(pwm_ir, pwm_buf0_size, hd_uration, ld_uration);

	usleep_range(total_time , total_time + 10);
	oplus_ir_pwm_stop(pwm_ir);

	/*for ir debug*/
	/*if (hw_data_num != 0) {
		pr_info("oplus_ir_pwm_file_write data count is = %d\n", hw_data_num);
		for(i = 0; i < hw_data_num; i ++) {
			pr_info("oplus_ir_pwm_file_write data[%d] = %d\n", i, pwm_ir->tx_buf[i]);
		}
	}*/


	if(ret != 0) {
		pr_info("oplus_pwm_ir:set pwm config failed\n");
		ret = -EINVAL;
		goto exit_free;
	}

exit_free:
	dma_free_coherent(pwm_ir->dev, (hw_data_num + IR_TRAIL_0_VALUE_SIZE), pwm_ir->virt, pwm_ir->wave_phy);
	return ret;
}


static struct device_ops pwm_ops = {
	.send = ir_pwm_file_write,
};

static int oplus_ir_pwm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct opus_pwm_ir_t *pwm_ir = NULL;

	pr_info("oplus_pwm_ir: oplus_ir_pwm_probe call\n");

	pwm_ir = kzalloc(sizeof(*pwm_ir), GFP_KERNEL);
	if (!pwm_ir) {
		pr_err("oplus_pwm_ir:pwm_ir is NULL!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, pwm_ir);

	pwm_ir->dev = &pdev->dev;
	ret = ir_register_device(&pwm_ops, IR_HW_PWM, pwm_ir, "PWM");
	if (ret < 0) {
		pr_err("oplus_pwm_ir:Failed to regiser ir device\n");
		goto end;
	}
	parse_hw_pwm_config(&pdev->dev, pwm_ir);

end:
	return ret;
}

static int oplus_ir_pwm_remove(struct platform_device *pdev)
{
	struct opus_pwm_ir_t *pwm_ir = platform_get_drvdata(pdev);

	pr_info("oplus_pwm_ir: kookong_ir_pwm_remove call\n");

	kfree(pwm_ir);
	return 0;
}

static struct of_device_id oplus_ir_pwm_of_match_table[] = {
	{
		.compatible = "oplus,kookong_ir_pwm",/*"kookong,ir-pwm",*/
	},
	{},
};
MODULE_DEVICE_TABLE(of, oplus_ir_pwm_of_match_table);


static struct platform_driver oplus_ir_pwm_driver = {
	.driver = {
		.name = PWM_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = oplus_ir_pwm_of_match_table,
	},
	.probe = oplus_ir_pwm_probe,
	.remove = oplus_ir_pwm_remove,
};

module_platform_driver(oplus_ir_pwm_driver);

MODULE_AUTHOR("oplus, Inc.");
MODULE_DESCRIPTION("oplus kookong PWM Bus Module");
MODULE_LICENSE("GPL");
