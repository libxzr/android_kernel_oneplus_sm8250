#ifdef CONFIG_OPLUS_KERNEL_SECURE_FEATURE
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y += -DWHITE_LIST_SUPPORT
endif
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_root_check.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_exec_block.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_mount_block.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_guard_general.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_security_hook.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_kevent.o
obj-$(CONFIG_OPLUS_KERNEL_SECURE_GUARD)	 +=  oplus_hook_syscall.o
#endif /* CONFIG_OPLUS_KERNEL_SECURE_FEATURE */
