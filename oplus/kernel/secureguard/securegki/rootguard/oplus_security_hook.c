// SPDX-License-Identifier: GPL-2.0-only
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_security_hook.c
* Description: oplus selinux define function
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#include <linux/un.h>		/* for Unix socket types */
#include <net/af_unix.h>	/* for Unix socket types */
#include <linux/security.h>
#include <linux/lsm_hooks.h>
#include "oplus_security.h"

extern struct selinux_state selinux_state;
extern int security_sid_to_context(struct selinux_state *state, u32 sid, char **scontext, u32 *scontext_len);

extern struct lsm_blob_sizes selinux_blob_sizes;
static inline struct task_security_struct *selinux_cred(const struct cred *cred)
{
    return cred->security + selinux_blob_sizes.lbs_cred;
}

static inline u32 current_sid(void)
{
    const struct task_security_struct *tsec = selinux_cred(current_cred());
    return tsec->sid;
}

int get_current_security_context(char **context, u32 *context_len)
{
	u32 sid = current_sid();
	return security_sid_to_context(&selinux_state, sid, context, context_len);
}