# vim: syntax=cmake
INCLUDE_DIRECTORIES(${KMPP_TOP}/osal/inc)

SET(OSAL_TEST_SRC
    osal_test.c
)

IF(${BUILD_MULTI_KO})
    ADD_KBUILD(osal_test SRC ${OSAL_TEST_SRC} DEPS kmpp_osal)
ELSE()
    ADD_KBUILD(osal_test SRC ${OSAL_TEST_SRC} DEPS kmpp)
ENDIF()
