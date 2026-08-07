# vim: syntax=cmake
SET(VCODEC_CODEC_SRC
    enc_impl.c
    mpp_enc.c
    enc/h264e/h264e_pps.c
    enc/h264e/h264e_api_v2.c
    enc/h264e/h264e_dpb.c
    enc/h264e/h264e_sps.c
    enc/h264e/h264e_sei.c
    enc/h264e/h264e_slice.c
    enc/jpeg/jpege_api_v2.c
    enc/h265e/h265e_ps.c
    enc/h265e/h265e_api.c
    enc/h265e/h265e_dpb.c
    enc/h265e/h265e_header_gen.c
    enc/h265e/h265e_enctropy.c
    enc/h265e/h265e_stream.c
    enc/h265e/h265e_syntax.c
    enc/h265e/h265e_slice.c
    mpp_enc_impl.c
)
ADD_KMPP_SRC(${VCODEC_CODEC_SRC})

ADD_SUBDIRECTORY(rc)
