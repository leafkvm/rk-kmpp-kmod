# vim: syntax=cmake

INCLUDE(${PROJECT_SOURCE_DIR}/build/cmake/kernel.build.cmake)

ADD_MODULE_CFLAGS(-DUSE_RING_BUF)
ADD_MODULE_CFLAGS(-DHAVE_H264E)
ADD_MODULE_CFLAGS(-DHAVE_H265E)
ADD_MODULE_CFLAGS(-DRKVEPU510_H264)
ADD_MODULE_CFLAGS(-DRKVEPU510_HEVC)
ADD_MODULE_CFLAGS(-DRKVEPU511_H264)
ADD_MODULE_CFLAGS(-DRKVEPU511_HEVC)

SET(VCODEC_SRC
    mpp_vcodec_chan.c
    mpp_vcodec_flow.c
    mpp_vcodec_driver.c
    mpp_vcodec_base.c
    osal/osal_mpp_service.c
    osal/mpp_device.c
    osal/mpp_maths.c
    mpp_vcodec_thread.c
)

ADD_KMPP_SRC(${VCODEC_SRC})

ADD_SUBDIRECTORY(kmpi)
ADD_SUBDIRECTORY(mpp/base)
ADD_SUBDIRECTORY(mpp/codec)
ADD_SUBDIRECTORY(mpp/hal)
