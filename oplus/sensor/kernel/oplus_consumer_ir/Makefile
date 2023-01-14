ifneq ($(filter y m,$(CONFIG_OPLUS_SENSOR_CONSUMER_IR_MTK)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/pwm_v2/include/
endif # !CONFIG_OPLUS_SENSOR_CONSUMER_IR_MTK

oplus_bsp_ir_core-y := oplus_ir_core.o
obj-$(CONFIG_OPLUS_SENSOR_CONSUMER_IR_CORE)	+= oplus_bsp_ir_core.o

oplus_bsp_kookong_ir_spi-y := oplus_ir_spi.o
obj-$(CONFIG_OPLUS_SENSOR_CONSUMER_IR_QC)	+= oplus_bsp_kookong_ir_spi.o

oplus_bsp_kookong_ir_pwm-y := oplus_ir_pwm.o
obj-$(CONFIG_OPLUS_SENSOR_CONSUMER_IR_MTK)	+= oplus_bsp_kookong_ir_pwm.o
