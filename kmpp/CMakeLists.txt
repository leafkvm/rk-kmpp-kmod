# vim: syntax=cmake

ADD_SUBDIRECTORY(vdec)
ADD_SUBDIRECTORY(hal)

SET(KMPP_SRC
    kmpp.c
    kmpi/kmpi_vdec.c
    objs/kmpp_buffer.c
    objs/kmpp_frame.c
    objs/kmpp_packet.c
    objs/kmpp_meta.c
    objs/kmpp_venc_objs.c
    objs/kmpp_vdec_objs.c
    objs/kmpp_vsp_objs.c
    venc/kmpp_venc_ioctl.c
    venc/kmpp_venc.c
    vsp/kmpp_vsp.c
    vsp/kmpi_vsp.c
)

ADD_KMPP_SRC(${KMPP_SRC})
