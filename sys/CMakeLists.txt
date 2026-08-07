# vim: syntax=cmake
INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(inc)

SET(KMPP_SYS_SRC
    kmpp_dq.c
    kmpp_ioctl.c
    kmpp_mem_pool.c
    kmpp_obj.c
    kmpp_shm.c
    kmpp_sym.c
    kmpp_sys.c
    kmpp_sys_info.c
    kmpp_trie.c
)

IF(${BUILD_MULTI_KO})
    ADD_KBUILD(kmpp_sys SRC ${KMPP_SYS_SRC} DEPS kmpp_osal)
ELSE()
    ADD_KMPP_SRC(${KMPP_SYS_SRC})
ENDIF()
