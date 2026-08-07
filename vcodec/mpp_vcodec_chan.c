/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define MODULE_TAG "mpp_vcodec_chan"

#include "kmpp_uaccess.h"

#include "mpp_vcodec_chan.h"
#include "mpp_vcodec_base.h"
#include "mpp_vcodec_debug.h"
#include "mpp_vcodec_flow.h"
#include "kmpp_log.h"
#include "mpp_enc.h"
#include "mpp_2str.h"
#include "mpp_vcodec_thread.h"
#include "rk_venc_cfg.h"
#include "kmpp_packet_impl.h"
#include "kmpp_time.h"
#include "mpp_enc_cfg.h"
#include "mpp_mem.h"
#include "mpp_buffer_impl.h"
#include "kmpp_buffer_impl.h"
#include "kmpp_shm.h"
#include "rk-mpp-kobj.h"
#include "kmpp_frame.h"
#include "kmpp_frame_impl.h"
#include "kmpp_meta.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"
#include "kmpp_atomic.h"

static rk_s32 check_chan_idle(const void *param)
{
    return !kmpp_atomic_read((KmppAtomic *)param);
}

static rk_s32 check_chan_stream(const void *param)
{
    return kmpp_atomic_read((KmppAtomic *)param);
}

int mpp_vcodec_schedule(void)
{
    return 0;
}

int mpp_vcodec_chan_create(KmppVencInitCfg init_cfg)
{
    KmppVencInitCfgImpl *cfg_impl = (KmppVencInitCfgImpl *)kmpp_obj_to_entry(init_cfg);
    int chan_id = cfg_impl->chan_id;
    MppCtxType type = cfg_impl->type;
    MppCodingType coding = cfg_impl->coding;
    RK_S32  online = cfg_impl->online;
    struct mpp_chan *chan_entry;
    MPP_RET ret = MPP_NOK;
    RK_U32 init_done = 1;
    RK_U32 num_chan;

    if (cfg_impl->chan_dup) {
        chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
        if (!chan_entry || chan_entry->state == CHAN_STATE_NULL) {
            kmpp_loge_f("chan %d not create, can not dup\n", chan_id);
            return -1;
        }

        return 0;
    }

    chan_id = mpp_vcodec_get_free_chan(type, chan_id);
    if (chan_id < 0) {
        kmpp_loge_f("get free chan fail type %d id %d\n", type, chan_id);
        return -1;
    }

    cfg_impl->chan_id = chan_id;
    chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    num_chan = mpp_vcodec_get_chan_num(type);

    switch (type) {
    case MPP_CTX_DEC: {

    } break;
    case MPP_CTX_ENC: {
        MppEnc enc = NULL;

        cfg_impl->shared_buf = &chan_entry->shared_buf;
        ret = mpp_enc_init(&enc, cfg_impl);
        if (ret)
            break;
        mpp_enc_register_chl(enc, (void *)mpp_vcodec_enc_int_handle, chan_id);
        mpp_vcodec_chan_entry_init(chan_entry, type, coding, (void *)enc);
#if 0//def CHAN_BUF_SHARED
        if (mpp_vcodec_chan_setup_hal_bufs(chan_entry, attr)) {
            mpp_enc_deinit(chan_entry->handle);
            mpp_vcodec_chan_entry_deinit(chan_entry);
            return -1;
        }
#endif
        chan_entry->cfg.online = online;
        chan_entry->last_yuv_time = kmpp_time_us();
        chan_entry->last_jeg_combo_start = kmpp_time_us();
        chan_entry->last_jeg_combo_end = kmpp_time_us();
        chan_entry->input_timeout = cfg_impl->input_timeout;
        init_done = 1;

        kmpp_logi_f("create chan %d total %d %s handle %p online %d\n",
                    chan_id, num_chan, strof_coding_type(coding), chan_entry->handle, online);
    } break;
    default: {
        kmpp_loge_f("create chan error type %d\n", type);
    } break;
    }

    if (!init_done) {
        mpp_vcodec_chan_entry_deinit(chan_entry);
        kmpp_loge_f("create chan %d fail\n", chan_id);
    }

    return 0;
}

