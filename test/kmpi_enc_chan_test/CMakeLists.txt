# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/kmpp/base/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/osal/inc)
INCLUDE_DIRECTORIES(${KMPP_TOP}/vcodec/mpp/codec/inc)

SET(KMPI_ENC_CHAN_TEST_SRC
    kmpi_enc_chan_test.c
)

IF(BUILD_MULTI_KO)
    ADD_KBUILD(kmpi_enc_chan_test SRC ${KMPI_ENC_CHAN_TEST_SRC} DEPS kmpp_osal kmpp_sys kmpp)
ELSE()
    ADD_KBUILD(kmpi_enc_chan_test SRC ${KMPI_ENC_CHAN_TEST_SRC} DEPS kmpp)
ENDIF()
