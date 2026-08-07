# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/base/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/osal/inc)

SET(KMPP_ENC_TEST_SRC
    kmpp_enc_test.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(kmpp_enc_test SRC ${KMPP_ENC_TEST_SRC} DEPS kmpp kmpp_sys kmpp_osal)
ELSE()
    ADD_KBUILD(kmpp_enc_test SRC ${KMPP_ENC_TEST_SRC} DEPS kmpp)
ENDIF()
