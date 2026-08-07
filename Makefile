# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
ifeq ($(KERNEL_VERSION),)
KERNEL_VERSION=6.10
endif

CHECK_KERNEL_FILES := Kbuild Makefile
THIS_MAKEFILE := $(realpath $(lastword $(MAKEFILE_LIST)))
THIS_DIR := $(dir $(THIS_MAKEFILE))

KERNEL_SEARCH_PATH := \
    $(realpath ../../../source/kernel/) \
    $(realpath $(THIS_DIR)/../../../../)

KERNEL_DIR ?= $(strip $(shell \
    KERNEL_PATHS="$(subst $(space), ,$(KERNEL_SEARCH_PATH))"; \
    CHECK_FILES="$(subst $(space), ,$(CHECK_KERNEL_FILES))"; \
    for kdir in $$KERNEL_PATHS; do \
        if [ -d "$$kdir" ] && test -f "$$kdir/Kbuild" && test -f "$$kdir/Makefile"; then \
            echo "$$kdir"; \
            break; \
        else \
            echo "Checking $$kdir: invalid." >&2; \
            continue; \
        fi; \
    done \
))

ifeq ($(KERNEL_DIR),)
$(error *** ERROR: Unable to find a valid kernel source directory in KERNEL_SEARCH_PATH (must include $(CHECK_KERNEL_FILES)):)
$(error *** Search Paths: $(KERNEL_SEARCH_PATH))
endif

ARCH ?= arm
KERNEL_ROOT=$(KERNEL_DIR)
CPU_TYPE=$(ARCH)
OSTYPE=linux
KMPP_SRC := kmpp
MAKE_JOBS ?= $(shell echo `getconf _NPROCESSORS_ONLN`)
KMPP_MAKE_JOBS := $(shell expr $(MAKE_JOBS) / 2)

EXTRA_CFLAGS += -fno-verbose-asm
EXTRA_CFLAGS += -ffunction-sections
EXTRA_CFLAGS += -fdata-sections

ifeq ($(RK_ENABLE_FASTBOOT), y)
TOP := $(srctree)/$(src)
KMPP_TEST ?= n
else
TOP := $(THIS_DIR)
KMPP_TEST ?= y
endif

define get_relative_path
$(eval _CUR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))))
$(eval _REL := $(subst $(TOP),,$(_CUR)))
$(eval _REL := $(patsubst /%,%,$(_REL)))
$(if $(_REL),$(_REL),.)
endef

# remove empty line
define get_rel_path
$(strip $(call get_relative_path))
endef

EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/include
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/base/inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/codec/enc/h264e
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/codec/inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/common
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/common
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/common/h264
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/common/h265
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/common/jpeg
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/rkenc/common
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/rkenc/h264e
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/rkenc/h265e
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/hal/rkenc/jpege
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/mpp/inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/drivers/video/rockchip/kmpp/vcodec/osal/inc

ifeq ($(CONFIG_CPU_RV1106), y)
VEPU_CORE := RKVEPU540C
endif

ifeq ($(CONFIG_CPU_RV1103B), y)
VEPU_CORE := RKVEPU500
endif

ifeq ($(CONFIG_CPU_RK3576), y)
VEPU_CORE := RKVEPU510
endif

ifeq ($(CONFIG_CPU_RV1126B), y)
VEPU_CORE := RKVEPU511
endif

BUILD_ONE_KO ?= y

ifeq ($(BUILD_ONE_KO), y)
	EXTRA_CFLAGS += -DBUILD_ONE_KO
	KMPP_OSAL_SRC := kmpp
	KMPP_SYS_SRC := kmpp
	MPP_SERVICE_SRC := kmpp
else
	BUILD_MULTI_KO=y
	EXTRA_CFLAGS += -DBUILD_MULTI_KO
	KMPP_OSAL_SRC := kmpp_osal
	KMPP_SYS_SRC := kmpp_sys
	MPP_SERVICE_SRC := kmpp
endif

KMPP_TEST_ENABLE=${KMPP_TEST}

ifeq ($(CPU_TYPE), arm64)
	export PREB_KO := ./prebuild/ko_64
	export REL_KO  := ./prebuild/ko_64_rel
else
	export PREB_KO := ./prebuild/ko_32
	export REL_KO  := ./prebuild/ko_32_rel
endif

KMPP_GIT_DIR := $(TOP)
$(shell [ -d "$(KMPP_GIT_DIR)/.git/" ] && chmod +x $(TOP)/version.sh && $(TOP)/version.sh $(KMPP_GIT_DIR) >/dev/null 2>&1)

VCODEC_GIT_REVISION := \
	$(shell [ -d "$(KMPP_GIT_DIR)/.git/" ] && git --git-dir=$(KMPP_GIT_DIR)/.git log -1 --no-decorate --date=short \
	--pretty=format:"%h author: %<|(30)%an %cd %s" -- . || \
	echo -n "unknown mpp version for missing VCS info")

