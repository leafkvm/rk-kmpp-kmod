/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_packet"

#include <linux/kernel.h>

#include "kmpp_cls.h"
#include "kmpp_log.h"
#include "kmpp_obj.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_string.h"

#include "kmpp_mod_id.h"
#include "kmpp_meta.h"
#include "kmpp_buffer_impl.h"
#include "mpp_buffer_impl.h"
#include "kmpp_packet_impl.h"

static const char *module_name = MODULE_TAG;

#define check_is_mpp_packet_f(packet, caller) \
    check_is_mpp_packet(packet, __FUNCTION__, caller)

static inline rk_s32 check_is_mpp_packet(void *packet, const rk_u8 *func, const rk_u8 *caller)
{
    if (packet && ((KmppPacketImpl *)packet)->name == module_name)
        return rk_ok;

    kmpp_err_f("pointer %p failed on check at %s from %s\n", packet, func, caller);

    return rk_nok;
}

static rk_s32 kmpp_packet_impl_init(void *entry, KmppObj obj, KmppFsDev *file,
                                    const rk_u8 *caller)
{
    if (entry) {
        KmppPacketImpl *impl = (KmppPacketImpl *)entry;
        KmppMeta meta = NULL;

        impl->name = module_name;
        if (file)
            kmpp_meta_get_share(&meta, file, caller);
        else
            kmpp_meta_get(&meta, caller);

        kmpp_obj_to_shmptr(meta, &impl->meta);
    }

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    (void)obj;

    if (entry) {
        KmppPacketImpl *impl = (KmppPacketImpl *)entry;
        KmppBuffer buffer = NULL;
        KmppMeta meta = NULL;

        buffer = kmpp_obj_from_shmptr(&impl->buffer);
        if (buffer)
            kmpp_buffer_put(buffer);

        meta = kmpp_obj_from_shmptr(&impl->meta);
        if (meta)
            kmpp_meta_put_f(meta);

        if (impl->flag & KMPP_PACKET_FLAG_INTERNAL)
            kmpp_free(impl->data.kptr);

        if (impl->buf.ring_pool) {
            ring_buf_put_free(impl->buf.ring_pool, &impl->buf);
            impl->buf.ring_pool = NULL;
        }
    }

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_share(void *entry, KmppObj obj, KmppFsDev *file,
                                     const rk_u8 *caller)
{
    if (entry) {
        KmppPacketImpl *impl = (KmppPacketImpl *)entry;
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

static rk_s32 kmpp_packet_impl_dump(void *entry)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;

    if (!impl) {
        kmpp_loge_f("invalid param packet NULL\n");
        return rk_nok;
    }

    kmpp_logi("size             %d\n", impl->size);
    kmpp_logi("length           %d\n", impl->length);
    kmpp_logi("pts              %lld\n", impl->pts);
    kmpp_logi("dts              %lld\n", impl->dts);
    kmpp_logi("status           %d\n", impl->status);
    kmpp_logi("flag             %#x\n", impl->flag);
    kmpp_logi("temporal_id      %d\n", impl->temporal_id);
    kmpp_logi("data [u:k]       %#llx:%#llx\n", impl->data.uaddr, impl->data.kaddr);
    kmpp_logi("pos [u:k]        %#llx:%#llx\n", impl->pos.uaddr, impl->pos.kaddr);
    kmpp_logi("buffer [u:k]     %#llx:%#llx\n", impl->buffer.uaddr, impl->buffer.kaddr);
    kmpp_logi("meta [u:k]       %#llx:%#llx\n", impl->meta.uaddr, impl->meta.kaddr);

    return rk_ok;
}

rk_s32 kmpp_packet_init_with_data(KmppPacket *packet, void *data, size_t size)
{
    KmppPacketImpl *impl = NULL;
    MPP_RET ret = MPP_OK;

    if (NULL == packet) {
        kmpp_err_f("invalid NULL input packet\n");
        return MPP_ERR_NULL_PTR;
    }

    ret = kmpp_packet_get(packet);
    if (ret) {
        kmpp_err_f("kmpp_packet_get failed ret %d\n", ret);
        return MPP_ERR_MALLOC;
    }

    impl = (KmppPacketImpl *)kmpp_obj_to_entry(*packet);
    impl->data.kptr = data;
    impl->pos.kptr = data;
    impl->size = impl->length = size;

    return rk_ok;
}

rk_s32 kmpp_packet_init_with_buffer(KmppPacket *packet, KmppShmPtr buffer)
{
    KmppPacketImpl *impl = NULL;
    rk_s32 ret;
    KmppBuffer buffer_obj = kmpp_obj_from_shmptr(&buffer);
    KmppBufCfgImpl *cfg;

    if (NULL == packet || NULL == buffer_obj) {
        kmpp_err_f("invalid input packet %p buffer %p\n", packet, buffer_obj);
        return MPP_ERR_NULL_PTR;
    }

    ret = kmpp_packet_get(packet);
    if (ret) {
        kmpp_err_f("kmpp_packet_get failed ret %d\n", ret);
        return MPP_ERR_MALLOC;
    }

    cfg = kmpp_obj_to_entry(kmpp_buffer_get_cfg_k(buffer_obj));
    impl = (KmppPacketImpl *)kmpp_obj_to_entry(*packet);
    impl->data.kptr = impl->pos.kptr = kmpp_buffer_get_kptr(buffer_obj);
    impl->data.uaddr = impl->pos.uaddr = kmpp_buffer_get_uptr(buffer_obj);
    impl->size = impl->length = cfg->size;
    impl->buffer = buffer;
    kmpp_buffer_inc_ref_f(buffer_obj);

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_set_buffer(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;
    KmppShmPtr *sptr = (KmppShmPtr *)arg;
    KmppBuffer prev = NULL;
    KmppBuffer next = NULL;

    if (check_is_mpp_packet_f(impl, caller))
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

static rk_s32 kmpp_packet_impl_get_buffer(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl*)entry;
    KmppShmPtr *buffer = (KmppShmPtr *)arg;

    if (check_is_mpp_packet_f(impl, caller))
        return rk_nok;

    buffer->kaddr = impl->buffer.kaddr;
    buffer->uaddr = impl->buffer.uaddr;

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_set_pos(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;
    KmppShmPtr *pos = (KmppShmPtr *)arg;
    size_t offset = 0;
    size_t diff = 0;

    if (check_is_mpp_packet_f(impl, caller))
        return rk_nok;

    offset = (RK_U8 *)pos->kptr - (RK_U8 *)impl->data.kptr;
    diff = (RK_U8 *)pos->kptr - (RK_U8 *)impl->pos.kptr;

    /*
     * If set pos is a simple update on original buffer update the length
     * If set pos setup a new buffer reset length to size - offset
     * This will avoid assert on change "data" in kmpp_packet
     */
    if (diff <= impl->length)
        impl->length -= diff;
    else
        impl->length = impl->size - offset;

    impl->pos.kptr = pos->kptr;
    impl->pos.uptr = pos->uptr;
    kmpp_assert(impl->data.kptr <= impl->pos.kptr);
    kmpp_assert(impl->size >= impl->length);

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_get_pos(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;
    KmppShmPtr *pos = (KmppShmPtr *)arg;

    if (check_is_mpp_packet_f(impl, caller))
        return rk_nok;

    pos->kptr = impl->pos.kptr;
    pos->uptr = impl->pos.uptr;

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_set_flag(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;
    rk_u32 flag = *(rk_u32 *)arg;

    if (check_is_mpp_packet_f(impl, caller))
        return rk_nok;

    impl->flag |= flag;

    return rk_ok;
}

static rk_s32 kmpp_packet_impl_get_flag(void *entry, void *arg, const rk_u8 *caller)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)entry;
    rk_u32 *flag = (rk_u32 *)arg;

    if (check_is_mpp_packet_f(impl, caller))
        return rk_nok;

    *flag = impl->flag;

    return rk_ok;
}

rk_s32 kmpp_packet_has_buffer(KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    KmppBuffer buffer = NULL;

    if (check_is_mpp_packet_f(impl, NULL))
        return 0;

    buffer = kmpp_obj_from_shmptr(&impl->buffer);

    return buffer ? 1 : 0;
}

rk_s32 kmpp_packet_set_eos(KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);

    if (check_is_mpp_packet_f(impl, NULL))
        return rk_nok;

    impl->flag |= KMPP_PACKET_FLAG_EOS;

    return rk_ok;
}

rk_s32 kmpp_packet_clr_eos(KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);

    if (check_is_mpp_packet_f(impl, NULL))
        return rk_nok;

    impl->flag &= ~KMPP_PACKET_FLAG_EOS;

    return rk_ok;
}

rk_s32 kmpp_packet_get_eos(KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);

    if (check_is_mpp_packet_f(impl, NULL))
        return 0;

    return (impl->flag & KMPP_PACKET_FLAG_EOS) ? (1) : (0);
}

rk_u32 kmpp_packet_is_partition(const KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    KmppPacketStatus status;

    if (check_is_mpp_packet_f(impl, NULL))
        return 0;

    status.val = impl->status;

    return status.partition;
}

rk_u32 kmpp_packet_is_soi(const KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    KmppPacketStatus status;

    if (check_is_mpp_packet_f(impl, NULL))
        return 0;

    status.val = impl->status;

    return status.soi;
}

rk_u32 kmpp_packet_is_eoi(const KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    KmppPacketStatus status;

    if (check_is_mpp_packet_f(impl, NULL))
        return 0;

    status.val = impl->status;

    return status.eoi;
}

rk_s32 kmpp_packet_read(KmppPacket packet, size_t offset, void *data, size_t size)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    void *src = NULL;

    if (check_is_mpp_packet_f(impl, NULL) || NULL == data) {
        kmpp_err_f("invalid input: packet %p data %p\n", packet, data);
        return MPP_ERR_UNKNOW;
    }

    if (0 == size)
        return rk_ok;

    src = impl->data.kptr;
    kmpp_assert(src != NULL);
    kmpp_memcpy(data, (char *)src + offset, size);

    return rk_ok;
}