int mpp_vcodec_chan_unbind(struct mpp_chan* chan)
{
    RK_S32 chan_id = chan->binder_chan_id != -1 ? chan->binder_chan_id : chan->master_chan_id;
    struct mpp_chan *bind_chan;

    if (chan_id == -1)
        return 0;

    bind_chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    kmpp_assert(bind_chan);
    if (chan->binder_chan_id != -1) {
        /* bind chan is slave */
        kmpp_atomic_set(chan->cfg.comb_runing, 0);
        kmpp_atomic_set(bind_chan->runing, 0);
        mpp_enc_deinit_frame(bind_chan->handle);
        chan->binder_chan_id = -1;
        bind_chan->master_chan_id = -1;
    } else {
        /* bind_chan is master */
        kmpp_atomic_set(bind_chan->cfg.comb_runing, 0);
        kmpp_atomic_set(chan->runing, 0);
        chan->master_chan_id = -1;
        bind_chan->binder_chan_id = -1;
    }

    return 0;
}

#define VCODEC_WAIT_TIMEOUT_DELAY       (2200)

int mpp_vcodec_chan_destory(int chan_id, MppCtxType type)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    int ret;

    if (!chan_entry || !chan_entry->handle)
        return 0;

    switch (type) {
    case MPP_CTX_DEC: {
    } break;
    case MPP_CTX_ENC: {
        bool wait = true;
        KmppFrame frame = NULL;

        kmpp_mutex_lock(chan_entry->chan_mutex);
        kmpp_logi_f("destroy chan %d hnd %p online %d combo %d mst %d\n",
                    chan_id, chan_entry->handle, chan_entry->cfg.online,
                    chan_entry->binder_chan_id, chan_entry->master_chan_id);

        ret = mpp_vcodec_chan_stop(chan_id, type);
        if (chan_entry->cfg.online) {
            struct mpp_chan *chan_tmp = chan_entry;

            if (chan_entry->master_chan_id != -1) {
                chan_tmp = mpp_vcodec_get_chan_entry(chan_entry->master_chan_id, type);
                mpp_enc_unbind_jpeg_task(chan_tmp->handle);
            }
            if (!mpp_enc_check_hw_running(chan_tmp->handle) &&
                !mpp_enc_check_is_int_process(chan_tmp->handle))
                wait = false;
        }

        if (wait)
            ret = kmpp_wait_timeout_uninterruptible(chan_entry->stop_wait, check_chan_idle,
                                                    chan_entry->runing, VCODEC_WAIT_TIMEOUT_DELAY);

        while (!kmpp_list_empty(&chan_entry->frm_list)) {
            KmppVencNtfy ntfy = mpp_enc_get_notify(chan_entry->handle);
            KmppVencNtfyImpl* ntfy_impl = ntfy ? kmpp_obj_to_entry(ntfy) : NULL;
            KmppShmPtr sptr;
            KmppFrame comb_frame = NULL;
            kmpp_list_head *next = chan_entry->frm_list.next;

            frame = kmpp_obj_from_list_head(next);
            kmpp_list_del_init(next);

            if (!kmpp_frame_get_meta(frame, &sptr)) {
                KmppMeta meta = kmpp_obj_from_shmptr(&sptr);
                kmpp_meta_get_obj_d(meta, KEY_COMBO_FRAME, &comb_frame, NULL);
                if (comb_frame) {
                    if (ntfy_impl) {
                        RK_S32 combo_id = -1;

                        if (!kmpp_frame_get_meta(comb_frame, &sptr)) {
                            meta = kmpp_obj_from_shmptr(&sptr);
                            kmpp_meta_get_s32(meta, KEY_CHANNEL_ID, &combo_id);
                        }
                        ntfy_impl->chan_id = combo_id;
                        ntfy_impl->frame = comb_frame;

                        ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                        ntfy_impl->drop_type = KMPP_VENC_DROP_ENC_FAILED;
                        kmpp_venc_notify(ntfy);
                    }
                }
            }

            if (ntfy_impl) {
                ntfy_impl->chan_id = chan_id;
                ntfy_impl->frame = frame;

                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                ntfy_impl->drop_type = KMPP_VENC_DROP_ENC_FAILED;
                kmpp_venc_notify(ntfy);
            }
        }
        if (chan_entry->cfg.online)
            mpp_vcodec_chan_unbind(chan_entry);
        mpp_vcodec_stream_clear(chan_entry);
        mpp_enc_deinit(chan_entry->handle);
        mpp_vcodec_chan_entry_deinit(chan_entry);
        kmpp_mutex_unlock(chan_entry->chan_mutex);
        kmpp_logi_f("destroy chan %d done\n", chan_id);
    } break;
    default: {
        kmpp_loge_f("create chan error type %d\n", type);
    } break;
    }

    return 0;
}

