# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2018-2020 Oplus. All rights reserved.
vendor := $(srctree)/$(src)

#ifndef OPLUS_BUG_STABILITY
#ifneq "$(wildcard $(vendor)/qcom)" ""
#	subdir-y += qcom
#endif
#else
subdir-y += qcom
ifeq ($(strip $(BRAND_SHOW_FLAG)), oneplus)
#oneplus SM8250 project
subdir-y += 19821
subdir-y += 19855
subdir-y += 19811
subdir-y += 19805
subdir-y += 20809
subdir-y += 20828
#7250 NORD
subdir-y += 20801
subdir-y += 20813
else ifeq ($(strip $(BRAND_SHOW_FLAG)), oplus)
#oplus SM8250 project
subdir-y += 19795
subdir-y += 19795_t0_evt2
subdir-y += 20607
subdir-y += 19705
subdir-y += 19705_t0_evt2
subdir-y += 21615
subdir-y += 21619
subdir-y += 21623
subdir-y += 21733
#oplus SM7250 project
subdir-y += oplus
else
#oplus SM8250 project
subdir-y += 19065
subdir-y += 19066
subdir-y += 19161
subdir-y += 20135
subdir-y += 20061
subdir-y += 20161
subdir-y += 20351
subdir-y += 21027
#oplus SM7250/SM7225 project
subdir-y += 19125
subdir-y += 19101
subdir-y += 19335
subdir-y += 19191
subdir-y += 19015
subdir-y += 19517
subdir-y += oplus
endif
#endif
