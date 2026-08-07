# vim: syntax=cmake
SET(VCODEC_HAL_SRC
    mpp_enc_hal.c
    rkenc/h264e/hal_h264e_scaling_list.c
    # rkenc/h264e/hal_h264e_vepu580.c
    rkenc/h264e/hal_h264e_vepu510.c
    rkenc/h264e/hal_h264e_vepu511.c
    # rkenc/h264e/hal_h264e_vepu540c.c
    # rkenc/h264e/hal_h264e_vepu500.c
    rkenc/common/vepu541_common.c
    rkenc/common/vepu510_common.c
    rkenc/common/vepu511_common.c
    rkenc/common/vepu5xx_common.c
    rkenc/common/vepu540c_common.c
    rkenc/common/vepu500_common.c
    # rkenc/h265e/hal_h265e_vepu580.c
    rkenc/h265e/hal_h265e_vepu510.c
    rkenc/h265e/hal_h265e_vepu511.c
    # rkenc/h265e/hal_h265e_vepu500.c
    # rkenc/h265e/hal_h265e_vepu541.c
    rkenc/h265e/hal_h265e_scaling_list.c
    # rkenc/h265e/hal_h265e_vepu540c.c
    # rkenc/jpege/hal_jpege_vepu540c.c
    rkenc/jpege/hal_jpege_vepu500.c
    rkenc/jpege/hal_jpege_vepu511.c
    rkenc/jpege/hal_jpege_vpu720.c
    common/h264/hal_h264e_api_v2.c
    common/h265/hal_h265e_api_v2.c
    common/hal_bufs.c
    common/jpeg/hal_jpege_api_v2.c
    common/jpeg/hal_jpege_hdr.c
)
ADD_KMPP_SRC(${VCODEC_HAL_SRC})