int mpp_vcodec_chan_start(int chan_id, MppCtxType type)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);

    kmpp_assert(chan_entry->handle != NULL);
    if (!chan_entry->handle)
        return MPP_NOK;

    switch (type) {
    case MPP_CTX_DEC: {

    } break;
    case MPP_CTX_ENC: {
        unsigned long lock_flag;

        mpp_enc_start(chan_entry->handle);
        kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
        chan_entry->state = CHAN_STATE_RUN;
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        enc_chan_update_chan_prior_tab();
    } break;
    default: {
        kmpp_loge_f("create chan error type %d\n", type);
    } break;
    }

    return 0;
}

int mpp_vcodec_chan_stop(int chan_id, MppCtxType type)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    int ret = 0;

    kmpp_assert(chan_entry->handle != NULL);
    switch (type) {
    case MPP_CTX_DEC: {

    } break;
    case MPP_CTX_ENC: {
        unsigned long lock_flag;

        ret = mpp_enc_stop(chan_entry->handle);
        kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
        if (chan_entry->state != CHAN_STATE_RUN) {
            kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
            return 0;
        }
        chan_entry->state = CHAN_STATE_SUSPEND;
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        kmpp_wait_wakeup(chan_entry->wait);
        enc_chan_update_chan_prior_tab();
    } break;
    default: {
        kmpp_loge_f("create chan error type %d\n", type);
    } break;
    }

    return ret;
}

int mpp_vcodec_chan_pause(int chan_id, MppCtxType type)
{
    return 0;
}

int mpp_vcodec_chan_resume(int chan_id, MppCtxType type)
{
    return 0;
}

static void mpp_vcodec_packet_buffer_share(KmppPacket packet)
{
    KmppPacketImpl *pkt_impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    KmppShmPtr pos_sptr;
    KmppShmPtr data_sptr;

    pos_sptr = pkt_impl->pos;
    data_sptr = pkt_impl->data;

    if (pkt_impl->buf.buf) {
        data_sptr.uaddr = mpp_buffer_get_uptr(pkt_impl->buf.buf);
        if (data_sptr.uaddr)
            pos_sptr.uaddr = data_sptr.uaddr + pkt_impl->buf.start_offset;
        else
            pos_sptr.uaddr = 0;

        kmpp_packet_set_data(packet, &data_sptr);
        kmpp_packet_set_pos(packet, &pos_sptr);
    }
}

static int mpp_vcodec_chan_get_packet(int chan_id, MppCtxType type, KmppPacket *packet)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    RK_U32 count = kmpp_atomic_read(chan_entry->stream_count);
    KmppPacketImpl *pkt_impl = NULL;
    rk_ul flags;

    if (!count) {
        /* empty queue is the normal end of a non-blocking drain loop */
        mpp_vcodec_detail("no stream count found in list\n");
        return MPP_NOK;
    }

    kmpp_spin_lock_irqsave(chan_entry->pkt_lock, &flags);
    *packet = NULL;
    if (!kmpp_list_empty(&chan_entry->pkt_list)) {
        kmpp_list_head *next = chan_entry->pkt_list.next;

        *packet = kmpp_obj_from_list_head(next);
        kmpp_list_del_init(next);
    }
    kmpp_spin_unlock_irqrestore(chan_entry->pkt_lock, &flags);
    if (!(*packet)) {
        mpp_vcodec_detail("no packet in packet list\n");
        return MPP_NOK;
    }

    pkt_impl = (KmppPacketImpl *)kmpp_obj_to_entry(*packet);
    /* flush cache before get packet*/
    if (pkt_impl->buf.buf)
        ring_buf_flush(&pkt_impl->buf, 1);
    kmpp_atomic_dec(chan_entry->stream_count);
    chan_entry->seq_user_get = pkt_impl->dts;
    kmpp_atomic_inc(chan_entry->pkt_user_get);

    return MPP_OK;
}

