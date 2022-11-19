// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "oplus_ir_core.h"
#include "oplus_ir_spi.h"

#define SPI_IR_MAX_SIZE            4096*1024 - 1 /*4M max size*/
#define KOOKONG_IR_MIN_SIZE        8u
#define ONESECOND                  1000000
#define SPIFREQUENCY               5000000
#define SPI_FREQ_MIN               999999
#define SPI_FREQ_MAX               10000001
#define SPI_DEFAULT_FREQ           5000000
#define BITS_PER_BYTE              8

struct hw_spi_config_t {
	u32 duty_cycle; /* 1/2 vule is 2, 1/3 value is 3 */
	int spi_frep;
	int spi_mode;
};

struct oplus_ir_spi_t {
	u8 *tx_buf;
	struct page *page;
	struct spi_device *spi;
	struct spi_transfer spi_tx_xfer;
	struct hw_spi_config_t hw_config;
};

static int parse_hw_spi_config(struct device *dev, struct oplus_ir_spi_t *spi_ir)
{
	int retval = 0;
	u32 value = 0;
	struct device_node *np = dev->of_node;

	if (!spi_ir) {
		pr_err("oplus_ir: parse_hw_config error, spi_ir is null!\n");
		return -EPERM;
	} else {
		retval = of_property_read_u32(np, "spi-max-frequency", &value);
		if (retval < 0) {
			spi_ir->hw_config.spi_frep = SPI_DEFAULT_FREQ;
		} else {
			spi_ir->hw_config.spi_frep = value;
		}

		retval = of_property_read_u32(np, "ir-spi-mode", &value);
		if (retval < 0) {
			spi_ir->hw_config.spi_mode = SPI_MODE_0;
		} else {
			spi_ir->hw_config.spi_mode = value;
		}

		retval = of_property_read_u32(np, "duty-cycle", &value);
		if (retval < 0) {
			spi_ir->hw_config.duty_cycle = IR_DEFAULT_DUTY_CYCLE;
		} else {
			spi_ir->hw_config.duty_cycle = value;
		}


		pr_info("oplus_spi_ir: %s: spi-max-frequency is %u\n", __func__, spi_ir->hw_config.spi_frep);
		pr_info("oplus_spi_ir: %s: ir-spi-mode is %u\n", __func__, spi_ir->hw_config.spi_mode);
		pr_info("oplus_spi_ir: %s: duty_cycle is %u\n", __func__, spi_ir->hw_config.duty_cycle);
	}

	return 0;
}

static int oplus_ir_spi_write(struct oplus_ir_spi_t *spidev, unsigned int length)
{
	int retval = 0;
	struct spi_message msg;

	if (!spidev) {
		return -EINVAL;
	}

	spi_message_init(&msg);
	spidev->spi_tx_xfer.len = length;
	spidev->spi_tx_xfer.tx_buf = spidev->tx_buf;
	spidev->spi_tx_xfer.speed_hz = spidev->hw_config.spi_frep;
	spi_message_add_tail(&spidev->spi_tx_xfer, &msg);
	spidev->spi->mode = spidev->hw_config.spi_mode;
	retval = spi_sync(spidev->spi, &msg);
	if (retval == 0)
		retval = length;
	else {
		pr_err("oplus_spi_ir: Failed to complete SPI transfer, error = %d\n", retval);
	}

	return retval;
}

