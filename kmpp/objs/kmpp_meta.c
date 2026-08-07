/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_meta"

#include <linux/kernel.h>

#include "kmpp_atomic.h"
#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_spinlock.h"
#include "kmpp_string.h"

#include "kmpp_shm.h"
#include "kmpp_obj.h"
#include "kmpp_mod_id.h"
#include "kmpp_meta_impl.h"

#define META_DBG_FLOW               (0x00000001)

#define meta_dbg(flag, fmt, ...)    kmpp_dbg(kmpp_meta_debug, flag, fmt, ## __VA_ARGS__)
#define meta_dbg_f(flag, fmt, ...)  kmpp_dbg_f(kmpp_meta_debug, flag, fmt, ## __VA_ARGS__)

#define meta_dbg_flow(fmt, ...)     meta_dbg_f(META_DBG_FLOW, fmt, ## __VA_ARGS__)

#define META_ON_OPS         (0x00010000)
#define META_VAL_INVALID    (0x00000000)
#define META_VAL_VALID      (0x00000001)
#define META_VAL_READY      (0x00000002)
#define META_READY_MASK     (META_VAL_VALID | META_VAL_READY)
/* property mask */
#define META_VAL_IS_OBJ     (0x00000010)
#define META_VAL_IS_SHM     (0x00000020)
#define META_PROP_MASK      (META_VAL_IS_OBJ | META_VAL_IS_SHM)
#define META_UNMASK_PROP(x) KMPP_FETCH_AND(x, (~META_PROP_MASK))

typedef enum KmppMetaDataType_e {
    /* kmpp meta data of normal data type */
    TYPE_VAL_32         = '3',
    TYPE_VAL_64         = '6',
    TYPE_KPTR           = 'k',  /* kernel pointer */
    TYPE_UPTR           = 'u',  /* userspace pointer */
    TYPE_SPTR           = 's',  /* share memory pointer */
} KmppMetaType;

typedef struct KmppMetaSrv_s {
    KmppSpinlock        *lock;
    kmpp_list_head      list;
    KmppObjDef          def;

    rk_u32              meta_id;
    rk_s32              meta_count;
    rk_u32              finished;
} KmppMetaSrv;

#define META_KEY_TO_U64(key, type)      ((rk_u64)((rk_u32)cpu_to_be32(key)) | ((rk_u64)type << 32))

static KmppMetaSrv *kmpp_meta_srv = NULL;
static rk_u32 kmpp_meta_debug = 0;

static rk_s32 kmpp_meta_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppMetaSrv *srv = kmpp_meta_srv;
    KmppMetaPriv *priv = (KmppMetaPriv *)kmpp_obj_to_priv(obj);
    KmppMetaImpl *impl = (KmppMetaImpl*)entry;

    if (!impl || !srv) {
        kmpp_loge_f("invalid entry %px srv %px\n", entry, srv);
        return rk_nok;
    }

    impl->caller = caller;
    impl->priv = priv;
    impl->node_count = 0;

    priv->meta_id = KMPP_FETCH_ADD(&srv->meta_id, 1);
    priv->ref_count = 1;
    priv->obj = obj;
    KMPP_INIT_LIST_HEAD(&priv->list);

    kmpp_spin_lock(srv->lock);
    kmpp_list_add_tail(&priv->list, &srv->list);
    kmpp_spin_unlock(srv->lock);
    KMPP_FETCH_ADD(&srv->meta_count, 1);

    meta_dbg_flow("meta %d obj %px entry %px at %s\n",
                  priv->meta_id, obj, entry, caller);

    return rk_ok;
}

static rk_s32 kmpp_meta_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppMetaSrv *srv = kmpp_meta_srv;
    KmppMetaImpl *impl = (KmppMetaImpl*)entry;
    KmppMetaPriv *priv = impl->priv;
    rk_s32 ref_count;
    rk_s32 old;
    (void)obj;

    if (srv->finished)
        return rk_nok;

    old = priv->ref_count;
    ref_count = KMPP_SUB_FETCH(&priv->ref_count, 1);

    meta_dbg_flow("meta %d ref_cnt %d -> %d at %s\n",
                  priv->meta_id, old, ref_count, caller);

    if (ref_count > 0)
        return rk_nok;

    if (ref_count < 0) {
        kmpp_loge_f("invalid negative ref_count %d at %s\n",
                    ref_count, caller);
        return rk_nok;
    }

    kmpp_spin_lock(srv->lock);
    kmpp_list_del_init(&priv->list);
    kmpp_spin_unlock(srv->lock);
    KMPP_FETCH_SUB(&srv->meta_count, 1);

    return rk_ok;
}