int mpp_vcodec_chan_get_stream_legacy(int chan_id, MppCtxType type, struct venc_packet *enc_packet)
{
    KmppPacket packet = NULL;
    KmppPacketImpl *pkt_impl = NULL;
    int ret;

    ret = mpp_vcodec_chan_get_packet(chan_id, type, &packet);
    if (ret) {
        kmpp_memset(enc_packet, 0, sizeof(struct venc_packet));
        return ret;
    }

    kmpp_packet_get_flag(packet, &enc_packet->flag);
    kmpp_packet_get_length(packet, &enc_packet->len);
    kmpp_packet_get_temporal_id(packet, &enc_packet->temporal_id);
    kmpp_packet_get_pts(packet, &enc_packet->u64pts);
    kmpp_packet_get_dts(packet, &enc_packet->u64dts);
    enc_packet->data_num = 1;
    enc_packet->u64packet_addr = (uintptr_t )packet;

    pkt_impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    if (pkt_impl->buf.buf) {
        enc_packet->u64priv_data = mpp_buffer_get_uptr(pkt_impl->buf.buf);
        enc_packet->offset = pkt_impl->buf.start_offset;
        enc_packet->buf_size = mpp_buffer_get_size(pkt_impl->buf.buf);
    }

    return MPP_OK;
}

int mpp_vcodec_chan_put_stream_legacy(int chan_id, MppCtxType type, struct venc_packet *enc_packet)
{
    KmppPacket packet = (KmppPacket)(uintptr_t )enc_packet->u64packet_addr;

    return kmpp_packet_put(packet);
}

int mpp_vcodec_chan_get_stream(int chan_id, MppCtxType type, KmppShmPtr *packet_sptr)
{
    KmppPacket packet = NULL;
    RK_U32 ret;

    kmpp_memset(packet_sptr, 0, sizeof(KmppShmPtr));

    ret = mpp_vcodec_chan_get_packet(chan_id, type, &packet);
    if (ret)
        return ret;

    /* kmppPacket and MppBuffer need to bind userspace share memory */
    kmpp_obj_share_f(packet);
    kmpp_obj_to_shmptr(packet, packet_sptr);
    mpp_vcodec_packet_buffer_share(packet);

    return MPP_OK;
}

