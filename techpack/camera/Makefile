# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/camera/config/konacamera.conf
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
include $(srctree)/techpack/camera/config/instantnoodlecamera.conf
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
include $(srctree)/techpack/camera/config/instantnoodlepcamera.conf
endif
endif

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/camera/config/litocamera.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/camera/config/bengalcamera.conf
endif

ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/camera/config/konacameraconf.h
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/camera/config/instantnoodlecameraconf.h
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/camera/config/instantnoodlepcameraconf.h
endif
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += \
		-include $(srctree)/techpack/camera/config/litocameraconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += \
		-include $(srctree)/techpack/camera/config/bengalcameraconf.h
endif

ifdef CONFIG_SPECTRA_CAMERA
# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/camera/include/uapi

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/camera/include/uapi \
                -I$(srctree)/techpack/camera/include
obj-y += drivers/
else
$(info Target not found)
endif
