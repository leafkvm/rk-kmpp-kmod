/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_IO_H__
#define __KMPP_IO_H__

#include "kmpp_log.h"
#include "kmpp_dev.h"

void kmpp_writel(rk_u32 val, void *p);
rk_u32 kmpp_readl(void *p);
void kmpp_writel_relaxed(rk_u32 val, void *p);
rk_u32 kmpp_readl_relaxed(void *p);

static inline void kmpp_dev_wr_reg(KmppDev *dev, rk_u32 offset, rk_u32 val);
static inline void kmpp_dev_rd_reg(KmppDev *dev, rk_u32 offset, rk_u32 *val);
static inline void kmpp_dev_wr_regs(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val);
static inline void kmpp_dev_rd_regs(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val);

static inline void kmpp_dev_wr_reg_relaxed(KmppDev *dev, rk_u32 offset, rk_u32 val);
static inline void kmpp_dev_rd_reg_relaxed(KmppDev *dev, rk_u32 offset, rk_u32 *val);
static inline void kmpp_dev_wr_regs_relaxed(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val);
static inline void kmpp_dev_rd_regs_relaxed(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val);

/* write register with resource segment index */
static inline void kmpp_dev_wr_seg_reg(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 val);
static inline void kmpp_dev_rd_seg_reg(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 *val);
static inline void kmpp_dev_wr_seg_regs(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val);
static inline void kmpp_dev_rd_seg_regs(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val);

static inline void kmpp_dev_wr_seg_reg_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 val);
static inline void kmpp_dev_rd_seg_reg_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 *val);
static inline void kmpp_dev_wr_seg_regs_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val);
static inline void kmpp_dev_rd_seg_regs_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val);

static inline void __seg_wr_reg(const char *dev_name, KmppDevRes *res, rk_u32 offset, rk_u32 val)
{
    kmpp_logd("%-8s %s write reg[%03d]: 0x%04x: 0x%08x\n", dev_name, res->name,
              offset / sizeof(rk_u32), offset, val);
    kmpp_writel(val, res->base + offset);
}

static inline rk_u32 __seg_rd_reg(const char *dev_name, KmppDevRes *res, rk_u32 offset)
{
    rk_u32 val = kmpp_readl(res->base + offset);

    kmpp_logd("%-8s %s read reg[%03d]: 0x%04x: 0x%08x\n", dev_name, res->name,
              offset / sizeof(rk_u32), offset, val);
    return val;
}

static inline void __seg_wr_reg_relaxed(const char *dev_name, KmppDevRes *res, rk_u32 offset, rk_u32 val)
{
    kmpp_logd("%-8s %s write reg[%03d]: 0x%04x: 0x%08x\n", dev_name, res->name,
              offset / sizeof(rk_u32), offset, val);
    kmpp_writel_relaxed(val, res->base + offset);
}

static inline rk_u32 __seg_rd_reg_relaxed(const char *dev_name, KmppDevRes *res, rk_u32 offset)
{
    rk_u32 val = kmpp_readl_relaxed(res->base + offset);

    kmpp_logd("%-8s %s read reg[%03d]: 0x%04x: 0x%08x\n", dev_name, res->name,
              offset / sizeof(rk_u32), offset, val);
    return val;
}

static inline void kmpp_dev_wr_reg(KmppDev *dev, rk_u32 offset, rk_u32 val)
{
    __seg_wr_reg(dev->name, &dev->res[0], offset, val);
}

static inline void kmpp_dev_rd_reg(KmppDev *dev, rk_u32 offset, rk_u32 *val)
{
    *val = __seg_rd_reg(dev->name, &dev->res[0], offset);
}

static inline void kmpp_dev_wr_regs(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        __seg_wr_reg(dev->name, &dev->res[0], offset, *val++);
}

static inline void kmpp_dev_rd_regs(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        *val++ = __seg_rd_reg(dev->name, &dev->res[0], offset);
}

static inline void kmpp_dev_wr_reg_relaxed(KmppDev *dev, rk_u32 offset, rk_u32 val)
{
    __seg_wr_reg_relaxed(dev->name, &dev->res[0], offset, val);
}

static inline void kmpp_dev_rd_reg_relaxed(KmppDev *dev, rk_u32 offset, rk_u32 *val)
{
    *val = __seg_rd_reg_relaxed(dev->name, &dev->res[0], offset);
}

static inline void kmpp_dev_wr_regs_relaxed(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        __seg_wr_reg_relaxed(dev->name, &dev->res[0], offset, *val++);
}

static inline void kmpp_dev_rd_regs_relaxed(KmppDev *dev, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        *val++ = __seg_rd_reg_relaxed(dev->name, &dev->res[0], offset);
}

static inline void kmpp_dev_wr_seg_reg(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 val)
{
    __seg_wr_reg(dev->name, &dev->res[seg], offset, val);
}

static inline void kmpp_dev_rd_seg_reg(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 *val)
{
    *val = __seg_rd_reg(dev->name, &dev->res[seg], offset);
}

static inline void kmpp_dev_wr_seg_regs(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        __seg_wr_reg(dev->name, &dev->res[seg], offset, *val++);
}

static inline void kmpp_dev_rd_seg_regs(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        *val++ = __seg_rd_reg(dev->name, &dev->res[seg], offset);
}

static inline void kmpp_dev_wr_seg_reg_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 val)
{
    __seg_wr_reg_relaxed(dev->name, &dev->res[seg], offset, val);
}

static inline void kmpp_dev_rd_seg_reg_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 offset, rk_u32 *val)
{
    *val = __seg_rd_reg_relaxed(dev->name, &dev->res[seg], offset);
}

static inline void kmpp_dev_wr_seg_regs_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        __seg_wr_reg_relaxed(dev->name, &dev->res[seg], offset, *val++);
}

static inline void kmpp_dev_rd_seg_regs_relaxed(KmppDev *dev, rk_s32 seg, rk_u32 start, rk_u32 end, rk_u32 *val)
{
    rk_u32 offset;

    for (offset = start; offset < end; offset += 4)
        *val++ = __seg_rd_reg_relaxed(dev->name, &dev->res[seg], offset);
}

#endif /* __KMPP_IO_H__ */