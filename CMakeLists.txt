# ----------------------------------------------------------------------------
#  Root CMake file for Rockchip Media Process Platform (MPP)
# ----------------------------------------------------------------------------

# vim: syntax=cmake
IF(NOT CMAKE_BUILD_TYPE)
    # default to Release build for GCC builds
    SET(CMAKE_BUILD_TYPE DEBUG CACHE STRING
        "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

CMAKE_MINIMUM_REQUIRED(VERSION 3.12.0)
PROJECT(rk_mpp C)

# ----------------------------------------------------------------------------
# export json compile commands
# ----------------------------------------------------------------------------
SET(CMAKE_EXPORT_COMPILE_COMMANDS true)

# ----------------------------------------------------------------------------
# include common cmake files
# ----------------------------------------------------------------------------
INCLUDE(CMakePrintHelpers)

# ----------------------------------------------------------------------------
# include project cmake files
# ----------------------------------------------------------------------------
INCLUDE(${PROJECT_SOURCE_DIR}/build/cmake/version.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/build/cmake/kernel.build.cmake)

# ----------------------------------------------------------------------------
# include share common headers
# ----------------------------------------------------------------------------
INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(mpp_osal)
INCLUDE_DIRECTORIES(mpp_service)
INCLUDE_DIRECTORIES(mpp_service/hack)
INCLUDE_DIRECTORIES(mpp_osal/inc)
INCLUDE_DIRECTORIES(osal/inc)
INCLUDE_DIRECTORIES(sys/inc)
INCLUDE_DIRECTORIES(dvbm)
INCLUDE_DIRECTORIES(legacy/inc)
INCLUDE_DIRECTORIES(${KERNEL_DIR})
INCLUDE_DIRECTORIES(${KERNEL_DIR}/include/linux)
INCLUDE_DIRECTORIES(${KERNEL_DIR}/include/uapi)
INCLUDE_DIRECTORIES(${KERNEL_DIR}/drivers/iommu)

SET(ENV{PATH} "${TOOLCHAIN_DIR}:$ENV{PATH}")
SET(KMPP_TOP ${PROJECT_SOURCE_DIR})

# ----------------------------------------------------------------------------
# check ko compile mode
# ----------------------------------------------------------------------------
IF(NOT MODULE_BUILD_MODE)
    SET(MODULE_BUILD_MODE "multi_ko")
ENDIF()

OPTION(BUILD_MULTI_KO   "Build multiple ko"         OFF)
OPTION(BUILD_ONE_KO     "Build single ko"           OFF)
OPTION(BUILD_NO_KO      "Build no ko and builtin"   OFF)

SET_PROPERTY(GLOBAL PROPERTY KMPP_KO_SRC "")
SET_PROPERTY(GLOBAL PROPERTY KMPP_KO_LDFLAG "")

IF(${MODULE_BUILD_MODE} STREQUAL "multi")
    SET(BUILD_MULTI_KO ON)
    ADD_DEFINITIONS(-DBUILD_MULTI_KO)
ELSEIF(${MODULE_BUILD_MODE} STREQUAL "one")
    SET(BUILD_ONE_KO ON)
    ADD_DEFINITIONS(-DBUILD_ONE_KO)
ELSEIF(${MODULE_BUILD_MODE} STREQUAL "builtin")
    SET(BUILD_NO_KO ON)
    ADD_DEFINITIONS(-DBUILD_NO_KO)
ELSE()
    MESSAGE(STATUS "invalid build mode - ${MODULE_BUILD_MODE}")
ENDIF()
MESSAGE(STATUS "module build mode - ${MODULE_BUILD_MODE}")

# ----------------------------------------------------------------------------
# check build config
# ----------------------------------------------------------------------------
CHECK_KERNEL_CONFIG("${KERNEL_DIR}" "CONFIG_CPU_RV1106" "CONFIG_CPU_RV1106_ENABLED")
IF(CONFIG_CPU_RV1106_ENABLED)
    MESSAGE(STATUS "CONFIG_CPU_RV1106 is enabled (y or m)")
    SET(VEPU_CORE "RKVEPU540C")
ENDIF()

CHECK_KERNEL_CONFIG("${KERNEL_DIR}" "CONFIG_CPU_RV1103B" "CONFIG_CPU_RV1103B_ENABLED")
IF(CONFIG_CPU_RV1103B_ENABLED)
    MESSAGE(STATUS "CONFIG_CPU_RV1103B is enabled (y or m)")
    SET(VEPU_CORE "RKVEPU500")
ENDIF()

CHECK_KERNEL_CONFIG("${KERNEL_DIR}" "CONFIG_CPU_RK3576" "CONFIG_CPU_RK3576_ENABLED")
IF(CONFIG_CPU_RK3576_ENABLED)
    MESSAGE(STATUS "CONFIG_CPU_RK3576 is enabled (y or m)")
    SET(VEPU_CORE "RKVEPU510")
ENDIF()

CHECK_KERNEL_CONFIG("${KERNEL_DIR}" "CONFIG_CPU_RV1126B" "CONFIG_CPU_RV1126B_ENABLED")
IF(CONFIG_CPU_RV1126B_ENABLED)
    MESSAGE(STATUS "CONFIG_CPU_RV1126B is enabled (y or m)")
    SET(VEPU_CORE "RKVEPU511")
ENDIF()

# ----------------------------------------------------------------------------
# Add all modules
# ----------------------------------------------------------------------------
# OPTION(BUILD_DVBM "Build kernel dvbm modules" ON)
# IF(${BUILD_DVBM})
#     ADD_SUBDIRECTORY(dvbm)
# ENDIF(${BUILD_DVBM})

OPTION(BUILD_MPP_OSAL "Build kernel mpp_osal modules" ON)
IF(${BUILD_MPP_OSAL})
    ADD_SUBDIRECTORY(mpp_osal)
ENDIF(${BUILD_MPP_OSAL})

OPTION(BUILD_OSAL "Build kernel osal modules" ON)
IF(${BUILD_OSAL})
    ADD_SUBDIRECTORY(osal)
ENDIF(${BUILD_OSAL})

OPTION(BUILD_SYS "Build kernel sys modules" ON)
IF(${BUILD_SYS})
    ADD_SUBDIRECTORY(sys)
ENDIF(${BUILD_SYS})

OPTION(BUILD_KMPP "Build kernel kmpp modules" ON)
IF(${BUILD_KMPP})
    ADD_SUBDIRECTORY(kmpp)
ENDIF(${BUILD_KMPP})

OPTION(BUILD_VPROC "Build kernel vproc modules" ON)
IF(${BUILD_VPROC})
    ADD_SUBDIRECTORY(vproc)
ENDIF(${BUILD_VPROC})

OPTION(BUILD_VCODEC "Build kernel vcodec modules" ON)
IF(${BUILD_VCODEC})
    ADD_SUBDIRECTORY(vcodec)
ENDIF(${BUILD_VCODEC})

OPTION(BUILD_MPP_SERVICE "Build kernel mpp_service modules" ON)
IF(${BUILD_MPP_SERVICE})
    ADD_SUBDIRECTORY(mpp_service)
ENDIF(${BUILD_MPP_SERVICE})

# build kmpp.ko
INCLUDE_DIRECTORIES(kmpp)
INCLUDE_DIRECTORIES(kmpp/inc)
INCLUDE_DIRECTORIES(kmpp/base/inc)
INCLUDE_DIRECTORIES(kmpp/objs/inc)
INCLUDE_DIRECTORIES(kmpp/venc)
INCLUDE_DIRECTORIES(kmpp/vdec)
INCLUDE_DIRECTORIES(kmpp/vdec/h264)
INCLUDE_DIRECTORIES(kmpp/vdec/h265)
INCLUDE_DIRECTORIES(kmpp/vdec/jpeg)
INCLUDE_DIRECTORIES(kmpp/vdec/hal)
INCLUDE_DIRECTORIES(kmpp/vdec/hal/h264)
INCLUDE_DIRECTORIES(kmpp/vdec/hal/h265)
INCLUDE_DIRECTORIES(kmpp/vdec/hal/jpeg)
INCLUDE_DIRECTORIES(kmpp/vsp/inc)
INCLUDE_DIRECTORIES(kmpp/hal/inc)
INCLUDE_DIRECTORIES(test/kmpi_enc_test)
INCLUDE_DIRECTORIES(vproc/pp)
INCLUDE_DIRECTORIES(vcodec)
INCLUDE_DIRECTORIES(vcodec/inc)
INCLUDE_DIRECTORIES(vcodec/osal/inc)
INCLUDE_DIRECTORIES(vcodec/mpp/inc)
INCLUDE_DIRECTORIES(vcodec/mpp/common)
INCLUDE_DIRECTORIES(vcodec/mpp/base/inc)
INCLUDE_DIRECTORIES(vcodec/mpp/codec)
INCLUDE_DIRECTORIES(vcodec/mpp/codec/inc)
INCLUDE_DIRECTORIES(vcodec/mpp/codec/rc)
INCLUDE_DIRECTORIES(vcodec/mpp/codec/enc/h264e)
INCLUDE_DIRECTORIES(vcodec/mpp/codec/enc/h265e)
INCLUDE_DIRECTORIES(vcodec/mpp/codec/enc/jpeg)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/inc)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/common)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/common/h264)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/common/h265)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/common/jpeg)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/rkenc/common)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/rkenc/h264e)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/rkenc/h265e)
INCLUDE_DIRECTORIES(vcodec/mpp/hal/rkenc/jpege)

OPTION(BUILD_KMPP_KO "Build kernel kmpp ko modules" ON)
IF(${BUILD_KMPP_KO})
    GET_PROPERTY(ko_src GLOBAL PROPERTY "KMPP_KO_SRC")
    GET_PROPERTY(ko_ldflag GLOBAL PROPERTY "KMPP_KO_LDFLAG")
    IF(${BUILD_MULTI_KO})
        ADD_KBUILD(kmpp SRC ${ko_src} LDFLAG ${ko_ldflag} DEPS kmpp_osal kmpp_sys)
    ELSE()
        INCLUDE_DIRECTORIES(sys)
        INCLUDE_DIRECTORIES(osal)
        ADD_KBUILD(kmpp SRC ${ko_src} LDFLAG ${ko_ldflag})
    ENDIF()
ENDIF(${BUILD_KMPP_KO})

OPTION(BUILD_TEST "Build kernel mpp test" ON)
IF(${BUILD_TEST})
    ADD_SUBDIRECTORY(test)
ENDIF(${BUILD_TEST})

IF(EXISTS "${PROJECT_SOURCE_DIR}/smt")
    ADD_SUBDIRECTORY(smt)
ENDIF()
