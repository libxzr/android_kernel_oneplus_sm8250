// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "touchpanel_exception.h"

#include <linux/err.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <asm/current.h>
#include <linux/version.h>

#include "touchpanel_common.h"

static inline int tp_olc_raise_exception(tp_excep_type excep_tpye, void *summary, unsigned int summary_size)
{
	return 0;
}

int tp_exception_report(void *tp_exception_data, tp_excep_type excep_tpye, void *summary, unsigned int summary_size)
{
	int ret = -1;

	struct exception_data *exception_data = (struct exception_data *)tp_exception_data;

	if (!exception_data || !exception_data->exception_upload_support) {
		return 0;
	}
	exception_data->exception_upload_count++;
	switch (excep_tpye) {
	case EXCEP_BUS:
		/*bus error upload tow times*/
		exception_data->bus_error_upload_count++;
		if (exception_data->bus_error_count > MAX_BUS_ERROR_COUNT
				&& exception_data->bus_error_upload_count < 3) {
			exception_data->bus_error_count = 0;
			ret = tp_olc_raise_exception(excep_tpye, summary, summary_size);
		}
		break;
	default:
		ret = tp_olc_raise_exception(excep_tpye, summary, summary_size);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(tp_exception_report);

