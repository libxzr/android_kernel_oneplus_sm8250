/* SPDX-License-Identifier: GPL-2.0-only */
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_secure_guard_netlink.h
* Description: For oplus_secure_guard_netlink
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#ifndef _OPLUS_SECURE_GUARD_TEST_NETLINK_H
#define _OPLUS_SECURE_GUARD_TEST_NETLINK_H

#define OPLUS_SECURE_GUARD_TEST_NETLINK_CMD_MIN			0
#define OPLUS_SECURE_GUARD_TEST_NETLINK_SEND_PID		1
#define OPLUS_SECURE_GUARD_TEST_NETLINK_RECEV			2
#define OPLUS_SECURE_GUARD_TEST_NETLINK_CMD_MAX			3

#define OPLUS_SECURE_GUARD_MSG_TO_KERNEL_BUF_LEN		256
#define OPLUS_SECURE_GUARD_MSG_FROM_KERNEL_BUF_LEN		(2048 +16)


#define OPLUS_SECURE_GUARD_PROTOCAL_NAME		"sgtgki"
#define OPLUS_SECURE_GUARD_GENL_VERSION				0x01
#define OPLUS_SECURE_GUARD_PROTOCAL_NAME_MAX_LEN		100

enum {
	SECURE_GUARD_CMD_ATTR_UNSPEC = 0,
	SECURE_GUARD_CMD_ATTR_MSG,
	SECURE_GUARD_CMD_ATTR_OPT,
	__SECURE_GUARD_CMD_ATTR_MAX,
};

#define SECURE_GUARD_CMD_ATTR_MAX 	(__SECURE_GUARD_CMD_ATTR_MAX - 1)

enum {
	SECURE_GUARD_CMD_GENL_UNSPEC = 0,
	SECURE_GUARD_CMD_GENL_SENDPID,
	SECURE_GUARD_CMD_GENL_UPLOAD,
	SECURE_GUARD_CMD_GENL_TEST_UPLOAD,
};


struct msg_to_kernel{
	struct nlmsghdr n_hd;
	struct genlmsghdr g_hd;
	char buf[OPLUS_SECURE_GUARD_MSG_TO_KERNEL_BUF_LEN];
};

struct msg_from_kernel{
	struct nlmsghdr n_hd;
	struct genlmsghdr g_hd;
	char buf[OPLUS_SECURE_GUARD_MSG_FROM_KERNEL_BUF_LEN];
};

struct msg_test_upload{
	unsigned int pro_pid;
	unsigned int val;
};

int oplus_secure_guard_test_netlink(int cmd);

#endif
