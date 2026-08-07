# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/sys/inc)

SET(SYM4_SRC
    sym4.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(sym4 SRC ${SYM4_SRC} DEPS kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(sym4 SRC ${SYM4_SRC} DEPS kmpp)
ENDIF()