VCODEC_REVISION_0 := $(subst \,\\\,$(VCODEC_GIT_REVISION))
VCODEC_REVISION   := $(subst ",\\\",$(VCODEC_REVISION_0))

# add git hooks
$(shell [ -d "$(TOP)/.git/" ] && [ -d "$(TOP)/tools/hooks" ] && cp -rf $(TOP)/tools/hooks $(TOP)/.git/)

-include $(TOP)/mpp_osal/Makefile
-include $(TOP)/osal/Makefile
-include $(TOP)/sys/Makefile
-include $(TOP)/kmpp/Makefile
-include $(TOP)/vproc/Makefile
-include $(TOP)/mpp_service/Makefile
-include $(TOP)/vcodec/Makefile

ifeq ($(KMPP_TEST_ENABLE), y)
-include $(TOP)/test/Makefile
endif

KMPP_SMT_SRC := kmpp_smart
$(info build ${KMPP_SMT_SRC}.ko)
ifneq ($(wildcard $(TOP)/smt/Makefile),)
-include $(TOP)/smt/Makefile
else
ifneq ($(wildcard $(TOP)/../smt_release/$(CPU_TYPE)/Makefile),)
-include $(TOP)/../smt_release/$(CPU_TYPE)/Makefile
endif
endif

ifeq ($(RK_ENABLE_FASTBOOT), y)
obj-y += ${KMPP_SRC}.o
else
obj-m += ${KMPP_SRC}.o
endif

ifneq ($(SYSDRV_KERNEL_OBJS_OUTPUT_DIR),)
KMPP_BUILD_KERNEL_OBJ_DIR=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)
else
KMPP_BUILD_KERNEL_OBJ_DIR=$(KERNEL_DIR)
endif

DIRS := $(shell find . -maxdepth 5 -type d)

FILES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.c))

OBJS = $(patsubst %.c,%.o, $(FILES))
CMDS = $(foreach dir,$(DIRS),$(wildcard $(dir)/.*.o.cmd))

.PHONY: all clean
all: $(OSTYPE)_build
clean: $(OSTYPE)_clean
install: $(OSTYPE)_install

linux_build: linux_clean
	@echo -e "\e[0;32;1m--Compiling '$(VMPI)'... Configs as follow:\e[0;36;1m"
	@echo ---- CROSS=$(CROSS_COMPILE)
	@echo ---- CPU_TYPE=$(CPU_TYPE)
	@$(MAKE) CROSS_COMPILE=${CROSS_COMPILE} ARCH=$(CPU_TYPE) -C $(KERNEL_ROOT) M=$(PWD) modules O=$(KMPP_BUILD_KERNEL_OBJ_DIR) -j$(KMPP_MAKE_JOBS)
	@mkdir -p $(PREB_KO)
	@cp ${KMPP_SRC}.ko $(PREB_KO)
ifeq ($(BUILD_MULTI_KO), y)
	@cp $(MPP_SERVICE_SRC).ko  $(PREB_KO)
	@cp $(KMPP_OSAL_SRC).ko  $(PREB_KO)
	@cp $(KMPP_SYS_SRC).ko  $(PREB_KO)
endif
	@test ! -f ${KMPP_SMT_SRC}.ko || cp ${KMPP_SMT_SRC}.ko $(PREB_KO)
	@mkdir -p $(REL_KO)
	@cp ${KMPP_SRC}.ko $(REL_KO)
ifeq ($(BUILD_MULTI_KO), y)
	@cp $(MPP_SERVICE_SRC).ko  $(REL_KO)
	@cp $(KMPP_OSAL_SRC).ko  $(REL_KO)
	@cp $(KMPP_SYS_SRC).ko  $(REL_KO)
endif
	@test ! -f ${KMPP_SMT_SRC}.ko || cp ${KMPP_SMT_SRC}.ko $(REL_KO)
	@find $(REL_KO) -name "*.ko" | xargs ${CROSS_COMPILE}strip --strip-debug --remove-section=.comment --remove-section=.note --preserve-dates

linux_clean:
	@rm -f *.ko *.mod.c *.o
	@rm -f *.symvers *.order
	@find ./ -name "*.cmd" -o -name "*.s" -o -name "*.o" -o -name "*.mod" | xargs rm -rf
	@rm -rf .tmp_versions
	@rm -f built-in.a lib.a
	@rm -rf $(PREB_KO)
	@rm -rf $(REL_KO)
	@rm -rf $(OBJS)
	@rm -rf $($(SMT_SRC)-objs)
	@rm -rf $(CMDS)

linux_release:
	find $(REL_KO) -name "*.ko" | xargs ${CROSS_COMPILE}strip --strip-debug --remove-section=.comment --remove-section=.note --preserve-dates

release_asm_s:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(CPU_TYPE) -C $(KERNEL_ROOT) M=$(PWD) O=$(KMPP_BUILD_KERNEL_OBJ_DIR) $(KMPP_ALL_ASM_OBJS) -j$(KMPP_MAKE_JOBS)
