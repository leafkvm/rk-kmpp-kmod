# vim: syntax=cmake
SET(VCODEC_CODEC_RC_SRC
    mpp_rc.c
    h265e_rc.c
    h264e_rc.c
    jpege_rc.c
    rc_model.c
    rc_base.c
    rc_api.c
    rc.c
)
ADD_KMPP_SRC(${VCODEC_CODEC_RC_SRC})
