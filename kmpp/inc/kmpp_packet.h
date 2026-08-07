/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_PACKET_H__
#define __KMPP_PACKET_H__

#include "kmpi_defs.h"
#include "mpp_stream_ring_buf.h"

#define KMPP_PACKET_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, s32, rk_s32,      size,           FLAG_NONE,  size) \
    ENTRY(prefix, s32, rk_s32,      length,         FLAG_NONE,  length) \
    ENTRY(prefix, s64, rk_s64,      pts,            FLAG_NONE,  pts) \
    ENTRY(prefix, s64, rk_s64,      dts,            FLAG_NONE,  dts) \
    ENTRY(prefix, u32, rk_u32,      status,         FLAG_NONE,  status) \
    ENTRY(prefix, u32, rk_u32,      temporal_id,    FLAG_NONE,  temporal_id) \
    STRCT(prefix, shm, KmppShmPtr,  data,           FLAG_NONE,  data) \
    SHOOK(prefix, shm, KmppShmPtr,  pos,            FLAG_NONE,  pos) \
    SHOOK(prefix, shm, KmppShmPtr,  buffer,         FLAG_NONE,  buffer) \
    STRCT(prefix, shm, KmppShmPtr,  meta,           FLAG_NONE,  meta) \
    EHOOK(prefix, u32, rk_u32,      flag,           FLAG_NONE,  flag)

#define KMPP_OBJ_NAME           kmpp_packet
#define KMPP_OBJ_INTF_TYPE      KmppPacket
#define KMPP_OBJ_ENTRY_TABLE    KMPP_PACKET_ENTRY_TABLE
#include "kmpp_obj_func.h"

#define kmpp_packet_dump_f(packet) kmpp_packet_dump(packet, __FUNCTION__)

rk_s32 kmpp_packet_init_with_data(KmppPacket *packet, void *data, size_t size);
rk_s32 kmpp_packet_init_with_buffer(KmppPacket * packet, KmppShmPtr buffer);
rk_s32 kmpp_packet_has_buffer(KmppPacket packet);
rk_s32 kmpp_packet_set_eos(KmppPacket packet);
rk_s32 kmpp_packet_clr_eos(KmppPacket packet);
rk_s32 kmpp_packet_get_eos(KmppPacket packet);
rk_u32 kmpp_packet_is_partition(const KmppPacket packet);
rk_u32 kmpp_packet_is_soi(const KmppPacket packet);
rk_u32 kmpp_packet_is_eoi(const KmppPacket packet);
rk_s32 kmpp_packet_read(KmppPacket packet, size_t offset, void *data, size_t size);
rk_s32 kmpp_packet_write(KmppPacket packet, size_t offset, void *data, size_t size);
rk_s32 kmpp_packet_copy(KmppPacket dst, KmppPacket src);
rk_s32 kmpp_packet_append(KmppPacket dst, KmppPacket src);

#endif /*__KMPP_PACKET_H__*/
