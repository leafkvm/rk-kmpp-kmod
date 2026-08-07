/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_FILE_H__
#define __KMPP_FILE_H__

#include "rk_type.h"

#define KMPP_SEEK_SET       0
#define KMPP_SEEK_CUR       1
#define KMPP_SEEK_END       2

#define KMPP_O_ACCMODE      00000003
#define KMPP_O_RDONLY       00000000
#define KMPP_O_WRONLY       00000001
#define KMPP_O_RDWR         00000002
#define KMPP_O_CREAT        00000100
#define KMPP_O_EXCL         00000200
#define KMPP_O_NOCTTY       00000400
#define KMPP_O_TRUNC        00001000
#define KMPP_O_APPEND       00002000
#define KMPP_O_NONBLOCK     00004000

typedef struct KmppFile_t {
    void *file;
} KmppFile;

KmppFile *kmpp_fopen(const char *filename, int flags, int mode);
void   kmpp_fclose(KmppFile *file);

rk_s32 kmpp_fread(KmppFile *file, char *buf, unsigned int len);
rk_s32 kmpp_fwrite(KmppFile *file, const char *buf, int len);
rk_s32 kmpp_fseek(KmppFile *file, rk_s32 offset, int whence);
rk_s64 kmpp_fsize(KmppFile *file);

#endif /* __KMPP_FILE_H__ */