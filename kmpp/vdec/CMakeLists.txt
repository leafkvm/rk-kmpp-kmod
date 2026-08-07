# vim: syntax=cmake

ADD_SUBDIRECTORY(h264)
ADD_SUBDIRECTORY(h265)
ADD_SUBDIRECTORY(jpeg)
ADD_SUBDIRECTORY(hal)

SET(KMPP_VDEC_SRC
    kmpp_vdec_ioctl.c
    kmpp_vdec.c
    vdec_parser.c
    vdec_hal.c
)

ADD_KMPP_SRC(${KMPP_VDEC_SRC})