static rk_s32 kmpp_meta_impl_dump(void *entry)
{
    KmppMetaImpl *impl = (KmppMetaImpl*)entry;
    KmppMetaVals *vals;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    vals = &impl->vals;

    kmpp_logi("input frame      %px\n", vals->in_frm.val_shm.kptr);
    kmpp_logi("input packet     %px\n", vals->in_pkt.val_shm.kptr);
    kmpp_logi("output frame     %px\n", vals->out_frm.val_shm.kptr);
    kmpp_logi("output packet    %px\n", vals->out_pkt.val_shm.kptr);

    return rk_ok;
}

#define META_LOCTBL_ADD(key, ktype, field) \
    do { \
        KmppEntry entry = { .val = 0, }; \
        rk_u64 name = META_KEY_TO_U64(key, ktype); \
        entry.tbl.elem_offset = offsetof(KmppMetaImpl, field); \
        entry.tbl.elem_size = sizeof(((KmppMetaImpl *)(0))->field); \
        entry.tbl.elem_type = ELEM_TYPE_st; \
        kmpp_objdef_add_entry(def, (const rk_u8 *)&name, &entry); \
    } while (0);

#define META_ENTRY_TABLE(ENTRY) \
    ENTRY(KEY_INPUT_FRAME,          TYPE_SPTR,      vals.in_frm) \
    ENTRY(KEY_INPUT_PACKET,         TYPE_SPTR,      vals.in_pkt) \
    ENTRY(KEY_OUTPUT_FRAME,         TYPE_SPTR,      vals.out_frm) \
    ENTRY(KEY_OUTPUT_PACKET,        TYPE_SPTR,      vals.out_pkt) \
    ENTRY(KEY_MOTION_INFO,          TYPE_SPTR,      vals.md_buf) \
    ENTRY(KEY_HDR_INFO,             TYPE_SPTR,      vals.hdr_buf) \
    ENTRY(KEY_HDR_META_OFFSET,      TYPE_VAL_32,    vals.hdr_meta_offset) \
    ENTRY(KEY_HDR_META_SIZE,        TYPE_VAL_32,    vals.hdr_meta_size) \
    ENTRY(KEY_INPUT_BLOCK,          TYPE_VAL_32,    vals.in_block) \
    ENTRY(KEY_OUTPUT_BLOCK,         TYPE_VAL_32,    vals.out_block) \
    ENTRY(KEY_INPUT_IDR_REQ,        TYPE_VAL_32,    vals.in_idr_req) \
    ENTRY(KEY_OUTPUT_INTRA,         TYPE_VAL_32,    vals.out_intra) \
    ENTRY(KEY_TEMPORAL_ID,          TYPE_VAL_32,    vals.temporal_id) \
    ENTRY(KEY_LONG_REF_IDX,         TYPE_VAL_32,    vals.lt_ref_idx) \
    ENTRY(KEY_ENC_AVERAGE_QP,       TYPE_VAL_32,    vals.enc_avg_qp) \
    ENTRY(KEY_ENC_START_QP,         TYPE_VAL_32,    vals.enc_start_qp) \
    ENTRY(KEY_ENC_BPS_RT,           TYPE_VAL_32,    vals.enc_bps_rt) \
    ENTRY(KEY_ROI_DATA,             TYPE_KPTR,      vals.enc_roi) \
    ENTRY(KEY_ROI_DATA2,            TYPE_SPTR,      vals.enc_roi2) \
    ENTRY(KEY_JPEG_ROI_DATA,        TYPE_KPTR,      vals.jpeg_roi) \
    ENTRY(KEY_OSD_DATA,             TYPE_SPTR,      vals.enc_osd) \
    ENTRY(KEY_OSD_DATA2,            TYPE_SPTR,      vals.enc_osd2) \
    ENTRY(KEY_OSD_DATA3,            TYPE_KPTR,      vals.enc_osd3) \
    ENTRY(KEY_USER_DATA,            TYPE_SPTR,      vals.usr_data) \
    ENTRY(KEY_USER_DATAS,           TYPE_SPTR,      vals.usr_datas) \
    ENTRY(KEY_LVL64_INTER_NUM,      TYPE_VAL_32,    vals.enc_inter64_num) \
    ENTRY(KEY_LVL32_INTER_NUM,      TYPE_VAL_32,    vals.enc_inter32_num) \
    ENTRY(KEY_LVL16_INTER_NUM,      TYPE_VAL_32,    vals.enc_inter16_num) \
    ENTRY(KEY_LVL8_INTER_NUM,       TYPE_VAL_32,    vals.enc_inter8_num) \
    ENTRY(KEY_LVL32_INTRA_NUM,      TYPE_VAL_32,    vals.enc_intra32_num) \
    ENTRY(KEY_LVL16_INTRA_NUM,      TYPE_VAL_32,    vals.enc_intra16_num) \
    ENTRY(KEY_LVL8_INTRA_NUM,       TYPE_VAL_32,    vals.enc_intra8_num) \
    ENTRY(KEY_LVL4_INTRA_NUM,       TYPE_VAL_32,    vals.enc_intra4_num) \
    ENTRY(KEY_OUTPUT_PSKIP,         TYPE_VAL_32,    vals.enc_out_pskip) \
    ENTRY(KEY_INPUT_PSKIP,          TYPE_VAL_32,    vals.enc_in_skip) \
    ENTRY(KEY_INPUT_PSKIP_NUM,      TYPE_VAL_32,    vals.enc_in_skip_num) \
    ENTRY(KEY_ENC_SSE,              TYPE_VAL_32,    vals.enc_sse) \
    ENTRY(KEY_QPMAP0,               TYPE_SPTR,      vals.enc_qpmap0) \
    ENTRY(KEY_NPU_SOBJ_FLAG,        TYPE_SPTR,      vals.npu_sobj) \
    ENTRY(KEY_NPU_UOBJ_FLAG,        TYPE_UPTR,      vals.npu_uobj) \
    ENTRY(KEY_NPU_OBJ_FG_AREA,      TYPE_VAL_32,    vals.npu_fg_area) \
    ENTRY(KEY_ENC_MARK_LTR,         TYPE_VAL_32,    vals.enc_mark_ltr) \
    ENTRY(KEY_ENC_USE_LTR,          TYPE_VAL_32,    vals.enc_use_ltr) \
    ENTRY(KEY_ENC_FRAME_QP,         TYPE_VAL_32,    vals.enc_frm_qp) \
    ENTRY(KEY_ENC_BASE_LAYER_PID,   TYPE_VAL_32,    vals.enc_base_layer_pid) \
    ENTRY(KEY_DEC_TBN_EN,           TYPE_VAL_32,    vals.dec_thumb_en) \
    ENTRY(KEY_DEC_TBN_Y_OFFSET,     TYPE_VAL_32,    vals.dec_thumb_y_offset) \
    ENTRY(KEY_DEC_TBN_UV_OFFSET,    TYPE_VAL_32,    vals.dec_thumb_uv_offset) \
    ENTRY(KEY_COMBO_FRAME,          TYPE_SPTR,      vals.combo_frame) \
    ENTRY(KEY_CHANNEL_ID,           TYPE_VAL_32,    vals.chan_id) \
    ENTRY(KEY_PP_MD_BUF,            TYPE_SPTR,      vals.pp_md_buf) \
    ENTRY(KEY_PP_OD_BUF,            TYPE_SPTR,      vals.pp_od_buf) \
    ENTRY(KEY_PP_OUT,               TYPE_SPTR,      vals.pp_out) \
    ENTRY(KEY_EXP_TIME,             TYPE_VAL_32,    vals.ae_exp_time) \
    ENTRY(KEY_ANALOG_GAIN,          TYPE_VAL_32,    vals.ae_analog_gain) \
    ENTRY(KEY_DIGITAL_GAIN,         TYPE_VAL_32,    vals.ae_digital_gain) \
    ENTRY(KEY_ISP_DGAIN,            TYPE_VAL_32,    vals.ae_isp_dgain)

