/* SPDX-License-Identifier: GPL-2.0-only */
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_guard_general.h
* Description: for rootguard general function
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#ifndef OPLUS_GUARD_GENERAL_H_
#define OPLUS_GUARD_GENERAL_H_

#define KERNEL_ADDR_LIMIT 0x0000008000000000

bool is_unlocked(void);

bool is_normal_boot_mode(void);

void oplus_root_check_succ(uid_t uid, uid_t euid, uid_t fsuid, uid_t callnum);

void oplus_root_reboot(void);

int oplus_mount_block(const char __user *dir_name, unsigned long flags);

int oplus_exec_block(struct file *file);

#endif /*OPLUS_GUARD_GENERAL_H_*/
