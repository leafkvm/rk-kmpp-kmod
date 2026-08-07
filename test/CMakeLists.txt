# vim: syntax=cmake
option(BUILD_OSAL_TEST "Build kernel mpp osal test" ON)
IF(${BUILD_OSAL_TEST})
    ADD_SUBDIRECTORY(osal_test)
ENDIF(${BUILD_OSAL_TEST})

option(BUILD_KMPP_TEST "Build kernel mpp kmpp test" ON)
IF(${BUILD_KMPP_TEST})
    ADD_SUBDIRECTORY(kmpp_test)
ENDIF(${BUILD_KMPP_TEST})

option(BUILD_KMPI_ENC_TEST "Build kernel mpp kmpi enc test" ON)
IF(${BUILD_KMPI_ENC_TEST})
    ADD_SUBDIRECTORY(kmpi_enc_test)
ENDIF(${BUILD_KMPI_ENC_TEST})

option(BUILD_KMPP_ENC_TEST "Build kernel mpp kmpp enc test" ON)
IF(${BUILD_KMPP_ENC_TEST})
    ADD_SUBDIRECTORY(kmpp_enc_test)
ENDIF(${BUILD_KMPP_ENC_TEST})

option(BUILD_KMPI_ENC_CHAN_TEST "Build kernel mpp kmpp enc chan test" ON)
IF(${BUILD_KMPI_ENC_CHAN_TEST})
    ADD_SUBDIRECTORY(kmpi_enc_chan_test)
ENDIF(${BUILD_KMPI_ENC_CHAN_TEST})

option(BUILD_KMPI_VDEC_TEST "Build kernel mpp kmpi vdec test" ON)
IF(${BUILD_KMPI_VDEC_TEST})
    ADD_SUBDIRECTORY(kmpi_vdec_test)
ENDIF(${BUILD_KMPI_VDEC_TEST})

option(BUILD_KMPI_VSP_TEST "Build kernel mpp kmpi vsp test" ON)
IF(${BUILD_KMPI_VSP_TEST})
    ADD_SUBDIRECTORY(kmpi_vsp_test)
ENDIF(${BUILD_KMPI_VSP_TEST})

option(BUILD_SYM_TEST "Build kernel mpp cross module symbol test" ON)
IF(${BUILD_SYM_TEST})
    ADD_SUBDIRECTORY(sym_test)
ENDIF(${BUILD_SYM_TEST})

# test stub ko build for stub header
option(BUILD_STUB "Build kernel mpp stub test" OFF)
IF(${BUILD_STUB})
    ADD_SUBDIRECTORY(stub_test)
ENDIF(${BUILD_STUB})