static rk_s32 kmpp_meta_init(void)
{
    if (!kmpp_meta_srv) {
        rk_s32 lock_size = kmpp_spinlock_size();
        KmppMetaSrv *srv = kmpp_calloc(sizeof(KmppMetaSrv) + lock_size);

        if (srv) {
            KmppObjDef def = NULL;

            kmpp_spinlock_assign(&srv->lock, (void *)(srv + 1), lock_size);
            KMPP_INIT_LIST_HEAD(&srv->list);

            kmpp_objdef_get(&def, sizeof(KmppMetaPriv), sizeof(KmppMetaImpl), "KmppMeta");

            /* add meta size */
            do {
                KmppEntry entry = { .val = 0, };

                entry.tbl.elem_offset = (uintptr_t)(&(((KmppMetaImpl *)(0))->node_count));
                entry.tbl.elem_size = sizeof(((KmppMetaImpl *)(0))->node_count);
                entry.tbl.elem_type = ELEM_TYPE_s32;
                kmpp_objdef_add_entry(def, "size", &entry);
            } while (0);

            META_ENTRY_TABLE(META_LOCTBL_ADD)

            kmpp_objdef_add_entry(def, NULL, NULL);
            kmpp_objdef_add_init(def, kmpp_meta_impl_init);
            kmpp_objdef_add_deinit(def, kmpp_meta_impl_deinit);
            kmpp_objdef_add_dump(def, kmpp_meta_impl_dump);
            kmpp_objdef_share(def);
            kmpp_objdef_set_prop(def, "disable_mismatch_log", "true");
            kmpp_objdef_set_prop(def, "late_deinit", "true");

            srv->def = def;
            kmpp_meta_srv = srv;
        }
    }

    return rk_ok;
}

