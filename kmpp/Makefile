RELATIVE_PATH := $(call get_rel_path)

EXTRA_CFLAGS += -I$(TOP)/kmpp/objs/inc
EXTRA_CFLAGS += -I$(TOP)/kmpp/vsp/inc
EXTRA_CFLAGS += -I$(TOP)/kmpp/venc
EXTRA_CFLAGS += -I$(TOP)/kmpp/vdec
EXTRA_CFLAGS += -I$(TOP)/vproc/pp
EXTRA_CFLAGS += -I$(TOP)/kmpp/inc
EXTRA_CFLAGS += -I$(TOP)/kmpp/hal/inc
EXTRA_CFLAGS += -I$(TOP)/mpp_service

${KMPP_SRC}-objs += \
	kmpp/kmpp.o \
	kmpp/kmpi/kmpi_vdec.o \
	kmpp/objs/kmpp_buffer.o \
	kmpp/objs/kmpp_frame.o \
	kmpp/objs/kmpp_meta.o \
	kmpp/objs/kmpp_packet.o \
	kmpp/objs/kmpp_venc_objs.o \
	kmpp/objs/kmpp_vdec_objs.o \
	kmpp/objs/kmpp_vsp_objs.o \
	kmpp/venc/kmpp_venc_ioctl.o \
	kmpp/venc/kmpp_venc.o \
	kmpp/vsp/kmpi_vsp.o \
	kmpp/vsp/kmpp_vsp.o

MAKE_VDEC := $(TOP)/$(RELATIVE_PATH)/vdec/Makefile
MAKE_HAL := $(TOP)/$(RELATIVE_PATH)/hal/Makefile

-include $(MAKE_VDEC)
-include $(MAKE_HAL)
