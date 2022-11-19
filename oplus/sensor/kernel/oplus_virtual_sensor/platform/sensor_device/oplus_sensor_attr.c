/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "<HWMSEN> " fmt

#include "oplus_sensor_attr.h"
#include "oplus_sensor_event.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>

static int oplus_sensor_attr_major = -1;
static struct class *oplus_sensor_attr_class;

static LIST_HEAD(oplus_sensor_attr_list);
static DEFINE_MUTEX(oplus_sensor_attr_mtx);

static int oplus_sensor_attr_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    struct oplus_sensor_attr_t *c;
    int err = -ENODEV;
    const struct file_operations *new_fops = NULL;

    mutex_lock(&oplus_sensor_attr_mtx);

    list_for_each_entry(c, &oplus_sensor_attr_list, list) {
        if (c->minor == minor) {
            new_fops = fops_get(c->fops);
            break;
        }
    }

    if (!new_fops) {
        mutex_unlock(&oplus_sensor_attr_mtx);
        request_module("char-major-%d-%d", oplus_sensor_attr_major, minor);
        mutex_lock(&oplus_sensor_attr_mtx);

        list_for_each_entry(c, &oplus_sensor_attr_list, list) {
            if (c->minor == minor) {
                new_fops = fops_get(c->fops);
                break;
            }
        }
        if (!new_fops)
            goto fail;
    }

    err = 0;
    replace_fops(file, new_fops);
    if (file->f_op->open) {
        file->private_data = c;
        err = file->f_op->open(inode, file);
    }
fail:
    mutex_unlock(&oplus_sensor_attr_mtx);
    return err;
}

static const struct file_operations oplus_sensor_attr_fops = {
    .owner = THIS_MODULE,
    .open = oplus_sensor_attr_open,
};

int oplus_sensor_attr_register(struct oplus_sensor_attr_t *misc)
{
    dev_t dev;
    int err = 0;
    struct oplus_sensor_attr_t *c;

    mutex_lock(&oplus_sensor_attr_mtx);
    list_for_each_entry(c, &oplus_sensor_attr_list, list) {
        if (c->minor == misc->minor) {
            err = -EBUSY;
            goto out;
        }
    }
    dev = MKDEV(oplus_sensor_attr_major, misc->minor);
    misc->this_device = device_create(oplus_sensor_attr_class, misc->parent, dev,
                      misc, "%s", misc->name);
    if (IS_ERR(misc->this_device))
        goto out;
    list_add(&misc->list, &oplus_sensor_attr_list);
    mutex_unlock(&oplus_sensor_attr_mtx);
    err = oplus_sensor_event_register(misc->minor);
    return err;
out:
    mutex_unlock(&oplus_sensor_attr_mtx);
    return err;
}
int oplus_sensor_attr_deregister(struct oplus_sensor_attr_t *misc)
{
    if (WARN_ON(list_empty(&misc->list)))
        return -EINVAL;

    mutex_lock(&oplus_sensor_attr_mtx);
    list_del(&misc->list);
    device_destroy(oplus_sensor_attr_class,
               MKDEV(oplus_sensor_attr_major, misc->minor));
    mutex_unlock(&oplus_sensor_attr_mtx);
    oplus_sensor_event_deregister(misc->minor);
    return 0;
}
#if 0
static char *sensor_attr_devnode(struct device *dev, umode_t *mode)
{
    pr_debug("sensor_attr: name :%s\n", dev_name(dev));
    return kasprintf(GFP_KERNEL, "sensor/%s", dev_name(dev));
}
#endif
static int __init oplus_sensor_attr_init(void)
{
    int err;

    oplus_sensor_attr_class = class_create(THIS_MODULE, "oplus_sensor");
    err = PTR_ERR(oplus_sensor_attr_class);
    if (IS_ERR(oplus_sensor_attr_class)) {
        err = -EIO;
        return err;
    }
    oplus_sensor_attr_major = register_chrdev(0, "oplus_sensor", &oplus_sensor_attr_fops);
    if (oplus_sensor_attr_major < 0)
        goto fail_printk;
    /* sensor_attr_class->devnode = sensor_attr_devnode; */
    return 0;

fail_printk:
    pr_err("unable to get major %d for misc devices\n", oplus_sensor_attr_major);
    class_destroy(oplus_sensor_attr_class);
    return err;
}
subsys_initcall(oplus_sensor_attr_init);