static rk_s32 kmpp_meta_deinit(void)
{
    KmppMetaSrv *srv = kmpp_meta_srv;
    KmppMetaPriv *meta, *n;
    KMPP_LIST_HEAD(list);

    kmpp_spin_lock(srv->lock);
    kmpp_list_for_each_entry_safe(meta, n, &srv->list, KmppMetaPriv, list) {
        kmpp_list_move_tail(&meta->list, &list);
    }
    kmpp_spin_unlock(srv->lock);

    kmpp_list_for_each_entry_safe(meta, n, &list, KmppMetaPriv, list) {
        kmpp_list_del_init(&meta->list);
        kmpp_meta_put_f(meta->obj);
    }

    kmpp_spinlock_deinit(&srv->lock);

    if (srv->def) {
        kmpp_objdef_put(srv->def);
        srv->def = NULL;
    }

    kmpp_free(srv);
    kmpp_meta_srv = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_META, meta, kmpp_meta_init, kmpp_meta_deinit);

static void *meta_key_to_addr(KmppMetaImpl *impl, KmppMetaKey key, KmppMetaType type)
{
    if (impl) {
        KmppMetaSrv *srv = kmpp_meta_srv;
        rk_u64 val = META_KEY_TO_U64(key, type);
        KmppEntry *tbl = NULL;

        kmpp_objdef_get_entry(srv->def, (const rk_u8 *)&val, &tbl);
        if (tbl)
            return ((rk_u8 *)impl) + tbl->tbl.elem_offset;
    }

    return NULL;
}

rk_s32 kmpp_meta_get(KmppMeta *meta, const rk_u8 *caller)
{
    KmppMetaSrv *srv = kmpp_meta_srv;

    if (!meta || !srv) {
        kmpp_loge_f("found invalid input %px or srv %px\n", meta, srv);
        return rk_nok;
    }

    return kmpp_obj_get(meta, srv->def, caller);
}

rk_s32 kmpp_meta_get_share(KmppMeta *meta, KmppFsDev *file, const rk_u8 *caller)
{
    KmppMetaSrv *srv = kmpp_meta_srv;

    if (!meta || !srv || !file) {
        kmpp_loge_f("found invalid input %px or srv %px file %px\n",
                    meta, srv, file);
        return rk_nok;
    }

    return kmpp_obj_get_share(meta, srv->def, file, caller);
}

rk_s32 kmpp_meta_put(KmppMeta meta, const rk_u8 *caller)
{
    return kmpp_obj_put(meta, caller);
}

