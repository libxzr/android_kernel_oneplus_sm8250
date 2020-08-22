# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
#
# This is based on the original scripts/dtc/Makefile, but has been revised to
# work with an insane submodule situation.

hostprogs-y	:= dtc-aosp
always		:= $(hostprogs-y)

dtc-aosp-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-aosp-objs	+= dtc-lexer.lex.o dtc-parser.tab.o

# We're working with a submodule, so make these all relative to that.
dtc-aosp-objs	:= $(addprefix dtc/,$(dtc-aosp-objs))

HOSTCFLAGS_DTC := -I$(src)/dtc -I$(src)/dtc/libfdt -I$(obj)/dtc

HOSTCFLAGS_checks.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_data.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_dtc.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_flattree.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_fstree.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_livetree.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_srcpos.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_treesource.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_util.o := $(HOSTCFLAGS_DTC)

HOSTCFLAGS_dtc-lexer.lex.o := $(HOSTCFLAGS_DTC)
HOSTCFLAGS_dtc-parser.tab.o := $(HOSTCFLAGS_DTC)

# Dependencies on generated files need to be listed explicitly.
$(obj)/dtc/dtc-lexer.lex.o: $(obj)/dtc/dtc-parser.h

# The kernel build supports flex and bison now
%.lex.c: %.l
	$(call if_changed,flex)
%.tab.c: %.y
	$(call if_changed,bison)
%.h: %.y
	$(call if_changed,bison_h)

# Generated files need to be cleaned explicitly.
clean-files	:= dtc/dtc-lexer.lex.c dtc/dtc-parser.tab.c dtc/dtc-parser.h
