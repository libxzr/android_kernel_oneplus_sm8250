/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef _SENSOR_ATTR_H
#define _SENSOR_ATTR_H
#include <linux/major.h>
#include <linux/types.h>

struct oplus_sensor_attr_t  {
    unsigned char minor;
    const char *name;
    const struct file_operations *fops;
    struct list_head list;
    struct device *parent;
    struct device *this_device;
};
struct oplus_sensor_attr_dev {
    struct device *dev;
};
extern int oplus_sensor_attr_register(struct oplus_sensor_attr_t *misc);
extern int oplus_sensor_attr_deregister(struct oplus_sensor_attr_t *misc);

#endif
