RELATIVE_PATH := $(call get_rel_path)

EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/h264
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/h265
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/jpeg
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/hal/h264
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/hal/h265
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec/hal/jpeg

VDEC_OBJS := \
	$(RELATIVE_PATH)/kmpp_vdec_ioctl.o \
	$(RELATIVE_PATH)/kmpp_vdec.o \
	$(RELATIVE_PATH)/vdec_parser.o \
	$(RELATIVE_PATH)/vdec_hal.o

${KMPP_SRC}-objs += $(VDEC_OBJS)

# IMPORTANT: use new variables instead of RELATIVE_PATH
# because RELATIVE_PATH will be changed on include new Makefile
MAKE_H264 := $(TOP)/$(RELATIVE_PATH)/h264/Makefile
MAKE_H265 := $(TOP)/$(RELATIVE_PATH)/h265/Makefile
MAKE_JPEG := $(TOP)/$(RELATIVE_PATH)/jpeg/Makefile
MAKE_VDEC_HAL := $(TOP)/$(RELATIVE_PATH)/hal/Makefile

-include $(MAKE_H264)
-include $(MAKE_H265)
-include $(MAKE_JPEG)
-include $(MAKE_VDEC_HAL)
