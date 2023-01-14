/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef __OPLUS_IR_UART_H__
#define __OPLUS_IR_UART_H__

#define UART_MODULE_NAME "oplus_kookong_ir_uart"

ssize_t oplus_ir_uart_file_write(struct file *file, const char __user *ubuff, size_t count, loff_t *offset);

#endif/*__OPLUS_IR_UART_H__*/
