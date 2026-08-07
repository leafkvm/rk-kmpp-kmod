# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/sys/inc)

SET(SYM3_SRC
    sym3.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(sym3 SRC ${SYM3_SRC} DEPS kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(sym3 SRC ${SYM3_SRC} DEPS kmpp)
ENDIF()