int ir_spi_file_write(void *priv, struct pattern_params *param, enum ir_interface inf)
{
	int retval = 0, i = 0;
	u32 order = 0;
	u32	spis_num = 0;
	u32	spi_total_num = 0;
	u64	spi_wave_len = 0;
	u32	spi_frequency = SPIFREQUENCY;
	u32	total_time = 0;
	struct ir_to_hw_t ir_to_hw;
	struct oplus_ir_spi_t *ir_spi = (struct oplus_ir_spi_t*)priv;

	if ((!ir_spi) || (!param)) {
		return -EINVAL;
	}

	for (i = 0; i < param->size; i++) {
		total_time += param->pattern[i];
	}

	if ((ir_spi->hw_config.spi_frep > SPI_FREQ_MIN) && (ir_spi->hw_config.spi_frep < SPI_FREQ_MAX)) {
		spi_frequency = ir_spi->hw_config.spi_frep;
	} else {
		spi_frequency = SPIFREQUENCY;
		ir_spi->hw_config.spi_frep = SPIFREQUENCY;
	}
	spi_wave_len = ONESECOND_X100000 / (u64) spi_frequency;

	spi_total_num = (u32) (((u64)total_time * IR_CALCUL_WAVE_GAIN) / spi_wave_len);

	spis_num = spi_total_num / BITS_PER_BYTE + BITS_PER_BYTE;

	/*for ir debug*/
	/*for (i = 0; i < param->size; i ++) {
		pr_info("oplus_pwm_ir:kookong_ir_file_write pattern[%d] = %d\n", i, param->pattern[i]);
	}
	pr_info("oplus_ir_spi: transmit for %d uS at %d Hz", total_time, param->carrier_freq);
	pr_info("oplus_ir_spi: spi_total_num=%d\n", spi_total_num);
	pr_info("oplus_ir_spi: spi_wave_len=%d\n", spi_wave_len);
	pr_info("oplus_ir_spi: spis_num=%d\n", spis_num);*/

	if (spis_num > SPI_IR_MAX_SIZE) {
		pr_err("oplus_ir_spi: spis_num is too large!\n");
		return -ELNRNG;
	}

	order = get_order(spis_num);
	ir_spi->page = alloc_pages(GFP_KERNEL, order);
	if (!ir_spi->page) {
		pr_err("oplus_ir_spi: Failed to alloc_pages!\n");
		return -ENOMEM;
	}
	ir_spi->tx_buf = (u8*)page_address(ir_spi->page);

	ir_to_hw.duty_cycle = ir_spi->hw_config.duty_cycle;
	ir_to_hw.hw_data = ir_spi->tx_buf;
	ir_to_hw.hw_data_num = spis_num;
	ir_to_hw.hw_frequency = spi_frequency;
	ir_to_hw.ir_frequency = param->carrier_freq;
	ir_to_hw.pattern = &param->pattern[0];
	ir_to_hw.pattern_length = param->size;
	if (oplus_ir_to_hw_data(&ir_to_hw, IR_HW_SPI) < 0) {
		pr_info("oplus_ir_to_hw_data error! = %d\n");
		retval = -EINVAL;
		goto extit;
	}

	retval = oplus_ir_spi_write(ir_spi, spis_num);

	/*for ir debug*/
	/*if (spis_num != 0) {
		pr_info("oplus_ir_spi data count is = %d\n", spis_num);
		for(i = 0; i < spis_num; i ++) {
			pr_info("oplus_ir_spi data[%d] = %d\n", i, ir_spi->tx_buf[i]);
		}
	}*/

extit:
	__free_pages(ir_spi->page, order);
	return retval;
}

static struct device_ops spi_ops = {
	.send = ir_spi_file_write,
};

static int oplus_ir_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	struct oplus_ir_spi_t *oplus_ir_spi_data = NULL;

	pr_info("oplus_ir_spi: spi_probe call\n");

	oplus_ir_spi_data = kzalloc(sizeof(*oplus_ir_spi_data), GFP_KERNEL);
	if (!oplus_ir_spi_data) {
		pr_err("oplus_ir_spi: oplus_ir_spi_data is NULL!\n");
		return -ENOMEM;
	}

	oplus_ir_spi_data->spi = spi;

	ret = ir_register_device(&spi_ops, IR_HW_SPI, oplus_ir_spi_data, "SPI");
	if (ret < 0) {
		pr_err("oplus_ir_spi: Failed to regiser ir device\n");
		goto end;
	}
	parse_hw_spi_config(&spi->dev, oplus_ir_spi_data);

	spi_set_drvdata(spi, oplus_ir_spi_data);

end:
	return ret;
}

static int oplus_ir_spi_remove(struct spi_device *spi)
{
	struct oplus_ir_spi_t *oplus_ir_data = spi_get_drvdata(spi);
	kfree(oplus_ir_data);
	return 0;
}

static const struct spi_device_id oplus_ir_spi_id_table[] = {
	{SPI_MODULE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(spi, oplus_ir_spi_id_table);

static struct of_device_id oplus_spi_ir_of_match_table[] = {
	{
		.compatible = "oplus,kookong_ir_spi",/*"kookong,ir-spi",*/
	},
	{},
};
MODULE_DEVICE_TABLE(of, oplus_spi_ir_of_match_table);


static struct spi_driver oplus_ir_spi_driver = {
	.driver = {
		.name = SPI_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = oplus_spi_ir_of_match_table,
	},
	.probe = oplus_ir_spi_probe,
	.remove = oplus_ir_spi_remove,
	.id_table = oplus_ir_spi_id_table,
};

module_spi_driver(oplus_ir_spi_driver);

MODULE_AUTHOR("oplus, Inc.");
MODULE_DESCRIPTION("oplus kookong SPI Bus Module");
MODULE_LICENSE("GPL");
