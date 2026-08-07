# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/sys/inc)

SET(SYM1_SRC
    sym1.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(sym1 SRC ${SYM1_SRC} DEPS kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(sym1 SRC ${SYM1_SRC} DEPS kmpp)
ENDIF()
