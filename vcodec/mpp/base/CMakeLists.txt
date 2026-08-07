# vim: syntax=cmake

SET(VCODEC_BASE_SRC
    mpp_enc_refs.c
    mpp_enc_cfg.c
    mpp_bitwrite.c
    mpp_enc_ref.c
    mpp_enc_refs_setup.c
    mpp_buffer.c
    mpp_stream_ring_buf.c
    mpp_bitread.c
    mpp_2str.c
)

ADD_KMPP_SRC(${VCODEC_BASE_SRC})
