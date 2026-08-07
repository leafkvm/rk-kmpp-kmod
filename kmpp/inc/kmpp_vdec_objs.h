/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VDEC_OBJS_H__
#define __KMPP_VDEC_OBJS_H__

#include "rk_defs.h"
#include "kmpi_vdec.h"

#define KMPP_VDEC_INIT_CFG_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    CFG_DEF_START() \
    STRUCT_START(base) \
    ENTRY(prefix, u32, rk_u32,     coding,              FLAG_BASE(0),   base, coding) \
    ENTRY(prefix, u32, rk_u32,     hw_type,             FLAG_AT(1),     base, hw_type) \
    ENTRY(prefix, u32, rk_u32,     batch_mode,          FLAG_AT(2),     base, batch_mode) \
    ENTRY(prefix, u32, rk_u32,     out_fmt,             FLAG_AT(3),     base, out_fmt) \
    ENTRY(prefix, u32, rk_u32,     fast_out,            FLAG_AT(4),     base, fast_out) \
    ENTRY(prefix, u32, rk_u32,     fast_parse,          FLAG_AT(5),     base, fast_parse) \
    ENTRY(prefix, u32, rk_u32,     split_parse,         FLAG_AT(6),     base, split_parse) \
    ENTRY(prefix, u32, rk_u32,     internal_pts,        FLAG_AT(7),     base, internal_pts) \
    ENTRY(prefix, u32, rk_u32,     sort_pts,            FLAG_AT(8),     base, sort_pts) \
    ENTRY(prefix, u32, rk_u32,     disable_error,       FLAG_AT(9),     base, disable_error) \
    ENTRY(prefix, u32, rk_u32,     enable_vproc,        FLAG_AT(10),    base, enable_vproc) \
    ENTRY(prefix, u32, rk_u32,     enable_fast_play,    FLAG_AT(11),    base, enable_fast_play) \
    ENTRY(prefix, u32, rk_u32,     enable_hdr_meta,     FLAG_AT(12),    base, enable_hdr_meta) \
    ENTRY(prefix, u32, rk_u32,     enable_thumbnail,    FLAG_AT(13),    base, enable_thumbnail) \
    ENTRY(prefix, u32, rk_u32,     enable_mvc,          FLAG_AT(14),    base, enable_mvc) \
    ENTRY(prefix, u32, rk_u32,     disable_dpb_chk,     FLAG_AT(15),    base, disable_dpb_chk) \
    ENTRY(prefix, u32, rk_u32,     disable_thread,      FLAG_AT(16),    base, disable_thread) \
    ENTRY(prefix, u32, rk_u32,     codec_mode,          FLAG_AT(17),    base, codec_mode) \
    ENTRY(prefix, u32, rk_u32,     dis_err_clr_mark,    FLAG_AT(18),    base, dis_err_clr_mark) \
    STRUCT_END(base) \
    STRUCT_START(input) \
    ENTRY(prefix, s32, rk_s32,     timeout,             FLAG_BASE(0),   input, timeout) \
    ENTRY(prefix, s32, rk_s32,     queue_size,          FLAG_INCR,      input, queue_size) \
    ENTRY(prefix, u32, rk_u32,     pkt_buf_size,        FLAG_INCR,      base, pkt_buf_size) \
    ENTRY(prefix, u32, rk_u32,     enable_ringbuf,      FLAG_INCR,      base, enable_ringbuf) \
    STRUCT_END(input) \
    STRUCT_START(output) \
    ENTRY(prefix, s32, rk_s32,     timeout,             FLAG_BASE(0),   output, timeout) \
    ENTRY(prefix, s32, rk_s32,     queue_size,          FLAG_INCR,      output, queue_size) \
    STRUCT_END(output) \
    CFG_DEF_END()

#define KMPP_OBJ_NAME           kmpp_vdec_init_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVdecInitCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_VDEC_INIT_CFG_ENTRY_TABLE
#define KMPP_OBJ_ACCESS_DISABLE
#include "kmpp_obj_func.h"

#define kmpp_vdec_init_cfg_dump_f(cfg) kmpp_vdec_init_cfg_dump(cfg, __FUNCTION__)

#define KMPP_OBJ_NAME           kmpp_vdec_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVdecCfg
#define KMPP_OBJ_ACCESS_DISABLE
#include "kmpp_obj_func.h"

#endif /*__KMPP_VDEC_OBJS_H__*/
