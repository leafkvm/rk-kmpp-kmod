KMPP_OSAL_DIR := osal
EXTRA_CFLAGS += -I$(TOP)/osal
EXTRA_CFLAGS += -I$(TOP)/include
EXTRA_CFLAGS += -I$(TOP)/osal/inc
ifeq ($(CPU_TYPE), arm64)
    EXTRA_CFLAGS += -mno-outline-atomics
endif

${KMPP_OSAL_SRC}-objs += \
    $(KMPP_OSAL_DIR)/kmpp_atomic.o \
    $(KMPP_OSAL_DIR)/kmpp_bitops.o \
    $(KMPP_OSAL_DIR)/kmpp_cls.o \
    $(KMPP_OSAL_DIR)/kmpp_completion.o \
    $(KMPP_OSAL_DIR)/kmpp_delay.o \
    $(KMPP_OSAL_DIR)/kmpp_dev.o \
    $(KMPP_OSAL_DIR)/kmpp_dmabuf.o \
    $(KMPP_OSAL_DIR)/kmpp_dmaheap.o \
    $(KMPP_OSAL_DIR)/kmpp_env.o \
    $(KMPP_OSAL_DIR)/kmpp_file.o \
    $(KMPP_OSAL_DIR)/kmpp_io.o \
    $(KMPP_OSAL_DIR)/kmpp_log.o \
    $(KMPP_OSAL_DIR)/kmpp_mem.o \
    $(KMPP_OSAL_DIR)/kmpp_module.o \
    $(KMPP_OSAL_DIR)/kmpp_mutex.o \
    $(KMPP_OSAL_DIR)/kmpp_osal.o \
    $(KMPP_OSAL_DIR)/kmpp_osal_info.o \
    $(KMPP_OSAL_DIR)/kmpp_platform.o \
    $(KMPP_OSAL_DIR)/kmpp_poll.o \
    $(KMPP_OSAL_DIR)/kmpp_rwsem.o \
    $(KMPP_OSAL_DIR)/kmpp_sema.o \
    $(KMPP_OSAL_DIR)/kmpp_spinlock.o \
    $(KMPP_OSAL_DIR)/kmpp_sthd.o \
    $(KMPP_OSAL_DIR)/kmpp_string.o \
    $(KMPP_OSAL_DIR)/kmpp_thread.o \
    $(KMPP_OSAL_DIR)/kmpp_time.o \
    $(KMPP_OSAL_DIR)/kmpp_uaccess.o \
    $(KMPP_OSAL_DIR)/kmpp_wait.o \

ifeq ($(BUILD_MULTI_KO), y)
    obj-m += ${KMPP_OSAL_SRC}.o
endif
