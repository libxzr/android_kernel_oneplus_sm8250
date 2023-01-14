ifeq ($(subst ",,$(CONFIG_MTK_SENSOR_ARCHITECTURE)),1.0)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
subdir-ccflags-y += -D _OPLUS_SENSOR_HUB_VI
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/oplus_virtual_sensor/platform/sensor_cmd
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/oplus_virtual_sensor/platform/sensor_device
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/oplus_virtual_sensor/platform/sensor_event
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
subdir-ccflags-y += -D _OPLUS_SENSOR_HUB_V2
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/oplus_virtual_sensor/platform/sensor_cmd
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/oplus_virtual_sensor/platform/sensor_device
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/oplus_virtual_sensor/platform/sensor_event
endif
obj-$(CONFIG_OPLUS_FEATURE_CAMERA_PROTECT)      +=	camera_protecthub/
obj-$(CONFIG_OPLUS_FEATURE_FREE_FALL)		+=	free_fallhub/
obj-$(CONFIG_OPLUS_FEATURE_PICKUP_DETECT)	+=	pickup_detecthub/
obj-$(CONFIG_OPLUS_FEATURE_FP_DISPLAY)		+=	fp_displayhub/
obj-$(CONFIG_OPLUS_FEATURE_LUX_AOD)	        +=	lux_aodhub/
obj-$(CONFIG_OPLUS_FEATURE_PEDO_MINUTE)	    +=	pedo_minutehub/
obj-$(CONFIG_OPLUS_FEATURE_ACTIVITY_RECOGNITION) += oplus_activity_recognitionhub/
obj-$(CONFIG_OPLUS_FEATURE_TP_GESTURE)          += tp_gesturehub/
obj-$(CONFIG_OPLUS_FEATURE_ELEVATOR_DETECT)     += elevator_detecthub/
obj-$(CONFIG_OPLUS_FEATURE_SENSOR_MONITOR)	+= sensor_monitor/