int mpp_vcodec_chan_push_frm(int chan_id, void *param)
{
    struct venc_module *venc = NULL;
    struct vcodec_threads *thd;
    struct mpp_chan *chan_entry = NULL;
    KmppObjShm *ioc = NULL;
    KmppBuffer buffer = NULL;
    KmppFrame frame = NULL;
    KmppShmPtr sptr;
    KmppShm shm;
    RK_S64 pts, dts;
    RK_U32 eos, buf_fd;
    rk_ul flags;

    chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    venc = mpp_vcodec_get_enc_module_entry();
    thd = venc->thd;

    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &flags);
    if (chan_entry->frame_cnt >= MAX_FRAME_NUM) {
        /* expected backpressure in non-blocking use, not an error */
        mpp_vcodec_detail("chan %d frame list is full\n", chan_entry->chan_id);
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &flags);
        vcodec_thread_trigger(thd);
        return MPP_NOK;
    }
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &flags);

    ioc = (KmppObjShm *)param;
    shm = (KmppShm)(uintptr_t)ioc->kobj_kaddr;
    frame = kmpp_shm_get_kpriv(shm);

    kmpp_frame_get_buf_fd(frame, &buf_fd);
    if (buf_fd) {
        MppBufferInfo buf_info;
        kmpp_memset(&buf_info, 0, sizeof(buf_info));
        buf_info.fd = buf_fd;
        mpp_buffer_import(&buffer, &buf_info);
        mpp_buffer_to_shmptr(buffer, &sptr);
        kmpp_frame_set_buffer(frame, &sptr);
        mpp_buffer_put(buffer);
    } else {
        kmpp_frame_get_buffer(frame, &sptr);
        buffer = kmpp_obj_from_shmptr(&sptr);
    }

    kmpp_frame_get_eos(frame, &eos);
    kmpp_frame_get_pts(frame, &pts);
    kmpp_frame_get_dts(frame, &dts);

    if (eos) {
        KmppPacket packet = NULL;

        kmpp_packet_get(&packet);
        kmpp_packet_set_eos(packet);
        kmpp_packet_set_pts(packet, pts);
        kmpp_packet_set_dts(packet, dts);
        mpp_vcodec_enc_add_packet_list(chan_entry, packet);
        return 0;
    }

    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &flags);
    kmpp_list_add_tail(kmpp_obj_to_list_head(frame), &chan_entry->frm_list);
    chan_entry->frame_cnt++;
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &flags);
    vcodec_thread_trigger(thd);

    if (chan_entry->input_timeout == MPP_POLL_BLOCK) {
        if (!kmpp_atomic_read(chan_entry->stream_count))
            kmpp_wait_timeout_uninterruptible(chan_entry->wait, check_chan_stream,
                                              chan_entry->stream_count, 1000);
    }

    return 0;
}

static int mpp_vcodec_chan_change_coding_type(int chan_id, void *arg)
{
    KmppVencInitCfgImpl *init_cfg = (KmppVencInitCfgImpl*)kmpp_obj_to_entry(arg);
    struct mpp_chan *entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    KmppVencStCfg cfg = NULL;
    MppEncCfgSet *cfg_impl = NULL;
    bool wait = true;
    int ret;

    ret = kmpp_venc_st_cfg_get(&cfg);
    if (ret) {
        kmpp_loge("change_coding_type malloc fail");
        return MPP_NOK;
    }
    cfg_impl = (MppEncCfgSet *)kmpp_obj_to_entry(cfg);

    mpp_enc_control(entry->handle, MPP_ENC_GET_CFG, cfg);
    kmpp_assert(entry->handle != NULL);
    kmpp_assert(chan_id == init_cfg->chan_id);

    ret = mpp_vcodec_chan_stop(chan_id, MPP_CTX_ENC);
    if (entry->cfg.online) {
        struct mpp_chan *chan_tmp = entry;

        if (entry->master_chan_id != -1) {
            chan_tmp = mpp_vcodec_get_chan_entry(entry->master_chan_id, MPP_CTX_ENC);
            mpp_enc_unbind_jpeg_task(chan_tmp->handle);
        }
        if (!mpp_enc_check_hw_running(chan_tmp->handle) &&
            !mpp_enc_check_is_int_process(chan_tmp->handle))
            wait = false;
    }

    if (wait)
        ret = kmpp_wait_timeout_uninterruptible(entry->stop_wait, check_chan_idle,
                                                entry->runing, VCODEC_WAIT_TIMEOUT_DELAY);

    if (entry->cfg.online)
        mpp_vcodec_chan_unbind(entry);

    kmpp_mutex_lock(entry->chan_mutex);
    mpp_enc_deinit(entry->handle);
    mpp_vcodec_stream_clear(entry);
    mpp_vcodec_put_chan(MPP_CTX_ENC);
    entry->handle = NULL;
    entry->state = CHAN_STATE_NULL;
    entry->reenc = 0;
    entry->binder_chan_id = -1;
    entry->master_chan_id = -1;
    kmpp_mutex_unlock(entry->chan_mutex);

    mpp_vcodec_chan_create(arg);
    entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);

    mpp_enc_control(entry->handle, MPP_ENC_SET_REF_CFG, &cfg_impl->ref_param);

    *mpp_enc_cfg_rc_change(cfg_impl) = (rk_u32)(-1);
    *mpp_enc_cfg_prep_change(cfg_impl) = (rk_u32)(-1);
    mpp_enc_control(entry->handle, MPP_ENC_SET_CFG, cfg);
    kmpp_venc_st_cfg_put(cfg);

    mpp_vcodec_chan_start(chan_id, MPP_CTX_ENC);

    return 0;
}

