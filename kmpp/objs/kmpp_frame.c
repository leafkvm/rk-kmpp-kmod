/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_frame"

#include <linux/kernel.h>

#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"

#include "kmpp_obj.h"

#include "kmpp_mod_id.h"
#include "kmpp_meta_impl.h"
#include "kmpp_frame_impl.h"
#include "kmpp_buffer_impl.h"

#include "rk_venc_cmd.h"
#include "mpp_maths.h"
#include "mpp_buffer.h"

static const char *module_name = MODULE_TAG;

static rk_u32 kmpp_frame_debug = 0;

static rk_s32 kmpp_frame_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    if (entry) {
        KmppFrameImpl *impl = (KmppFrameImpl*)entry;
        KmppMeta meta = NULL;

        impl->name = module_name;

        if (file)
            kmpp_meta_get_share(&meta, file, caller);
        else
            kmpp_meta_get(&meta, caller);

        kmpp_obj_to_shmptr(meta, &impl->meta);

        kmpp_logi_cf(kmpp_frame_debug, "frame %px meta %px : sptr %#llx:%#llx\n",
                     impl, meta, impl->meta.uaddr, impl->meta.kaddr);
    }

    return rk_ok;
}

static rk_s32 kmpp_frame_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    (void)obj;

    if (entry) {
        KmppFrameImpl *impl = (KmppFrameImpl*)entry;
        KmppMeta meta = NULL;
        KmppBuffer buffer = NULL;

        meta = kmpp_obj_from_shmptr(&impl->meta);

        kmpp_logi_cf(kmpp_frame_debug, "meta %px : sptr %#llx:%#llx\n",
                     meta, impl->meta.uaddr, impl->meta.kaddr);

        if (meta)
            kmpp_meta_put_f(meta);

        buffer = kmpp_obj_from_shmptr(&impl->buffer);

        kmpp_logi_cf(kmpp_frame_debug, "buffer %px : sptr %#llx:%#llx\n",
                     buffer, impl->buffer.uaddr, impl->buffer.kaddr);

        if (buffer)
            kmpp_buffer_put(buffer);
    }

    return rk_ok;
}

static rk_s32 kmpp_frame_impl_share(void *entry, KmppObj obj, KmppFsDev *file,
                                    const rk_u8 *caller)
{
    if (entry) {
        KmppFrameImpl *impl = (KmppFrameImpl *)entry;
        KmppBuffer buffer = NULL;
        KmppMeta meta = NULL;

        buffer = kmpp_obj_from_shmptr(&impl->buffer);
        if (buffer && !kmpp_obj_to_shm(buffer)) {
            if (!kmpp_obj_share_f(buffer))
                kmpp_obj_to_shmptr(buffer, &impl->buffer);
        }

        meta = kmpp_obj_from_shmptr(&impl->meta);
        if (meta && !kmpp_obj_to_shm(meta)) {
            if (!kmpp_obj_share_f(meta))
                kmpp_obj_to_shmptr(meta, &impl->meta);
        }
    }

    return rk_ok;
}

