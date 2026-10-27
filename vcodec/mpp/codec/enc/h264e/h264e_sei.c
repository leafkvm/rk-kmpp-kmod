/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "h264e_sei"

#include "kmpp_string.h"

#include "mpp_maths.h"

#include "mpp_bitwrite.h"
#include "h264e_debug.h"

#include "h264_syntax.h"
#include "h264e_sei.h"

static MPP_RET write_recovery_point(MppWriteCtx *bit, RK_U32 recovery_frame_cnt)
{
    mpp_writer_put_ue(bit, recovery_frame_cnt);
    mpp_writer_put_bits(bit, 1, 1);
    mpp_writer_put_bits(bit, 0, 1);
    mpp_writer_put_bits(bit, 0, 2);
    mpp_writer_trailing(bit);

    return MPP_OK;
}

MPP_RET h264e_sei_recovery_point_to_packet(KmppPacket packet, RK_S32 *len, RK_U32 recovery_frame_cnt)
{
    MppWriteCtx bit_ctx;
    MppWriteCtx *bit = &bit_ctx;
    RK_U8 src[100] = {0};
    KmppShmPtr pos;
    KmppShmPtr pkt_base;
    RK_S32 pkt_size;
    RK_S32 length;
    void *dst;
    RK_S32 buf_size;
    RK_S32 payload_size = 0;
    RK_S32 type = H264_SEI_RECOVERY_POINT;
    RK_S32 sei_size = 0;
    MPP_RET ret = MPP_OK;
    RK_S32 i;

    kmpp_packet_get_pos(packet, &pos);
    kmpp_packet_get_data(packet, &pkt_base);
    kmpp_packet_get_size(packet, &pkt_size);
    kmpp_packet_get_length(packet, &length);
    dst = pos.kptr + length;
    buf_size = (pkt_base.kptr + pkt_size) - (pos.kptr + length);

    mpp_writer_init(bit, src, 100);
    write_recovery_point(bit, recovery_frame_cnt);
    payload_size = mpp_writer_bytes(bit);

    mpp_writer_init(bit, dst, buf_size);

    /* start_code_prefix 00 00 00 01 */
    mpp_writer_put_raw_bits(bit, 0, 24);
    mpp_writer_put_raw_bits(bit, 1, 8);
    /* forbidden_zero_bit */
    mpp_writer_put_raw_bits(bit, 0, 1);
    /* nal_ref_idc */
    mpp_writer_put_raw_bits(bit, H264_NALU_PRIORITY_DISPOSABLE, 2);
    /* nal_unit_type */
    mpp_writer_put_raw_bits(bit, H264_NALU_TYPE_SEI, 5);

    /* sei_payload_type_ff_byte */
    for (i = 0; i <= type - 255; i += 255)
        mpp_writer_put_bits(bit, 0xff, 8);

    /* sei_last_payload_type_byte */
    mpp_writer_put_bits(bit, type - i, 8);

    /* sei_payload_size_ff_byte */
    for (i = 0; i <= payload_size - 255; i += 255)
        mpp_writer_put_bits(bit, 0xff, 8);

    /* sei_last_payload_size_byte */
    mpp_writer_put_bits(bit, payload_size - i, 8);

    write_recovery_point(bit, recovery_frame_cnt);
    /*
     * leafkvm: rbsp_trailing_bits of the SEI NAL. write_recovery_point() ends with
     * the payload's own byte alignment, so without this the NAL's stop bit is the
     * payload's alignment bit and the payload reads one bit short: FFmpeg logs
     * "SEI type 6 size 2 truncated at 1" and, with AV_EF_EXPLODE (Chrome's
     * software H.264 decoder), fails that frame, once per GDR cycle.
     */
    mpp_writer_trailing(bit);

    sei_size = mpp_writer_bytes(bit);
    if (len)
        *len = sei_size;

    kmpp_packet_set_length(packet, length + sei_size);

    h264e_dbg_sei("sei data length %d pkt len %d -> %d\n", sei_size,
                  length, length + sei_size);

    return ret;

}

MPP_RET h264e_sei_to_packet(KmppPacket packet, RK_S32 *len, RK_S32 type,
                            RK_U8 uuid[16], const void *data, RK_S32 size)
{
    KmppShmPtr pos;
    KmppShmPtr pkt_base;
    RK_S32 pkt_size;
    RK_S32 length;
    const RK_U8 *src = (RK_U8 *)data;
    void *dst = NULL;
    RK_S32 buf_size;
    MppWriteCtx bit_ctx;
    MppWriteCtx *bit = &bit_ctx;
    RK_S32 uuid_size = uuid ? 16 : 0;
    RK_S32 payload_size = size + uuid_size;
    RK_S32 sei_size = 0;
    RK_S32 i;

    kmpp_packet_get_pos(packet, &pos);
    kmpp_packet_get_data(packet, &pkt_base);
    kmpp_packet_get_size(packet, &pkt_size);
    kmpp_packet_get_length(packet, &length);
    dst = pos.kptr + length;
    buf_size = (pkt_base.kptr + pkt_size) - (pos.kptr + length);

    h264e_dbg_sei("write sei to pkt [%p:%d] [%p:%d]\n", pkt_base.kptr, pkt_size,
                  pos.kptr, length);

    mpp_writer_init(bit, dst, buf_size);

    /* start_code_prefix 00 00 00 01 */
    mpp_writer_put_raw_bits(bit, 0, 24);
    mpp_writer_put_raw_bits(bit, 1, 8);
    /* forbidden_zero_bit */
    mpp_writer_put_raw_bits(bit, 0, 1);
    /* nal_ref_idc */
    mpp_writer_put_raw_bits(bit, H264_NALU_PRIORITY_DISPOSABLE, 2);
    /* nal_unit_type */
    mpp_writer_put_raw_bits(bit, H264_NALU_TYPE_SEI, 5);

    /* sei_payload_type_ff_byte */
    for (i = 0; i <= type - 255; i += 255)
        mpp_writer_put_bits(bit, 0xff, 8);

    /* sei_last_payload_type_byte */
    mpp_writer_put_bits(bit, type - i, 8);

    /* sei_payload_size_ff_byte */
    for (i = 0; i <= payload_size - 255; i += 255)
        mpp_writer_put_bits(bit, 0xff, 8);

    /* sei_last_payload_size_byte */
    mpp_writer_put_bits(bit, payload_size - i, 8);

    /* uuid_iso_iec_11578 */
    for (i = 0; i < uuid_size; i++)
        mpp_writer_put_bits(bit, uuid[i], 8);

    /* sei_payload_data */
    for (i = 0; i < size; i++)
        mpp_writer_put_bits(bit, src[i], 8);

    mpp_writer_trailing(bit);

    sei_size = mpp_writer_bytes(bit);
    if (len)
        *len = sei_size;

    kmpp_packet_set_length(packet, length + sei_size);

    h264e_dbg_sei("sei data length %u pkt len %d -> %d\n", sei_size,
                  length, length + sei_size);

    return MPP_OK;
}
