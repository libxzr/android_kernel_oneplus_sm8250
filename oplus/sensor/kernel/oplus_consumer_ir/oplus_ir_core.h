/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef __OPLUS_IR_CORE_H__
#define __OPLUS_IR_CORE_H__

#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>

#define IR_CALCUL_WAVE_GAIN       100000
#define ONESECOND                 1000000
#define ONESECOND_X100000         100000000000
#define IR_DEFAULT_VDD_MIN_VOL    2950000
#define IR_DEFAULT_VDD_MAX_VOL    3050000
#define IR_DEFAULT_DUTY_CYCLE     3
#define CORE_MODULE_NAME "oplus_kookong_ir_core"
#define OPLUS_CONSUMER_IR_DEVICE_NAME "oplus_consumer_ir"

#define IR_IOCTL_GROUP 0xE5
#define IR_GET_INF 	_IO(IR_IOCTL_GROUP, 0x01)
#define IR_SEND_PATTERN           _IO(IR_IOCTL_GROUP, 0x02)
#define IR_START                  IO(IR_IOCTL_GROUP, 0x03)
#define IR_SET_PARAM              _IO(IR_IOCTL_GROUP, 0x03)

#ifdef assert
#undef assert
#endif
#define assert(expression) {\
	if (!expression) {\
		(void)panic(\
			"assertion %s,%d"\
			__FILE__, __LINE__);\
	}\
}

enum ir_interface {
	IR_HW_SPI,
	IR_HW_UART,
	IR_HW_PWM,
	IR_HW_UNKOWN
};

struct ir_to_hw_t {
	u32 duty_cycle; /* 1/2 vule is 2, 1/3 value is 3 */
	u8* hw_data;
	u32 hw_data_num;
	u32 hw_frequency;
	u32 ir_frequency;
	int *pattern;
	u32 pattern_length;
};

struct pattern_params {
	int32_t carrier_freq;
	uint32_t size;
	uint32_t pattern[];
};

struct device_ops {
	int (*send)(void *priv, struct pattern_params *param, enum ir_interface inf);
};

int oplus_ir_to_hw_data(struct ir_to_hw_t *ir_to_hw_config, enum ir_interface hw_intf);
int ir_register_device(struct device_ops *ops, enum ir_interface inf, void *priv, char *name);

#endif/*__OPLUS_IR_CORE_H__*/
