# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
include $(srctree)/techpack/display/config/instantnoodledisp.conf
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
include $(srctree)/techpack/display/config/instantnoodlepdisp.conf
endif
endif

ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/instantnoodledispconf.h
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/instantnoodlepdispconf.h
endif
endif

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
obj-$(CONFIG_QCOM_MDSS_PLL) += pll/
