// SPDX-License-Identifier: GPL-2.0-only
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_secure_debug.c
* Description: for oplus_secure_debug
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

#include "oplus_secure_debug.h"

void print_format(unsigned char * temp,unsigned int len)
{
	unsigned int i=0;
	printk(" ");
	for(i=0;i<len;i++)
	{
		if(temp[i]<16)
			printk("0%x",temp[i]);
		else
			printk("%x",temp[i]);


		if(i%32==31)
			printk("\n");
		if((i%16==7)||(i%16==15))
			printk(" ");
	}
	if(len%16!=15)
		printk("\n");
	printk("\n\n");

}