rk_s32 kmpp_packet_write(KmppPacket packet, size_t offset, void *data, size_t size)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    void *dst = NULL;

    if (check_is_mpp_packet_f(impl, NULL) || NULL == data) {
        kmpp_err_f("invalid input: packet %p data %p\n", packet, data);
        return MPP_ERR_UNKNOW;
    }

    if (0 == size)
        return rk_ok;

    dst = impl->data.kptr;
    kmpp_assert(dst != NULL);
    kmpp_memcpy((char *)dst + offset, data, size);

    return rk_ok;
}

rk_s32 kmpp_packet_copy(KmppPacket dst, KmppPacket src)
{
    KmppPacketImpl *dst_impl = (KmppPacketImpl *)kmpp_obj_to_entry(dst);
    KmppPacketImpl *src_impl = (KmppPacketImpl *)kmpp_obj_to_entry(src);

    if (check_is_mpp_packet_f(dst_impl, NULL) ||
        check_is_mpp_packet_f(src_impl, NULL)) {
        kmpp_err_f("invalid input: dst %p src %p\n", dst, src);
        return MPP_ERR_UNKNOW;
    }

    kmpp_memcpy(dst_impl->pos.kptr, src_impl->pos.kptr, src_impl->length);
    dst_impl->length = src_impl->length;

    return rk_ok;
}

