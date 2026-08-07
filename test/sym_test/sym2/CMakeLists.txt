# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/sys/inc)

SET(SYM2_SRC
    sym2.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(sym2 SRC ${SYM2_SRC} DEPS kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(sym2 SRC ${SYM2_SRC} DEPS kmpp)
ENDIF()