int mpp_vcodec_chan_control(int chan_id, MppCtxType type, int cmd, void *arg)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    kmpp_assert(chan_entry->handle != NULL);

    switch (type) {
    case MPP_CTX_DEC: {
        ;
    } break;
    case MPP_CTX_ENC: {
        if (cmd == MPP_SET_VENC_INIT_KCFG) {
            KmppVencInitCfg cfg = NULL;
            KmppObjShm *ioc = (KmppObjShm *)arg;
            KmppShm shm = (KmppShm)(uintptr_t)ioc->kobj_kaddr;
            void *kbase = kmpp_shm_get_kbase(shm);

            if (!kbase || kbase != shm) {
                kmpp_loge_f("invalid obj kbase %px shm %px\n", kbase, shm);
                return MPP_NOK;
            }

            cfg = kmpp_shm_get_kpriv(shm);
            mpp_vcodec_chan_change_coding_type(chan_id, cfg);
        } else if (cmd == MPP_ENC_SET_CFG || cmd == MPP_ENC_GET_CFG) {
            KmppObjShm *ioc = (KmppObjShm *)arg;
            KmppShm shm = (KmppShm)(uintptr_t)ioc->kobj_kaddr;
            void *kbase = kmpp_shm_get_kbase(shm);
            MppEncCfgSet *cfg = NULL;
            KmppVencStCfg cfg_obj = NULL;
            RK_U32 prep_change;

            if (!kbase || kbase != shm) {
                kmpp_loge_f("invalid obj kbase %px shm %px\n", kbase, shm);
                return MPP_NOK;
            }
            cfg = (MppEncCfgSet *)kmpp_shm_get_kaddr(shm);
            cfg_obj = kmpp_shm_get_kpriv(shm);
            prep_change = *mpp_enc_cfg_prep_change(cfg);

            mpp_enc_control(chan_entry->handle, cmd, (void *)cfg_obj);
            /*
            * In the case of wrapping, when switching resolutions,
            * need to ensure that the wrapping frame being encoded finish.
            */
            if (cmd == MPP_ENC_SET_CFG && chan_entry->cfg.online && prep_change) {
                if (mpp_enc_check_hw_running(chan_entry->handle)) {
                    kmpp_wait_timeout_uninterruptible(chan_entry->stop_wait, check_chan_idle,
                                                      chan_entry->runing,
                                                      VCODEC_WAIT_TIMEOUT_DELAY);
                }
            }
        } else {
            mpp_enc_control(chan_entry->handle, cmd, arg);
        }
    } break;
    default: {
        kmpp_loge_f("control type %d error\n", type);
    } break;
    }

    return 0;
}

#include <linux/export.h>

EXPORT_SYMBOL(mpp_vcodec_chan_create);
EXPORT_SYMBOL(mpp_vcodec_chan_destory);
EXPORT_SYMBOL(mpp_vcodec_chan_start);
EXPORT_SYMBOL(mpp_vcodec_chan_stop);
EXPORT_SYMBOL(mpp_vcodec_chan_pause);
EXPORT_SYMBOL(mpp_vcodec_chan_resume);
EXPORT_SYMBOL(mpp_vcodec_chan_control);
EXPORT_SYMBOL(mpp_vcodec_chan_push_frm);
EXPORT_SYMBOL(mpp_vcodec_chan_get_stream_legacy);
EXPORT_SYMBOL(mpp_vcodec_chan_put_stream_legacy);
EXPORT_SYMBOL(mpp_vcodec_chan_get_stream);
EXPORT_SYMBOL(mpp_vcodec_schedule);
