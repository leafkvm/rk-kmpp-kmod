RELATIVE_PATH := $(call get_rel_path)

HAL_OBJS := \
	$(RELATIVE_PATH)/kmpp_hal.o

${KMPP_SRC}-objs += $(HAL_OBJS)

MAKE_SRV := $(TOP)/$(RELATIVE_PATH)/srv/Makefile

-include $(MAKE_SRV)
