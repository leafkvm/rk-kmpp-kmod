# vim: syntax=cmake

INCLUDE_DIRECTORIES(${KMPP_TOP}/osal)

SET(KMPP_OSAL_SRC
    kmpp_atomic.c
    kmpp_bitops.c
    kmpp_cls.c
    kmpp_completion.c
    kmpp_delay.c
    kmpp_dev.c
    kmpp_dmabuf.c
    kmpp_dmaheap.c
    kmpp_env.c
    kmpp_file.c
    kmpp_io.c
    kmpp_log.c
    kmpp_mem.c
    kmpp_module.c
    kmpp_mutex.c
    kmpp_osal.c
    kmpp_osal_info.c
    kmpp_platform.c
    kmpp_poll.c
    kmpp_rwsem.c
    kmpp_sema.c
    kmpp_spinlock.c
    kmpp_sthd.c
    kmpp_string.c
    kmpp_thread.c
    kmpp_time.c
    kmpp_uaccess.c
    kmpp_wait.c
)

IF(${BUILD_MULTI_KO})
    ADD_KBUILD(kmpp_osal SRC ${KMPP_OSAL_SRC})
ELSE()
    ADD_KMPP_SRC(${KMPP_OSAL_SRC})
ENDIF()
