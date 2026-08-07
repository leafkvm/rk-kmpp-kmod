# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/base/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/test/kmpi_vdec_test)

SET(KMPI_VDEC_TEST_SRC
    kmpi_vdec_test.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(kmpi_vdec_test SRC ${KMPI_VDEC_TEST_SRC} DEPS kmpp kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(kmpi_vdec_test SRC ${KMPI_VDEC_TEST_SRC} DEPS kmpp)
ENDIF()
