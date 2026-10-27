/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_PACKET_IMPL_H__
#define __KMPP_PACKET_IMPL_H__

#include "kmpp_packet.h"

#define KMPP_PACKET_FLAG_EOS        (0x00000001)
#define KMPP_PACKET_FLAG_EXTRA_DATA (0x00000002)
#define KMPP_PACKET_FLAG_INTERNAL   (0x00000004)
#define KMPP_PACKET_FLAG_EXTERNAL   (0x00000008)
#define KMPP_PACKET_FLAG_INTRA      (0x00000010)
#define KMPP_PACKET_FLAG_PARTITION  (0x00000020)
#define KMPP_PACKET_FLAG_EOI        (0x00000040)

typedef union KmppPacketStatus_t {
    rk_u32 val;
    struct {
        rk_u32 eos : 1;
        rk_u32 extra_data : 1;
        rk_u32 internal : 1;
        /* packet is inputed on reset mark as discard */
        rk_u32 discard : 1;

        /* for slice input output */
        rk_u32 partition : 1;
        rk_u32 soi : 1;
        rk_u32 eoi : 1;
    };
} KmppPacketStatus;

typedef struct KmppPacketImpl_t {
    const char *name;
    rk_s32 size;
    rk_s32 length;
    rk_s64 pts;
    rk_s64 dts;
    rk_u32 status;
    rk_u32 flag;
    rk_u32 temporal_id;
    KmppShmPtr data;
    KmppShmPtr pos;
    KmppShmPtr buffer;
    KmppShmPtr meta;
    /* leafkvm: per-frame encoder feedback, see KMPP_PACKET_ENTRY_TABLE */
    rk_u32 enc_valid;
    rk_s32 enc_bit_target;
    rk_s32 enc_bit_real;
    rk_s32 enc_qp_avg;
    rk_s32 enc_qp_target;
    rk_s32 enc_qp_min;
    rk_s32 enc_qp_max;
    rk_u32 enc_qp_out;
    rk_s32 enc_madi;
    rk_s32 enc_madp;
    rk_u32 enc_iblk4_prop;
    rk_u32 enc_motion_lvl;
    rk_u32 enc_cplx_lvl;
    rk_u32 enc_luma_ave;
    rk_u32 enc_rc_reinit;
    RingBuf buf;
} KmppPacketImpl;

#endif /* __KMPP_PACKET_IMPL_H__ */