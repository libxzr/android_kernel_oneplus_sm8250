/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "<sensor_event> " fmt

#include "oplus_sensor_event.h"
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

struct oplus_sensor_event_client {
    spinlock_t buffer_lock;
    unsigned int head;
    unsigned int tail;
    unsigned int bufsize;
    unsigned int buffull;
    struct oplus_sensor_event *buffer;
    wait_queue_head_t wait;
};
struct oplus_sensor_event_obj {
    struct oplus_sensor_event_client client[MINOR_NUM_MAX + 1];
};
static struct oplus_sensor_event_obj *event_obj;
static struct lock_class_key buffer_lock_key[MINOR_NUM_MAX + 1];
/*
 * sensor_input_event only support process context.
 */
int oplus_sensor_input_event(unsigned char handle, const struct oplus_sensor_event *event)
{
    struct oplus_sensor_event_client *client = &event_obj->client[handle];
    unsigned int dummy = 0;

    /* spin_lock safe, this function don't support interrupt context */
    spin_lock(&client->buffer_lock);
    /*
     * Reserve below log if need debug LockProve
     * SE_ERR("[Lomen]sensor_input_event: printf key handle ID=%d, key
     * addr=%p\n",
     * handle, (struct
     * lock_class_key*)client->buffer_lock.rlock.dep_map.key);
     */
    if (unlikely(client->buffull == true)) {
        pr_err_ratelimited(
            "input buffull, handle:%d, head:%d, tail:%d\n", handle,
            client->head, client->tail);
        spin_unlock(&client->buffer_lock);
        wake_up_interruptible(&client->wait);
        return -1;
    }
    client->buffer[client->head++] = *event;
    client->head &= client->bufsize - 1;
    /* remain 1 count */
    dummy = client->head + 1;
    dummy &= client->bufsize - 1;
    if (unlikely(dummy == client->tail))
        client->buffull = true;
    spin_unlock(&client->buffer_lock);

    wake_up_interruptible(&client->wait);
    return 0;
}

static int oplus_sensor_event_fetch_next(struct oplus_sensor_event_client *client,
                   struct oplus_sensor_event *event)
{
    int have_event;
    /*
     * spin_lock safe, sensor_input_event always in process context.
     */
    spin_lock(&client->buffer_lock);

    have_event = client->head != client->tail;
    if (have_event) {
        *event = client->buffer[client->tail++];
        client->tail &= client->bufsize - 1;
        client->buffull = false;
    }

    spin_unlock(&client->buffer_lock);

    return have_event;
}

ssize_t oplus_sensor_event_read(unsigned char handle, struct file *file,
              char __user *buffer, size_t count, loff_t *ppos)
{
    struct oplus_sensor_event_client *client = &event_obj->client[handle];
    struct oplus_sensor_event event;
    size_t read = 0;

    if (count != 0 && count < sizeof(struct oplus_sensor_event)) {
        pr_err("%s handle: %d err count(%d)\n", __func__,
              handle, (int)count);
        return -EINVAL;
    }

    for (;;) {
        if (client->head == client->tail)
            return 0;
        if (count == 0) {
            pr_debug("%s count: %d\n", __func__, (int)count);
            break;
        }

        while (read + sizeof(struct oplus_sensor_event) <= count &&
               oplus_sensor_event_fetch_next(client, &event)) {

            if (copy_to_user(buffer + read, &event,
                     sizeof(struct oplus_sensor_event)))
                return -EFAULT;

            read += sizeof(struct oplus_sensor_event);
        }

        if (read)
            break;
    }

    return read;
}

unsigned int oplus_sensor_event_poll(unsigned char handle, struct file *file,
                   poll_table *wait)
{
    struct oplus_sensor_event_client *client = &event_obj->client[handle];
    unsigned int mask = 0;

    poll_wait(file, &client->wait, wait);

    if (client->head != client->tail) {
        /* pr_err("sensor_event_poll handle:%d\n", handle); */
        mask |= POLLIN | POLLRDNORM;
    }

    return mask;
}
unsigned int oplus_sensor_event_register(unsigned char handle)
{
    struct  oplus_sensor_event_obj *obj = event_obj;

    spin_lock_init(&obj->client[handle].buffer_lock);
    lockdep_set_class(&obj->client[handle].buffer_lock,
              &buffer_lock_key[handle]);

    obj->client[handle].head = 0;
    obj->client[handle].tail = 0;
    obj->client[handle].bufsize = OTHER_SENSOR_BUF_SIZE;
    obj->client[handle].buffull = false;
    obj->client[handle].buffer =
        vzalloc(obj->client[handle].bufsize *
            sizeof(struct oplus_sensor_event));
    if (!obj->client[handle].buffer) {
        pr_err("Alloc ringbuffer error!\n");
        return -1;
    }
    init_waitqueue_head(&obj->client[handle].wait);
    return 0;
}
unsigned int oplus_sensor_event_deregister(unsigned char handle)
{
    struct oplus_sensor_event_obj *obj = event_obj;

    vfree(obj->client[handle].buffer);
    return 0;
}

static int __init oplus_sensor_event_entry(void)
{
    struct oplus_sensor_event_obj *obj =
        kzalloc(sizeof(struct oplus_sensor_event_obj), GFP_KERNEL);

    event_obj = obj;
    return 0;
}
subsys_initcall(oplus_sensor_event_entry);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sensor event driver");
MODULE_AUTHOR("Mediatek");
