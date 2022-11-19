// SPDX-License-Identifier: GPL-2.0-only
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_kevent_upload.c
* Description: oplus_kevent_upload
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <net/net_namespace.h>
#include <linux/proc_fs.h>
#include <net/sock.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <net/genetlink.h>

#include <trace/hooks/secureguard.h>

#include "oplus_kevent.h"
#include "oplus_secure_debug.h"
#include "oplus_secure_guard_netlink.h"

static int oplus_security_keventupload_flag = 0;
#ifdef CONFIG_OPLUS_KERNEL_SECURE_GUARD
static int oplus_security_trace_flag = 0;
#endif /*CONFIG_OPLUS_KERNEL_SECURE_GUARD*/
static volatile unsigned int kevent_pid;

#define OPLUS_KEVENT_MAX_UP_PALOAD_LEN			2048
#define OPLUS_KEVENT_TEST_TAG				"test_event"
#define OPLUS_KEVENT_TEST_ID				"test_check"

static void oplus_kevent_send_to_user(void *data, struct kernel_packet_info *userinfo, int *p_retval);

static int security_keventupload_sendpid_cmd(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na = NULL;
	unsigned int * p_data = NULL;

	pr_info("[OPLUS_SECURITY DEBUG]:kernel recv cmd \n");
	if (info->attrs[SECURE_GUARD_CMD_ATTR_MSG]){
		na = info->attrs[SECURE_GUARD_CMD_ATTR_MSG];
		PRINT_FORMAT(nla_data(na),  nla_len(na));
		pr_info("[OPLUS_SECURITY DEBUG]:nla_len(na) is %d  \n", nla_len(na));
		p_data = nla_data(na);
		kevent_pid = *p_data;
		pr_info("[OPLUS_SECURITY DEBUG]:kevent_pid is 0x%x  \n", kevent_pid);
        }

	return 0;
}

static int security_keventupload_test_upload(struct sk_buff *skb, struct genl_info *info)
{
        int ret = 0;
	struct nlattr *na = NULL;
	struct msg_test_upload *p_test_upload = NULL;
	struct kernel_packet_info *p_dcs_event = NULL;
	size_t data_len = 0;

	pr_info("[OPLUS_SECURITY DEBUG]:security_keventupload_test_upload \n");
	if (info->attrs[SECURE_GUARD_CMD_ATTR_OPT]){
		na = info->attrs[SECURE_GUARD_CMD_ATTR_OPT];
		PRINT_FORMAT(nla_data(na),  nla_len(na));
		pr_info("[OPLUS_SECURITY DEBUG]:nla_len(na) is %d  \n", nla_len(na));
		p_test_upload = (struct msg_test_upload *)nla_data(na);
		kevent_pid = p_test_upload->pro_pid;
		pr_info("[OPLUS_SECURITY DEBUG]:p_test_upload->pro_pid is %u, p_test_upload->val is %u, \n", p_test_upload->pro_pid, p_test_upload->val);
#if 1
		if ((p_test_upload->val) > OPLUS_KEVENT_MAX_UP_PALOAD_LEN){
			pr_err("[ERROR]:p_test_upload->val too long \n", p_test_upload->val);
			return -1;
		}

                data_len = p_test_upload->val + sizeof(struct kernel_packet_info);
                pr_info("[OPLUS_SECURITY DEBUG]:data_len is %u\n", data_len);
		p_dcs_event = (struct kernel_packet_info *)kmalloc(data_len, GFP_ATOMIC);
		if (NULL == p_dcs_event){
			pr_err("[ERROR]:kmalloc for p_dcs_event err\n");
			return -1;
		}
                pr_info("[OPLUS_SECURITY DEBUG]:p_dcs_event kmalloc ok .\n");

                memset((unsigned char *)p_dcs_event, 0x00, data_len);
		p_dcs_event->type = 1;
		strncpy(p_dcs_event->log_tag, OPLUS_KEVENT_TEST_TAG, sizeof(p_dcs_event->log_tag));
		strncpy(p_dcs_event->event_id, OPLUS_KEVENT_TEST_ID, sizeof(p_dcs_event->event_id));
		p_dcs_event->payload_length = p_test_upload->val;
                memset(p_dcs_event->payload, 0xFF, p_test_upload->val);

		oplus_kevent_send_to_user(NULL, p_dcs_event, &ret);
		if (ret){
		    pr_err("[ERROR]:kevent_send_to_user err, ret is %d \n", ret);
		}

		kfree(p_dcs_event);
#endif
    }

	return 0;

}


static const struct genl_ops oplus_security_ops[] = {
	{
		.cmd		= SECURE_GUARD_CMD_GENL_SENDPID,
		.doit		= security_keventupload_sendpid_cmd,
		//.policy		= taskstats_cmd_get_policy,
		//.flags		= GENL_ADMIN_PERM,
	},
	{
		.cmd		= SECURE_GUARD_CMD_GENL_TEST_UPLOAD,
		.doit		= security_keventupload_test_upload,
		//.dumpit		= taskstats2_foreach,
		//.policy		= taskstats_cmd_get_policy,
	},
};