static rk_s32 kmpp_frame_impl_dump(void *entry)
{
    KmppFrameImpl *impl = (KmppFrameImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("width            %d\n", impl->width);
    kmpp_logi("height           %d\n", impl->height);
    kmpp_logi("hor_stride       %d\n", impl->hor_stride);
    kmpp_logi("ver_stride       %d\n", impl->ver_stride);
    kmpp_logi("hor_stride_pixel %d\n", impl->hor_stride_pixel);
    kmpp_logi("offset_x         %d\n", impl->offset_x);
    kmpp_logi("offset_y         %d\n", impl->offset_y);
    kmpp_logi("fmt              %d\n", impl->fmt);
    kmpp_logi("meta [u:k]       %#llx:%#llx\n", impl->meta.uaddr, impl->meta.kaddr);

    return rk_ok;
}

static MPP_RET check_is_mpp_frame(void *frame)
{
    if (frame && ((KmppFrameImpl *)frame)->name == module_name)
        return MPP_OK;

    kmpp_err_f("pointer %p failed on check\n", frame);

    return MPP_NOK;
}

static rk_s32 kmpp_frame_impl_set_buffer(void *entry, void *arg, const rk_u8 *caller)
{
    KmppFrameImpl *impl = (KmppFrameImpl*)entry;
    KmppShmPtr *sptr = (KmppShmPtr *)arg;
    KmppBuffer prev = NULL;
    KmppBuffer next = NULL;

    if (check_is_mpp_frame(impl))
        return rk_nok;

    prev = kmpp_obj_from_shmptr(&impl->buffer);
    next = kmpp_obj_from_shmptr(sptr);

    if (prev != next) {
        if (next)
            kmpp_buffer_inc_ref_f(next);

        if (prev)
            kmpp_buffer_put(prev);

        impl->buffer.uaddr = sptr ? sptr->uaddr : 0;
        impl->buffer.kaddr = sptr ? sptr->kaddr : 0;
    }

    return rk_ok;
}

static rk_s32 kmpp_frame_impl_get_buffer(void *entry, void *arg, const rk_u8 *caller)
{
    KmppFrameImpl *impl = (KmppFrameImpl*)entry;
    KmppShmPtr *buffer = (KmppShmPtr *)arg;

    if (check_is_mpp_frame(impl))
        return rk_nok;

    buffer->kaddr = impl->buffer.kaddr;
    buffer->uaddr = impl->buffer.uaddr;

    return rk_ok;
}

rk_s32 kmpp_frame_has_buffer(const KmppFrame frame)
{
    KmppFrameImpl *p = kmpp_obj_to_entry(frame);
    KmppBuffer buffer = NULL;

    if (check_is_mpp_frame(p))
        return 0;

    buffer = kmpp_obj_from_shmptr(&p->buffer);

    return buffer ? 1 : 0;
}

rk_s32 kmpp_frame_has_meta(const KmppFrame frame)
{
    KmppFrameImpl *p = kmpp_obj_to_entry(frame);

    if (check_is_mpp_frame(p))
        return 0;

    return 0;
}

MPP_RET kmpp_frame_copy(KmppFrame dst, KmppFrame src)
{
    KmppFrameImpl *dst_p = kmpp_obj_to_entry(dst);
    KmppFrameImpl *src_p = kmpp_obj_to_entry(src);
    KmppBuffer buffer = NULL;
    KmppMeta meta = NULL;

    if (NULL == dst || check_is_mpp_frame(src_p)) {
        kmpp_err_f("invalid input dst %p src %p\n", dst, src);
        return MPP_ERR_UNKNOW;
    }

    kmpp_memcpy(dst_p, src_p, sizeof(KmppFrameImpl));
    buffer = kmpp_obj_from_shmptr(&src_p->buffer);
    if (buffer)
        kmpp_buffer_inc_ref_f(buffer);

    meta = kmpp_obj_from_shmptr(&src_p->meta);
    if (meta)
        kmpp_meta_inc_ref(meta);

    return MPP_OK;
}

MPP_RET kmpp_frame_info_cmp(KmppFrame frame0, KmppFrame frame1)
{
    KmppFrameImpl *f0 = kmpp_obj_to_entry(frame0);
    KmppFrameImpl *f1 = kmpp_obj_to_entry(frame1);

    if (check_is_mpp_frame(f0) || check_is_mpp_frame(f1)) {
        kmpp_err_f("invalid NULL pointer input\n");
        return MPP_ERR_NULL_PTR;
    }

    if ((f0->width == f1->width) &&
        (f0->height == f1->height) &&
        (f0->hor_stride == f1->hor_stride) &&
        (f0->ver_stride == f1->ver_stride) &&
        (f0->fmt == f1->fmt) && (f0->buf_size == f1->buf_size))
        return MPP_OK;

    return MPP_NOK;
}

rk_s32 kmpp_frame_get_fbc_offset(KmppFrame frame, rk_u32 *offset)
{
    KmppFrameImpl *p = kmpp_obj_to_entry(frame);

    if (check_is_mpp_frame(p))
        return rk_nok;

    if (MPP_FRAME_FMT_IS_FBC(p->fmt)) {
        rk_u32 fbc_version = p->fmt & MPP_FRAME_FBC_MASK;
        rk_u32 fbc_offset = 0;

        if (fbc_version == MPP_FRAME_FBC_AFBC_V1) {
            fbc_offset = KMPP_ALIGN(p->width, 16) * KMPP_ALIGN(p->height, 16) / 16;
            fbc_offset = KMPP_ALIGN(fbc_offset, SZ_4K);
        } else if (fbc_version == MPP_FRAME_FBC_AFBC_V2 ||
                   fbc_version == MPP_FRAME_FBC_RKFBC)
            fbc_offset = 0;
        p->fbc_offset = fbc_offset;
    }
    *offset = p->fbc_offset;

    return rk_ok;
}

rk_s32 kmpp_frame_get_fbc_stride(KmppFrame frame, rk_u32 *stride)
{
    KmppFrameImpl *p = kmpp_obj_to_entry(frame);

    if (check_is_mpp_frame(p))
        return rk_nok;

    *stride = KMPP_ALIGN(p->width, 16);

    return rk_ok;
}

EXPORT_SYMBOL(kmpp_frame_has_meta);
EXPORT_SYMBOL(kmpp_frame_copy);
EXPORT_SYMBOL(kmpp_frame_info_cmp);
EXPORT_SYMBOL(kmpp_frame_get_fbc_offset);
EXPORT_SYMBOL(kmpp_frame_get_fbc_stride);

#define KMPP_OBJ_NAME               kmpp_frame
#define KMPP_OBJ_INTF_TYPE          KmppFrame
#define KMPP_OBJ_IMPL_TYPE          KmppFrameImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_FRAME_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_frame_impl_init
#define KMPP_OBJ_FUNC_DEINIT        kmpp_frame_impl_deinit
#define KMPP_OBJ_FUNC_SHARE         kmpp_frame_impl_share
#define KMPP_OBJ_FUNC_DUMP          kmpp_frame_impl_dump
#include "kmpp_obj_helper.h"

KMPP_MODULE_ADD(kmpp, KMPP_MOD_FRAME, frame, kmpp_frame_objdef_get, kmpp_frame_objdef_put)
