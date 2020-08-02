# auto-detect subdirs
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
include $(srctree)/techpack/audio/config/sdxpoorwillsauto.conf
export
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/audio/config/konaauto.conf
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
include $(srctree)/techpack/audio/config/instantnoodleauto.conf
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
include $(srctree)/techpack/audio/config/instantnoodlepauto.conf
endif
endif

# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdxpoorwillsautoconf.h
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifneq ($(CONFIG_MACH_ONEPLUS_SM8250), y)
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/konaautoconf.h
endif
else
ifeq ($(CONFIG_MACH_ONEPLUS_8), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/instantnoodleautoconf.h
endif
ifeq ($(CONFIG_MACH_ONEPLUS_8PRO), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/instantnoodlepautoconf.h
endif
endif

obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