rk_s32 kmpp_packet_append(KmppPacket dst, KmppPacket src)
{
    KmppPacketImpl *dst_impl = (KmppPacketImpl *)kmpp_obj_to_entry(dst);
    KmppPacketImpl *src_impl = (KmppPacketImpl *)kmpp_obj_to_entry(src);

    if (check_is_mpp_packet_f(dst_impl, NULL) ||
        check_is_mpp_packet_f(src_impl, NULL)) {
        kmpp_err_f("invalid input: dst %p src %p\n", dst, src);
        return MPP_ERR_UNKNOW;
    }

    kmpp_memcpy((RK_U8 *)dst_impl->pos.kptr + dst_impl->length, src_impl->pos.kptr, src_impl->length);
    dst_impl->length += src_impl->length;

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_packet
#define KMPP_OBJ_INTF_TYPE          KmppPacket
#define KMPP_OBJ_IMPL_TYPE          KmppPacketImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_PACKET_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_packet_impl_init
#define KMPP_OBJ_FUNC_SHARE         kmpp_packet_impl_share
#define KMPP_OBJ_FUNC_DUMP          kmpp_packet_impl_dump
#define KMPP_OBJ_FUNC_DEINIT        kmpp_packet_impl_deinit
#include "kmpp_obj_helper.h"

KMPP_MODULE_ADD(kmpp, KMPP_MOD_PACKET, packet, kmpp_packet_objdef_get, kmpp_packet_objdef_put)

EXPORT_SYMBOL(kmpp_packet_init_with_data);
EXPORT_SYMBOL(kmpp_packet_init_with_buffer);
EXPORT_SYMBOL(kmpp_packet_set_eos);
EXPORT_SYMBOL(kmpp_packet_clr_eos);
EXPORT_SYMBOL(kmpp_packet_get_eos);
EXPORT_SYMBOL(kmpp_packet_is_partition);
EXPORT_SYMBOL(kmpp_packet_is_soi);
EXPORT_SYMBOL(kmpp_packet_is_eoi);
EXPORT_SYMBOL(kmpp_packet_read);
EXPORT_SYMBOL(kmpp_packet_write);
EXPORT_SYMBOL(kmpp_packet_copy);
EXPORT_SYMBOL(kmpp_packet_append);
