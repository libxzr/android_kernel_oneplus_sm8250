/* SPDX-License-Identifier: GPL-2.0-only  */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef __OPLUS_IR_SPI_H__
#define __OPLUS_IR_SPI_H__

#define SPI_MODULE_NAME "oplus_kookong_ir_spi"

ssize_t oplus_ir_spi_file_write(struct file *file, const char __user *ubuff, size_t count, loff_t *offset);

#endif/*__OPLUS_KOOKONG_IR_SPI_H__*/
