# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/video/config/konavid.conf
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
include $(srctree)/techpack/video/config/instantnoodlevid.conf
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
include $(srctree)/techpack/video/config/instantnoodlepvid.conf
endif
endif

ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/video/config/konavidconf.h
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
LINUXINCLUDE    += -include $(srctree)/techpack/video/config/instantnoodlevidconf.h
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/video/config/instantnoodlepvidconf.h
endif
endif

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/video/config/litovid.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/video/config/litovidconf.h
endif

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/video/config/bengalvid.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/video/config/bengalvidconf.h
endif

obj-y +=msm/