rk_s32 kmpp_meta_inc_ref(KmppMeta meta)
{
    KmppMetaPriv *priv = kmpp_obj_to_priv(meta);
    rk_s32 old;

    if (!priv) {
        kmpp_loge_f("found NULL input\n");
        return rk_nok;
    }

    old = KMPP_FETCH_ADD(&priv->ref_count, 1);

    meta_dbg_flow("meta %d inc ref %d -> %d\n",
                  priv->meta_id, old, priv->ref_count);

    return rk_ok;
}

rk_s32 kmpp_meta_size(KmppMeta meta)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);

    if (!meta) {
        kmpp_loge_f("found NULL input\n");
        return -1;
    }

    return KMPP_FETCH_ADD(&impl->node_count, 0);
}

rk_s32 kmpp_meta_dump(KmppMeta meta)
{
    return kmpp_obj_dump_f(meta);
}

#define KMPP_META_ACCESSOR(func_type, arg_type, key_type, key_field)  \
    rk_s32 kmpp_meta_set_##func_type(KmppMeta meta, KmppMetaKey key, arg_type val) \
    { \
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta); \
        KmppMetaVal *meta_val = meta_key_to_addr(impl, key, key_type); \
        if (!meta_val) \
            return rk_nok; \
        if (KMPP_BOOL_CAS(&meta_val->state, META_VAL_INVALID, META_VAL_VALID)) \
            KMPP_FETCH_ADD(&impl->node_count, 1); \
        meta_val->key_field = val; \
        KMPP_FETCH_OR(&meta_val->state, META_VAL_READY); \
        return rk_ok; \
    } \
    EXPORT_SYMBOL(kmpp_meta_set_##func_type); \
    rk_s32 kmpp_meta_get_##func_type(KmppMeta meta, KmppMetaKey key, arg_type *val) \
    { \
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta); \
        KmppMetaVal *meta_val = meta_key_to_addr(impl, key, key_type); \
        if (!meta_val) \
            return rk_nok; \
        if (KMPP_BOOL_CAS(&meta_val->state, META_READY_MASK, META_VAL_INVALID)) { \
            if (val) *val = meta_val->key_field; \
            KMPP_FETCH_SUB(&impl->node_count, 1); \
            return rk_ok; \
        } \
        return rk_nok; \
    } \
    EXPORT_SYMBOL(kmpp_meta_get_##func_type); \
    rk_s32 kmpp_meta_get_##func_type##_d(KmppMeta meta, KmppMetaKey key, arg_type *val, arg_type def) \
    { \
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta); \
        KmppMetaVal *meta_val = meta_key_to_addr(impl, key, key_type); \
        if (!meta_val) \
            return rk_nok; \
        if (KMPP_BOOL_CAS(&meta_val->state, META_READY_MASK, META_VAL_INVALID)) { \
            if (val) *val = meta_val->key_field; \
            KMPP_FETCH_SUB(&impl->node_count, 1); \
        } else { \
            if (val) *val = def; \
        } \
        return rk_ok; \
    } \
    EXPORT_SYMBOL(kmpp_meta_get_##func_type##_d);

KMPP_META_ACCESSOR(s32, rk_s32, TYPE_VAL_32, val_s32)
KMPP_META_ACCESSOR(s64, rk_s64, TYPE_VAL_64, val_s64)
KMPP_META_ACCESSOR(ptr, void *, TYPE_KPTR, val_ptr)

rk_s32 kmpp_meta_set_obj(KmppMeta meta, KmppMetaKey key, KmppObj val)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    if (KMPP_BOOL_CAS(&meta_obj->state, META_VAL_INVALID, META_VAL_VALID))
        KMPP_FETCH_ADD(&impl->node_count, 1);

    {
        KmppShm shm = kmpp_obj_to_shm(val);

        if (shm) {
            KmppShmPtr *ptr = kmpp_shm_to_shmptr(shm);

            meta_obj->val_shm.uaddr = ptr->uaddr;
            meta_obj->val_shm.kaddr = ptr->kaddr;;
            KMPP_FETCH_OR(&meta_obj->state, META_VAL_IS_SHM);
        } else {
            meta_obj->val_shm.uaddr = 0;
            meta_obj->val_shm.kptr = val;
            KMPP_FETCH_AND(&meta_obj->state, ~META_VAL_IS_SHM);
        }
    }
    KMPP_FETCH_OR(&meta_obj->state, META_VAL_READY);
    return rk_ok;
}

