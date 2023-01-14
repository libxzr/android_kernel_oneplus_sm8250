/* SPDX-License-Identifier: GPL-2.0-only */
/**************************************************************
* Copyright (c)  2008- 2030  OPLUS Mobile communication Corp.ltd All rights reserved.
* VENDOR_EDIT
* File       : oplus_secure_debug.h
* Description: For oplus_secure_debug
* Version   : 1.0
* Date        : 2019-12-19
* Author    :
* TAG         :
****************************************************************/
#ifndef _OPLUS_SECURE_GUARD_DEBUG_H
#define _OPLUS_SECURE_GUARD_DEBUG_H

#define OPLUS_SECURE_GUARD_DEGBU


void print_format(unsigned char * temp,unsigned int len);

#ifdef OPLUS_SECURE_GUARD_DEGBU
#define  PRINT_FORMAT(t, l)		        print_format(t, l)
#else
#define  PRINT_BUF(t, l)
#endif
#endif
