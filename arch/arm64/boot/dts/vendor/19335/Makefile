ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
    dtbo-$(CONFIG_ARCH_LITO) += lito-mtp-overlay.dtbo \
	dtbo-$(CONFIG_ARCH_LITO) += lito-v2-mtp-overlay.dtbo \

    lito-mtp-overlay.dtbo-base := lito.dtb
	lito-v2-mtp-overlay.dtbo-base := lito-v2.dtb
else
    dtb-$(CONFIG_ARCH_LITO) += lito-mtp.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
