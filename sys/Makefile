KMPP_SYS_DIR := sys
EXTRA_CFLAGS += -I$(TOP)/sys/inc
EXTRA_CFLAGS += -I$(TOP)/sys

${KMPP_SYS_SRC}-objs += \
    $(KMPP_SYS_DIR)/kmpp_dq.o \
    $(KMPP_SYS_DIR)/kmpp_ioctl.o \
    $(KMPP_SYS_DIR)/kmpp_mem_pool.o \
    $(KMPP_SYS_DIR)/kmpp_obj.o \
    $(KMPP_SYS_DIR)/kmpp_shm.o \
    $(KMPP_SYS_DIR)/kmpp_sym.o \
    $(KMPP_SYS_DIR)/kmpp_sys.o \
    $(KMPP_SYS_DIR)/kmpp_sys_info.o \
    $(KMPP_SYS_DIR)/kmpp_trie.o

ifeq ($(BUILD_MULTI_KO), y)
    obj-m += ${KMPP_SYS_SRC}.o
endif
