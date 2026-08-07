
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __VDEC_PARSER_H__
#define __VDEC_PARSER_H__

#include "kmpp_vdec.h"

#define MAX_DEC_REF_NUM 17

typedef struct ParserCfg_t {
    /* input */
    KmppVdec        vdec;
    MppCodingType   coding;
    rk_s32          task_req;

    /* output */
    rk_u64          flag;
    rk_s32          ctx_size;
    rk_s32          task_cnt;

    /* enable hardware ring buffer */
    rk_u32          enable_hw_ring_buf;
} ParserCfg;

/* parser input job header */
struct ParserJob_t {
    /* valid packet with ring buffer user should copy data to buffer and update valid length */
    KmppPacket      hw_buf;
    /* input packet with or without KmppBuffer */
    KmppPacket      pkt;
};

typedef struct ParserOutStatus_t {
    rk_u64          val;
    struct {
        rk_u32      eos              : 1;
        rk_u32      info_change      : 1;

        /*
         * Different error flags for task
         *
         * parse_err :
         * When set it means fatal error happened at parsing stage
         * This task should not enable hardware just output a empty frame with
         * error flag.
         *
         * ref_err :
         * When set it means current task is ok but it contains reference frame
         * with error which will introduce error pixels to this frame.
         *
         * used_for_ref :
         * When set it means this output frame will be used as reference frame
         * for further decoding. When there is error on decoding this frame
         * if used_for_ref is set then the frame will set errinfo flag
         * if used_for_ref is cleared then the frame will set discard flag.
         */
        rk_u32      parse_err        : 1;
        rk_u32      ref_err          : 1;
        rk_u32      used_for_ref     : 1;

        rk_u32      wait_done        : 1;
        rk_u32      reserved0        : 1;
        rk_u32      ref_info_valid   : 1;
        rk_u32      ref_miss         : 16;
        rk_u32      ref_used         : 16;
    };
} ParserOutStatus;

/* parser output data header */
struct ParserOut_t {
    ParserOutStatus status;
    rk_s32          index;
    rk_u32          id;
    rk_s32          need_update;

    /* syntax is for protocol hal usage */
    void            *syntax;
    /* current slice / frame data info */
    KmppPacket      pkt;
    KmppFrame       frm;

    /* reference frame slots */
    KmppFrame       ref[MAX_DEC_REF_NUM];

    /* slice data update notify */
    void            *updates;

    /* private data after attached hal */
    void            *priv;
};

/*
 * Vdec parser work flow
 * init
 *
 * input side:
 * get_job
 * setup job with valid data length or input packet info
 * put_job
 *
 * hal side:
 * get_out (get valid output task with syntax for hal usage)
 * fill output packet / frame resources
 * send to hal to decode
 * wait hal hardware ready
 * put_out (notify parser hardware is done the frame can be output)
 *
 * output side:
 * get_frm (get valid output frame for decode output)
 *
 * deinit
 */

rk_s32 parser_query(ParserCfg *cfg);
rk_s32 parser_init(VdecParser parser, ParserCfg *cfg);
rk_s32 parser_deinit(VdecParser parser);

rk_s32 parser_reset(VdecParser parser);
rk_s32 parser_start(VdecParser parser);
rk_s32 parser_stop(VdecParser parser);

/* request valid job slot from parser with valid data length size. zero for external buffer */
rk_s32 parser_get_job(VdecParser parser, ParserJob **job, rk_s32 size);
/* send job to parser when packet length is zero the packet is finished */
rk_s32 parser_put_job(VdecParser parser, ParserJob *job);

/* run parser process and try get valid parser output task also slice update will get a frame task multiple times */
rk_s32 parser_get_out(VdecParser parser, ParserOut **task);
/*
 * poll parser out task to check it has update or not
 * return -1 for not update
 */
rk_s32 parser_poll_out(VdecParser parser, ParserOut *task);
/* run parser callback on task finish hardware processing */
rk_s32 parser_put_out(VdecParser parser, ParserOut *task);

rk_s32 parser_get_frm(VdecParser parser, KmppFrame *frm);

#endif /* __VDEC_PARSER_H__ */
