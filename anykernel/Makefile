# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 LibXZR <i@xzr.moe>. All Rights Reserved.

NAME := Horizon-Kernel$(LOCALVERSION).zip

always += all
ALL := $(obj)/all
TARGET := $(objtree)/$(NAME)
PHONY += $(ALL)
anykernel := $(srctree)/$(src)

# For printing info only
quiet_cmd_anykernel = ZIP     $@
cmd_anykernel =

$(ALL): $(TARGET)

$(TARGET): FORCE
	$(Q)rm -rf $(obj)/*
	$(Q)cp -rf $(anykernel)/* $(obj)/
	$(Q)rm -f $(obj)/Makefile
	$(Q)cp -f arch/arm64/boot/Image.gz $(obj)/
	$(Q)cp -f arch/arm64/boot/dtb $(obj)/
	$(Q)cp -f arch/arm64/boot/dtbo.img $(obj)/
	$(Q)cd $(obj);zip -r "$(NAME)" * > /dev/null
	$(Q)mv -f "$(obj)/$(NAME)" $(objtree)
	$(call cmd,anykernel)

clean-files := $(patsubst $(anykernel)/%,%,$(shell find $(anykernel) -type f))
clean-files += Image.gz dtb dtbo.img
