/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2021 Oplus. All rights reserved.
 */
#ifndef _OSVELTE_COMMON_H
#define _OSVELTE_COMMON_H

#include <asm/ioctls.h>

#define KMODULE_NAME "oplus_bsp_mm_osvelte"

#define DEV_NAME "osvelte"
#define DEV_PATH "/dev/" DEV_NAME

#define OSVELTE_LOG_TAG DEV_NAME

#define __COMMONIO 0xFA
#define CMD_OSVELTE_GET_VERSION _IO(__COMMONIO, 1) /* osvelte version */

#define OSVELTE_MAJOR		(0)
#define OSVELTE_MINOR		(1)
#define OSVELTE_PATCH_NUM	(3)
#define OSVELTE_VERSION (OSVELTE_MAJOR << 16 | OSVELTE_MINOR)

#define osvelte_info(fmt, ...)      \
	pr_info(OSVELTE_LOG_TAG ": " fmt, ##__VA_ARGS__)

#define osvelte_err(fmt, ...)      \
	pr_err(OSVELTE_LOG_TAG ": " fmt, ##__VA_ARGS__)

#endif /* _OSVELTE_COMMON_H */
