ifeq ($(subst ",,$(CONFIG_MTK_SENSOR_ARCHITECTURE)),1.0)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/sensorhub
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/rv33
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
subdir-ccflags-y += -D CONFIG_OPLUS_SENSOR_MTK68XX
endif


obj-$(CONFIG_OPLUS_FEATURE_SENSOR_DEVINFO_V2)	+= oplus_bsp_sensor_devinfo.o
oplus_bsp_sensor_devinfo-objs += sensor_devinfo.o
oplus_bsp_sensor_devinfo-objs += sensor_interface_v2.o

