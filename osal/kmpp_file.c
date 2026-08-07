/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/fs.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_file.h"

KmppFile *kmpp_fopen(const char *filename, int flags, int mode)
{
    if (filename) {
        KmppFile *p = NULL;
        struct file *f;

        p = kmpp_malloc(sizeof(KmppFile));
        if (!p)
            return NULL;

        f = filp_open(filename, flags, mode);
        if (IS_ERR_OR_NULL(f)) {
            kmpp_free(p);
            return NULL;
        }

        p->file = f;
        return p;
    }

    return NULL;
}
EXPORT_SYMBOL(kmpp_fopen);

void kmpp_fclose(KmppFile *file)
{
    if (file) {
        filp_close(file->file, NULL);
        kmpp_free(file);
    }
}
EXPORT_SYMBOL(kmpp_fclose);

rk_s32 kmpp_fread(KmppFile *file, char *buf, unsigned int len)
{
    if (file && file->file && buf && len > 0) {
        struct file *f = file->file;

        return kernel_read(f, buf, len, &f->f_pos);
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_fread);

rk_s32 kmpp_fwrite(KmppFile *file, const char *buf, int len)
{
    if (file && file->file && buf && len > 0) {
        struct file *f = file->file;

        return kernel_write(f, buf, len, &f->f_pos);
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_fwrite);

rk_s32 kmpp_fseek(KmppFile *file, rk_s32 offset, int whence)
{
    if (file && file->file) {
        struct file *f = file->file;

        return default_llseek(f, offset, whence);
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_fseek);

rk_s64 kmpp_fsize(KmppFile *file)
{
    if (file && file->file) {
        struct file *f = file->file;

        return i_size_read(file_inode(f));
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_fsize);
