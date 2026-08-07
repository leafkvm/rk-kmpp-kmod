/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_MPP_VCODEC_BASE_H__
#define __ROCKCHIP_MPP_VCODEC_BASE_H__

#include "kmpp_spinlock.h"
#include "kmpp_mutex.h"
#include "kmpp_atomic.h"
#include "legacy_rockit.h"
#include "kmpp_packet.h"
#include "kmpp_wait.h"
#include "kmpp_list.h"
#include "kmpp_venc_objs.h"

#define MAX_ENC_NUM 32
#define MAX_FRAME_NUM 2

typedef enum {
    CHAN_STATE_NULL = 0,
    CHAN_STATE_INIT,
    CHAN_STATE_SUSPEND_PENDING,
    CHAN_STATE_SUSPEND,
    CHAN_STATE_RUN,
    CHAN_STATE_CANCEL,
    CHAN_STATE_MAX
} chan_state;

struct chan_cfg {
    RK_U8 priority;
    RK_U8 online;
    KmppAtomic *comb_runing;
};

struct mpp_chan {
    RK_S32 chan_id;
    RK_S32 binder_chan_id;
    RK_S32 master_chan_id;
    RK_S32 coding_type;
    MppCtxType type;
    struct chan_cfg cfg;
    chan_state state;
    RK_U32 chan_pri;
    KmppAtomic *runing;
    RK_U32 reenc;
    KmppSpinlock *chan_lock;
    /* protect debug info when destory channel */
    KmppMutex *chan_mutex;
    void *handle;
    KmppWait *wait;
    KmppWait *stop_wait;
    /* debug info */
    KmppAtomic *stream_count;
    KmppAtomic *pkt_total_num;
    KmppAtomic *pkt_user_get;
    RK_U64 seq_user_get;
    RK_U64 seq_encoded;
    RK_U64 seq_encoding;
    RK_U32 last_cfg_time;
    RK_S64 last_yuv_time;
    RK_S64 last_jeg_combo_start;
    RK_S64 last_jeg_combo_end;
    RK_S32 gap_time;
    RK_S32 combo_gap_time;
    RK_U32 max_width;
    RK_U32 max_height;
    RK_U32 ring_buf_size;
    struct hal_shared_buf shared_buf;
    RK_U32 shared_buf_release;
    RK_U32 max_lt_cnt;
    RK_S64 pts_diff;
    KmppFrame pskip_frame;
    RK_U32 frame_cnt;
    RK_S32 input_timeout;
    RK_S32 output_timeout;
    kmpp_list_head frm_list;
    KmppSpinlock *pkt_lock;
    kmpp_list_head pkt_list;
};

struct venc_module {
    void *check;
    const char *name;
    struct mpp_chan mpp_enc_chan_entry[MAX_ENC_NUM];
    RK_U32 num_enc;
    KmppSpinlock *enc_lock;
    RK_U32 started_chan_num;
    RK_U32 chan_pri_tab_index;
    RK_U32 curr_high_prior_chan;
    RK_S32 chan_pri_tab[MAX_ENC_NUM][2];    /* chan_id and prio */
    struct vcodec_threads *thd;
};

struct vcodec_entry {
    struct venc_module venc;
};

RK_U32 mpp_vcodec_get_chan_num(MppCtxType type);
struct venc_module *mpp_vcodec_get_enc_module_entry(void);
struct mpp_chan *mpp_vcodec_get_chan_entry(RK_S32 chan_id, MppCtxType type);
void enc_chan_update_chan_prior_tab(void);
MPP_RET enc_chan_update_tab_after_enc(RK_U32 curr_chan);
void enc_chan_get_high_prior_chan(void);
int mpp_vcodec_init(void);
int mpp_vcodec_deinit(void);
int mpp_vcodec_chan_entry_init(struct mpp_chan *entry, MppCtxType type,
                               MppCodingType coding, void *handle);
int mpp_vcodec_chan_entry_deinit(struct mpp_chan *entry);
void mpp_vcodec_stream_clear(struct mpp_chan *entry);
RK_S32 mpp_vcodec_get_free_chan(MppCtxType type, RK_S32 chan_id);
void mpp_vcodec_put_chan(MppCtxType type);
void enc_test(void);
MPP_RET mpp_vcodec_chan_setup_hal_bufs(struct mpp_chan *entry, KmppVencInitCfg cfg);

#endif
