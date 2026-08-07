# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/base/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/objs/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/test/kmpp_test)

SET(KMPP_TEST_SRC
    kmpp_test.c
    kmpp_objs_test.c
    kmpp_buffer_test.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(kmpp_test SRC ${KMPP_TEST_SRC} DEPS kmpp kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(kmpp_test SRC ${KMPP_TEST_SRC} DEPS kmpp)
ENDIF()
