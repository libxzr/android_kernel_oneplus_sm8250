####################################################################################
## File: - fingerprints_hal/vendor/fingerprint/Android.mk
## OPLUS_FEATURE_FINGERPRINT
## Copyright (C), 2008-2020, OPLUS Mobile Comm Corp., Ltd
##
## Description:
##      Fingerprint Common Feature Config for Android O
##
## Version: 1.0
## Date created: 18:03:11,08/12/2017
## Author: Ziqing.guo@Prd.BaseDrv
## TAG: BSP.Fingerprint.Basic
## --------------------------- Revision History: --------------------------------
##  <author>      <data>            <desc>
##  Ziqing.guo   2017/12/08        create the file
##  Long.Liu     2018/11/19    modify 18531 static test
##  Long.Liu     2019/01/03    add static test checklist for 18161 fpc1511
##  Ran.Chen     2019/03/08    add secureDSP for goodixfp (SDM855)
####################################################################################
$(warning  OPLUS_FEATURE_FINGERPRINT - $(OPLUS_FEATURE_FINGERPRINT))

ifeq ($(OPLUS_FEATURE_FINGERPRINT), uff_act)
include $(call all-subdir-makefiles)
endif

ifeq ($(OPLUS_FEATURE_UFF_FINGERPRINT), yes)
include $(call all-subdir-makefiles)
endif