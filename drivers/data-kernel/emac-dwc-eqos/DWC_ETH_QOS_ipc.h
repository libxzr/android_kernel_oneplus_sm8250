/***************************************************************************

* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.

* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.

* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**************************************************************************/

/***************************************************************************
*
* File: tcp-splice-ipc.h
* Header file to define log apis for tcp splice debugging
*
***************************************************************************/

#include <linux/ipc_logging.h>
#include <linux/ratelimit.h>

extern void *ipc_emac_log_ctxt;
extern void *ipc_emac_log_ctxt_low;
#define IPC_RATELIMIT_BURST 1
#define WARNON_RATELIMIT_BURST 1

#define IPCLOG_STATE_PAGES 50
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
	strrchr(__FILE__, '/') + 1 : __FILE__)

#define EMACDBG(fmt, args...) \
do {\
	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] DEBUG:" fmt, __FILENAME__ , \
		__func__, __LINE__, ## args); \
	} \
}while(0)
#define EMACINFO(fmt, args...) \
do {\
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] INFO:" fmt, __FILENAME__ , \
		__func__, __LINE__, ## args); \
	} \
}while(0)
#define EMACERR(fmt, args...) \
do {\
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] ERROR:" fmt, __FILENAME__ , \
		__func__, __LINE__, ## args); \
	} \
}while(0)

#define IPC_LOW(fmt, args...) \
do {\
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt_low) { \
		ipc_log_string(ipc_emac_log_ctxt_low, \
		"%s: %s[%u]:[ipc] DEBUG:" fmt, __FILENAME__ , \
		__func__, __LINE__, ## args); \
	} \
}while(0)

/*
	Printing one warning message in 5 seconds if multiple warning messages
	are coming back to back.
*/
#define WARN_ON_RATELIMIT_IPC(condition) \
({ \
	static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL, WARNON_RATELIMIT_BURST); \
	int rtn = !!(condition); \
	if (unlikely(rtn && __ratelimit(&_rs))) \
		WARN_ON(rtn); \
})

/*
	Printing one error message in 5 seconds if multiple error messages
	are coming back to back.
*/
#define pr_err_ratelimited_ipc(fmt, ...) \
	printk_ratelimited_ipc(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define printk_ratelimited_ipc(fmt, ...) \
({ \
	static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL, IPC_RATELIMIT_BURST); \
	if (__ratelimit(&_rs)) \
		printk(fmt, ##__VA_ARGS__); \
})
#define IPCERR_RL(fmt, args...) \
do { \
	pr_err_ratelimited_ipc(DRV_NAME " %s:%d " fmt, __func__,\
	__LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] Error:" fmt, __FILENAME__ , \
		__func__, __LINE__, ## args); \
	} \
} while (0)