rk_s32 kmpp_meta_get_obj(KmppMeta meta, KmppMetaKey key, KmppObj *val)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    META_UNMASK_PROP(&meta_obj->state);
    if (KMPP_BOOL_CAS(&meta_obj->state, META_READY_MASK, META_VAL_INVALID)) {
        if (val)
            *val = meta_obj->val_shm.kptr;
        KMPP_FETCH_SUB(&impl->node_count, 1);
        return rk_ok;
    }

    return rk_nok;
}

rk_s32 kmpp_meta_get_obj_d(KmppMeta meta, KmppMetaKey key, KmppObj *val, KmppObj def)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    META_UNMASK_PROP(&meta_obj->state);
    if (KMPP_BOOL_CAS(&meta_obj->state, META_READY_MASK, META_VAL_INVALID)) {
        if (val)
            *val = meta_obj->val_shm.kptr;
        KMPP_FETCH_SUB(&impl->node_count, 1);
    } else {
        if (val)
            *val = def ? def : NULL;
    }

    return rk_ok;
}

rk_s32 kmpp_meta_set_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = (KmppMetaObj *)meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    if (KMPP_BOOL_CAS(&meta_obj->state, META_VAL_INVALID, META_VAL_VALID))
        KMPP_FETCH_ADD(&impl->node_count, 1);

    if (sptr) {
        meta_obj->val_shm.uaddr = sptr->uaddr;
        meta_obj->val_shm.kaddr = sptr->kaddr;
    } else {
        meta_obj->val_shm.uaddr = 0;
        meta_obj->val_shm.kptr = 0;
    }

    if (sptr->uaddr)
        KMPP_FETCH_OR(&meta_obj->state, META_VAL_IS_SHM);
    else
        KMPP_FETCH_AND(&meta_obj->state, ~META_VAL_IS_SHM);

    KMPP_FETCH_OR(&meta_obj->state, META_VAL_READY);

    return rk_ok;
}

rk_s32 kmpp_meta_get_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    META_UNMASK_PROP(&meta_obj->state);
    if (KMPP_BOOL_CAS(&meta_obj->state, META_READY_MASK, META_VAL_INVALID)) {
        if (sptr) {
            sptr->uaddr = meta_obj->val_shm.uaddr;
            sptr->kaddr = meta_obj->val_shm.kaddr;
        }
        KMPP_FETCH_SUB(&impl->node_count, 1);
        return rk_ok;
    }
    return rk_nok;
}

rk_s32 kmpp_meta_get_shm_d(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr, KmppShmPtr *def)
{
    KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
    KmppMetaObj *meta_obj = meta_key_to_addr(impl, key, TYPE_SPTR);

    if (!meta_obj)
        return rk_nok;

    META_UNMASK_PROP(&meta_obj->state);
    if (KMPP_BOOL_CAS(&meta_obj->state, META_READY_MASK, META_VAL_INVALID)) {
        if (sptr) {
            sptr->uaddr = meta_obj->val_shm.uaddr;
            sptr->kaddr = meta_obj->val_shm.kaddr;
        }
        KMPP_FETCH_SUB(&impl->node_count, 1);
    } else {
        if (sptr) {
            if (def) {
                sptr->uaddr = def->uaddr;
                sptr->kaddr = def->kaddr;
            } else {
                sptr->uaddr = 0;
                sptr->kaddr = 0;
            }
        }
    }

    return rk_ok;
}

EXPORT_SYMBOL(kmpp_meta_get);
EXPORT_SYMBOL(kmpp_meta_get_share);
EXPORT_SYMBOL(kmpp_meta_put);
EXPORT_SYMBOL(kmpp_meta_size);
EXPORT_SYMBOL(kmpp_meta_dump);
EXPORT_SYMBOL(kmpp_meta_set_obj);
EXPORT_SYMBOL(kmpp_meta_get_obj);
EXPORT_SYMBOL(kmpp_meta_get_obj_d);
EXPORT_SYMBOL(kmpp_meta_set_shm);
EXPORT_SYMBOL(kmpp_meta_get_shm);
EXPORT_SYMBOL(kmpp_meta_get_shm_d);
