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
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#include "oplus_ir_core.h"
#include "oplus_ir_uart.h"


#define IR_SENDDATA_START    0
#define IR_SENDDATA_END      1

struct hw_uart_config_t {
	int ir_power_ctrl;
	struct pinctrl *pinctrl;
	struct pinctrl_state *ir_send_data_enable;
	struct pinctrl_state *ir_send_data_disable;
};

struct oplus_ir_uart_t {
	struct device *dev;
	struct hw_uart_config_t hw_config;
};

static int parse_hw_uart_config(struct device *dev, struct oplus_ir_uart_t *ir_uart)
{
	int retval = 0;
	struct device_node *np = dev->of_node;

	if (!ir_uart) {
		pr_err("oplus_ir_uart: parse_hw_config error, ir_uart is null!\n");
	} else {
		ir_uart->hw_config.pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR_OR_NULL(ir_uart->hw_config.pinctrl)) {
			pr_info("oplus_ir_uart: falied to get pinctrl!\n");
			return -EINVAL;
		} else {
			ir_uart->hw_config.ir_send_data_enable = pinctrl_lookup_state(ir_uart->hw_config.pinctrl, "ir_data_enable");
			if (IS_ERR_OR_NULL(ir_uart->hw_config.ir_send_data_enable)) {
				pr_err("oplus_ir_uart: %s, failed to request ir_data_enable.\n", __func__);
				return -EINVAL;
			}

			ir_uart->hw_config.ir_send_data_disable = pinctrl_lookup_state(ir_uart->hw_config.pinctrl, "ir_data_disable");
			if (IS_ERR_OR_NULL(ir_uart->hw_config.ir_send_data_disable)) {
				pr_err("oplus_ir_uart: %s, failed to request ir_data_disable.\n", __func__);
				return -EINVAL;
			}
			pinctrl_select_state(ir_uart->hw_config.pinctrl, ir_uart->hw_config.ir_send_data_disable);
		}

		retval = of_get_named_gpio(np, "ir-power-ctrl", 0);
		if (retval < 0) {
			pr_info("oplus_ir_uart: falied to get ir-power-ctrl\n");
			return retval;
		} else {
			ir_uart->hw_config.ir_power_ctrl = retval;
			retval = devm_gpio_request(dev, ir_uart->hw_config.ir_power_ctrl, "ir-power-ctrl");
			if (retval) {
				pr_err("oplus_ir_uart: %s, failed to request ir-power-ctrl, ret = %d.\n", __func__, retval);
			}
		}
	}

	return 0;
}

int ir_uart_file_write(void *priv, struct pattern_params *param, enum ir_interface inf)
{
	struct oplus_ir_uart_t *ir_uart = (struct oplus_ir_uart_t*)priv;

	pr_info("oplus_ir_uart: oplus_ir_uart_file_write call\n");
	if (!ir_uart) {
		return 0;
	}

	if (param->pattern[0] == IR_SENDDATA_START) {
		pr_info("oplus_ir_uart: oplus_ir_uart_open enable power and sendData!\n");
		gpio_direction_output(ir_uart->hw_config.ir_power_ctrl, 1);
		pinctrl_select_state(ir_uart->hw_config.pinctrl, ir_uart->hw_config.ir_send_data_enable);
		mdelay(2);
	} else if (param->pattern[0] == IR_SENDDATA_END) {
		pr_info("oplus_ir_uart: oplus_ir_uart_open disable power and sendData!\n");
		mdelay(2);
		gpio_direction_output(ir_uart->hw_config.ir_power_ctrl, 0);
		pinctrl_select_state(ir_uart->hw_config.pinctrl, ir_uart->hw_config.ir_send_data_disable);
	}

	return 0;
}

static struct device_ops uart_ops = {
	.send = ir_uart_file_write,
};

static int oplus_ir_uart_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct oplus_ir_uart_t *ir_uart = NULL;

	pr_info("oplus_ir_uart: oplus_ir_uart_probe call\n");

	ir_uart = kzalloc(sizeof(*ir_uart), GFP_KERNEL);
	if (!ir_uart) {
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ir_uart);

	ir_uart->dev = &pdev->dev;
	ret = ir_register_device(&uart_ops, IR_HW_UART, ir_uart, "UART");
	if (ret < 0) {
		pr_err("oplus_ir_uart: Failed to regiser ir device\n");
		goto end;
	}
	parse_hw_uart_config(&pdev->dev, ir_uart);

end:
	return 0;
}

static int oplus_ir_uart_remove(struct platform_device *pdev)
{
	struct oplus_ir_uart_t *pwm_ir = platform_get_drvdata(pdev);

	pr_info("oplus_ir_uart: oplus_ir_uart_remove call\n");
	kfree(pwm_ir);
	return 0;
}

static struct of_device_id oplus_ir_uart_of_match_table[] = {
	{
		.compatible = "oplus,kookong_ir_uart",/*"kookong,ir-uart",*/
	},
	{},
};
MODULE_DEVICE_TABLE(of, oplus_ir_uart_of_match_table);


static struct platform_driver oplus_ir_uart_driver = {
	.driver = {
		.name = UART_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = oplus_ir_uart_of_match_table,
	},
	.probe = oplus_ir_uart_probe,
	.remove = oplus_ir_uart_remove,
};

static int __init oplus_ir_uart_init(void)
{
	pr_info("oplus_ir_uart_init call\n");

	platform_driver_register(&oplus_ir_uart_driver);
	return 0;
}


arch_initcall(oplus_ir_uart_init);

MODULE_AUTHOR("oplus, Inc.");
MODULE_DESCRIPTION("oplus UART Bus Module");
MODULE_LICENSE("GPL");