static struct genl_family oplus_security_family __ro_after_init = {
	.name		= OPLUS_SECURE_GUARD_PROTOCAL_NAME,
	.version	= OPLUS_SECURE_GUARD_GENL_VERSION,
	.maxattr	= SECURE_GUARD_CMD_ATTR_MAX,
	.module		= THIS_MODULE,
	.ops		= oplus_security_ops,
	.n_ops		= ARRAY_SIZE(oplus_security_ops),
};


static inline int genl_msg_prepare_usr_msg(unsigned char cmd, size_t size, pid_t pid, struct sk_buff **skbp)
{
    struct sk_buff *skb;

    /* create a new netlink msg */
    skb = genlmsg_new(size, GFP_KERNEL);
    if (skb == NULL) {
        return -ENOMEM;
    }

    /* Add a new netlink message to an skb */
    genlmsg_put(skb, pid, 0, &oplus_security_family, 0, cmd);

    *skbp = skb;
    return 0;
}

static inline int genl_msg_mk_usr_msg(struct sk_buff *skb, int type, void *data, int len)
{
    int ret;

    /* add a netlink attribute to a socket buffer */
    if ((ret = nla_put(skb, type, len, data)) != 0) {
        return ret;
    }
    return 0;
}


static void oplus_kevent_send_to_user(void *data, struct kernel_packet_info *userinfo, int *p_retval)
{
	int ret = 0;

	struct sk_buff *skbuff = NULL;
	void * head = NULL;
	size_t data_len = 0;
	size_t attr_len = 0;
        *p_retval = 0;
	/*max_len */
	pr_info("[OPLUS_SECURITY DEBUG]:oplus_kevent_send_to_user\n");

	if (userinfo->payload_length >= OPLUS_KEVENT_MAX_UP_PALOAD_LEN){
        pr_err("[ERROR]:kevent_send_to_user: payload_length out of range\n");
		*p_retval = -1;
		return ;
	}

	data_len = userinfo->payload_length + sizeof(struct kernel_packet_info);
	attr_len = nla_total_size(data_len);
	pr_info("[OPLUS_SECURITY DEBUG]:data_len is %u, attr_len is %u\n", data_len, attr_len);

	ret = genl_msg_prepare_usr_msg(SECURE_GUARD_CMD_GENL_UPLOAD, attr_len, kevent_pid, &skbuff);
        if (ret){
        pr_err("[ERROR]:genl_msg_prepare_usr_msg err, ret is %d \n");
		*p_retval = -1;
		return ;
	}

        ret = genl_msg_mk_usr_msg(skbuff, SECURE_GUARD_CMD_ATTR_MSG, userinfo, data_len);
        if (ret) {
        kfree_skb(skbuff);
		*p_retval = ret;
		return ;
        }

        head = genlmsg_data(nlmsg_data(nlmsg_hdr(skbuff)));

        genlmsg_end(skbuff, head);

        ret = genlmsg_unicast(&init_net, skbuff, kevent_pid);
        if (ret < 0) {
		*p_retval = ret;
		return ;
        }

	*p_retval = 0;
        return ;
}

static int __init oplus_security_keventupload_init(void)
{
	int ret = 0;

	/*register gen_link family*/
	ret = genl_register_family(&oplus_security_family);
	if (ret){
		goto exit;
	}
	oplus_security_keventupload_flag = 1;
	pr_info("registered gen_link family %s OK \n", OPLUS_SECURE_GUARD_PROTOCAL_NAME);
	/*register trace hook*/
#ifdef CONFIG_OPLUS_KERNEL_SECURE_GUARD
        ret = register_trace_android_vh_secureguard_send_to_user(oplus_kevent_send_to_user, NULL);
        if (ret != 0){
	        pr_err("[ERROR]:register_trace_android_vh_secureguard_post_handle failed! ret=%d\n", ret);
	        goto exit;
        }
        oplus_security_trace_flag = 1;

#endif /*CONFIG_OPLUS_KERNEL_SECURE_GUARD*/

exit:
        if (ret){
#ifdef CONFIG_OPLUS_KERNEL_SECURE_GUARD
                if (oplus_security_trace_flag){
		        unregister_trace_android_vh_secureguard_send_to_user(oplus_kevent_send_to_user, NULL);
	        }
#endif /*CONFIG_OPLUS_KERNEL_SECURE_GUARD*/
                if (oplus_security_keventupload_flag){
	                genl_unregister_family(&oplus_security_family);
	        }
	}
	return ret;
}


static void __exit oplus_security_keventupload_exit()
{
#ifdef CONFIG_OPLUS_KERNEL_SECURE_GUARD
	if (oplus_security_trace_flag){
		unregister_trace_android_vh_secureguard_send_to_user(oplus_kevent_send_to_user, NULL);
        }
#endif /*CONFIG_OPLUS_KERNEL_SECURE_GUARD*/
	if (oplus_security_keventupload_flag){
		genl_unregister_family(&oplus_security_family);
	}
	return ;
}

module_init(oplus_security_keventupload_init);
module_exit(oplus_security_keventupload_exit);
MODULE_LICENSE("GPL");
