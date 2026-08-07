/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_env"

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "kmpp_list.h"

#include "kmpp_macro.h"
#include "kmpp_string.h"
#include "kmpp_log.h"
#include "kmpp_env.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"
#include "kmpp_spinlock.h"

#define get_env_srv_f() \
    ({ \
        KmppEnvSrv *__tmp; \
        if (srv_env) { \
            __tmp = srv_env; \
        } else { \
            kmpp_loge_f("kmpp env srv not init at %s\n", __FUNCTION__); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

typedef struct KmppEnvGrpImpl_t {
    rk_u8 name[64];
    /* list to global list_env_grp */
    kmpp_list_head list_group;
    /* list to parent groups */
    kmpp_list_head list_parent;
    /* list for child groups */
    kmpp_list_head list_child;
    /* list for nodes in the group */
    kmpp_list_head list_nodes;

    rk_u32 is_root;
    struct proc_dir_entry *dir;
} KmppEnvGrpImpl;

typedef struct KmppEnvNodeImpl_t {
    kmpp_list_head list;
    KmppEnvInfo info;
    rk_s32 str_val_size;
    KmppEnvGrpImpl *grp;
    struct seq_file *seq;
    void *pde;
} KmppEnvNodeImpl;

typedef struct KmppEnvSrv_t {
    kmpp_list_head list_env_grp;
    kmpp_list_head list_env_node;
    KmppSpinlock *lock_env;

    KmppEnvGrp root;
    KmppEnvGrp osal;
    KmppEnvGrp debug;
} KmppEnvSrv;

static KmppEnvSrv *srv_env = NULL;

void kmpp_env_log(KmppEnvNode node, const rk_u8 *fmt, ...)
{
    KmppEnvNodeImpl *impl = (KmppEnvNodeImpl *)node;
    va_list args;

    if (!impl || !impl->seq)
        return;

    va_start(args, fmt);
    seq_vprintf((struct seq_file *)impl->seq, fmt, args);
    va_end(args);
}

static int env_fops_show(struct seq_file *file, void *v)
{
    KmppEnvNodeImpl *node = (KmppEnvNodeImpl *)(file->private);
    KmppEnvInfo *info = &node->info;
    void *val = info->val;

    switch (info->type) {
    case KmppEnv_s32 : {
        seq_printf(file, "%u\n", ((rk_s32 *)val)[0]);
    } break;
    case KmppEnv_u32 : {
        seq_printf(file, "%u\n", ((rk_u32 *)val)[0]);
    } break;
    case KmppEnv_s64 : {
        seq_printf(file, "%lld\n", ((rk_s64 *)val)[0]);
    } break;
    case KmppEnv_u64 : {
        seq_printf(file, "0x%llx\n", ((rk_u64 *)val)[0]);
    } break;
    case KmppEnv_str : {
        seq_printf(file, "%s\n", (char *)val);
    } break;
    case KmppEnv_user : {
        if (info->env_show)
            info->env_show(node, info->val);
    } break;
    default : {
        seq_printf(file, "unknown type\n");
    } break;
    }

    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
#define PDE_DATA(inode)         pde_data(inode)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
/* already has the Macro definition */
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
#define PDE_DATA(inode)         NULL
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */

static int env_fops_open(struct inode *inode, struct file *file)
{
    KmppEnvNodeImpl *node = (KmppEnvNodeImpl *)(PDE_DATA(inode));
    int ret;

    ret = single_open(file, env_fops_show, node);
    node->seq = file->private_data;
    return ret;
}

static ssize_t env_fops_write(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct seq_file *priv = file->private_data;
    KmppEnvNodeImpl *node = (KmppEnvNodeImpl *)(priv->private);
    char *val = (char *)node->info.val;
    int ret;

    if (priv != node->seq) {
        kmpp_loge_f("seq file mismatch %px - %px\n", priv, node->seq);
        return -EINVAL;
    }

    if (node->info.readonly)
        return -EPERM;

    switch (node->info.type) {
    case KmppEnv_s32 : {
        ret = kstrtos32_from_user(buf, count, 0, (rk_s32 *)val);
    } break;
    case KmppEnv_u32 : {
        ret = kstrtou32_from_user(buf, count, 0, (rk_u32 *)val);
    } break;
    case KmppEnv_s64 : {
        ret = kstrtos64_from_user(buf, count, 0, (rk_s64 *)val);
    } break;
    case KmppEnv_u64 : {
        ret = kstrtou64_from_user(buf, count, 0, (rk_u64 *)val);
    } break;
    case KmppEnv_str : {
        if (count > node->str_val_size) {
            node->str_val_size = KMPP_ALIGN(count, sizeof(rk_u64));
            val = kmpp_calloc(node->str_val_size);
            if (!val) {
                kmpp_loge_f("kmpp_calloc %d failed\n", node->str_val_size);
                return -ENOMEM;
            }
            kmpp_free(node->info.val);
            node->info.val = val;
        }
        ret = copy_from_user(val, buf, count);
        if (val[count - 1] != '\0')
            val[count - 1] = '\0';
    } break;
    default : {
        ret = -EINVAL;
    } break;
    }

    if (ret)
        return ret;

    return count;
}

static const struct proc_ops kmpp_env_fops = {
    .proc_open = env_fops_open,
    .proc_read = seq_read,
    .proc_release = single_release,
    .proc_write = env_fops_write,
};

static rk_s32 kmpp_env_init(void)
{
    KmppEnvSrv *srv = srv_env;
    rk_s32 lock_size;
    rk_s32 size;

    if (srv)
        return rk_ok;

    lock_size = kmpp_spinlock_size();
    size = lock_size + sizeof(*srv);
    srv = kmpp_calloc(size);
    if (!srv) {
        kmpp_loge_f("kmpp_calloc %d failed\n", size);
        return rk_nok;
    }

    srv_env = srv;

    KMPP_INIT_LIST_HEAD(&srv->list_env_grp);
    KMPP_INIT_LIST_HEAD(&srv->list_env_node);
    kmpp_spinlock_assign(&srv->lock_env, (void *)(srv + 1), lock_size);

    kmpp_env_get(&srv->root, NULL, "kmpp");
    kmpp_env_get(&srv->osal, srv->root, "osal");
    kmpp_env_get(&srv->debug, srv->root, "debug");

    kmpp_mem_env_init();

    return rk_ok;
}

static rk_s32 kmpp_env_deinit(void)
{
    KmppEnvGrpImpl *grp;
    KmppEnvSrv *srv = srv_env;

    if (!srv)
        return rk_ok;

    kmpp_spin_lock(srv->lock_env);
    do {
        grp = kmpp_list_first_entry_or_null(&srv->list_env_grp, KmppEnvGrpImpl, list_group);
        if (!grp)
            break;

        kmpp_spin_unlock(srv->lock_env);

        kmpp_env_put(grp);

        kmpp_spin_lock(srv->lock_env);
    } while (grp);
    kmpp_spin_unlock(srv->lock_env);

    if (!kmpp_list_empty(&srv->list_env_grp))
        kmpp_loge_f("list_env_grp is still not empty\n");

    if (!kmpp_list_empty(&srv->list_env_node))
        kmpp_loge_f("list_env_node is still not empty\n");

    kmpp_spinlock_deinit(&srv->lock_env);

    kmpp_free(srv);
    srv_env = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_ENV, env, kmpp_env_init, kmpp_env_deinit);

rk_s32 kmpp_env_get(KmppEnvGrp *env, KmppEnvGrp parent, const rk_u8 *name)
{
    KmppEnvSrv *srv = get_env_srv_f();
    KmppEnvGrpImpl *grp;

    if (!env || !name) {
        kmpp_loge_f("invalid param env %px name %s\n", env, name);
        return rk_nok;
    }

    *env = NULL;

    if (!srv)
        return rk_nok;

    grp = kmpp_calloc_atomic(sizeof(*grp));
    if (!grp) {
        kmpp_loge_f("failed to create KmppEnvGrp size %d\n", sizeof(*grp));
        return rk_nok;
    }

    KMPP_INIT_LIST_HEAD(&grp->list_group);
    KMPP_INIT_LIST_HEAD(&grp->list_child);
    KMPP_INIT_LIST_HEAD(&grp->list_parent);
    KMPP_INIT_LIST_HEAD(&grp->list_nodes);

    kmpp_strncpy(grp->name, name, sizeof(grp->name));

    if (parent) {
        KmppEnvGrpImpl *p = (KmppEnvGrpImpl *)parent;

        grp->dir = proc_mkdir(grp->name, p->dir);

        kmpp_spin_lock(srv->lock_env);
        kmpp_list_add_tail(&grp->list_group, &srv->list_env_grp);
        kmpp_list_add_tail(&grp->list_parent, &p->list_child);
        kmpp_spin_unlock(srv->lock_env);
    } else {
        grp->dir = proc_mkdir(grp->name, NULL);
        grp->is_root = 1;

        kmpp_spin_lock(srv->lock_env);
        kmpp_list_add_tail(&grp->list_group, &srv->list_env_grp);
        kmpp_spin_unlock(srv->lock_env);
    }

    *env = grp;

    return rk_ok;
}

rk_s32 kmpp_env_put(KmppEnvGrp env)
{
    KmppEnvSrv *srv = get_env_srv_f();
    KmppEnvGrpImpl *grp = (KmppEnvGrpImpl *)env;
    KmppEnvGrpImpl *n_grp;
    KmppEnvGrpImpl *grp_child;
    KmppEnvNodeImpl *node, *n_node;
    kmpp_list_head local_list_node;

    if (!env) {
        kmpp_loge_f("invalid param env %px\n", env);
        return rk_nok;
    }

    if (!srv)
        return rk_nok;

    /* release child env group first */
    kmpp_spin_lock(srv->lock_env);

    /* break link on list_group and list_parent */
    kmpp_list_del_init(&grp->list_group);
    kmpp_list_del_init(&grp->list_parent);

    kmpp_list_for_each_entry_safe(grp_child, n_grp, &grp->list_child, KmppEnvGrpImpl, list_parent) {
        kmpp_spin_unlock(srv->lock_env);

        kmpp_env_put(grp_child);

        kmpp_spin_lock(srv->lock_env);
    }
    kmpp_spin_unlock(srv->lock_env);

    KMPP_INIT_LIST_HEAD(&local_list_node);

    kmpp_spin_lock(srv->lock_env);
    if (!kmpp_list_empty(&grp->list_nodes))
        kmpp_list_splice_tail_init(&grp->list_nodes, &local_list_node);
    kmpp_spin_unlock(srv->lock_env);

    kmpp_list_for_each_entry_safe(node, n_node, &local_list_node, KmppEnvNodeImpl, list) {
        kmpp_env_del(grp, node);
    }

    /* remove only root procfs directory */
    if (grp->dir && grp->is_root) {
        proc_remove(grp->dir);
        grp->dir = NULL;
    }

    /* NOTE: release original buffer */
    kmpp_free(env);

    return rk_ok;
}

rk_s32 kmpp_env_add(KmppEnvGrp env, KmppEnvNode *node, KmppEnvInfo *info)
{
    KmppEnvSrv *srv = get_env_srv_f();
    struct proc_dir_entry *pde = NULL;
    KmppEnvGrpImpl *grp = (KmppEnvGrpImpl *)env;
    KmppEnvNodeImpl *n = NULL;
    rk_s32 name_len;

    if (!env || !info) {
        kmpp_loge_f("invalid param env %px node %px info %px\n", env, node, info);
        return rk_nok;
    }

    if (node)
        *node = NULL;

    if (!srv)
        return rk_nok;

    name_len = KMPP_ALIGN(kmpp_strlen(info->name) + 1, sizeof(rk_u64));

    n = kmpp_calloc_atomic(sizeof(*n) + name_len);
    if (!n) {
        kmpp_loge_f("failed to create KmppEnvNode size %d\n", sizeof(*n));
        goto failed;
    }

    if (info->type == KmppEnv_str) {
        rk_s32 val_size = KMPP_ALIGN(kmpp_strlen(info->val) + 1, sizeof(rk_u64));
        void *val_buf = kmpp_calloc_atomic(val_size);

        if (!val_buf) {
            kmpp_loge_f("failed to create val_buf size %d\n", val_size);
            goto failed;
        }

        kmpp_strncpy(val_buf, info->val, val_size);
        n->info.val = val_buf;
        n->str_val_size = val_size;
    } else {
        n->info.val = info->val;
        n->str_val_size = 0;
    }

    /* copy env name */
    kmpp_strncpy((rk_u8 *)(n + 1), info->name, name_len);

    n->grp = grp;
    n->info.type = info->type;
    n->info.readonly = info->readonly;
    n->info.name = (const rk_u8 *)(n + 1);
    n->info.env_show = info->env_show;

    pde = proc_create_data(n->info.name, info->readonly ? 0444 : 0644, grp->dir,
                           &kmpp_env_fops, n);
    if (!pde) {
        kmpp_loge_f("failed to create procfs entry %s\n", n->info.name);
        goto failed;
    }

    n->pde = pde;

    kmpp_spin_lock(srv->lock_env);
    kmpp_list_add_tail(&n->list, &grp->list_nodes);
    kmpp_spin_unlock(srv->lock_env);

    if (node)
        *node = n;

    return rk_ok;

failed:
    if (pde)
        proc_remove(pde);

    kmpp_free(n);
    return rk_nok;
}

rk_s32 kmpp_env_del(KmppEnvGrp env, KmppEnvNode node)
{
    KmppEnvNodeImpl *p = (KmppEnvNodeImpl *)node;

    if (p) {
        KmppEnvSrv *srv = get_env_srv_f();

        if (p->grp != env) {
            kmpp_loge_f("invalid param env %px node %px node->grp\n",
                        env, p, p->grp);

            return rk_nok;
        }

        if (srv) {
            kmpp_spin_lock(srv->lock_env);
            kmpp_list_del_init(&p->list);
            kmpp_spin_unlock(srv->lock_env);
        }

        if (p->pde) {
            proc_remove(p->pde);
            p->pde = NULL;
        }

        if (p->info.type == KmppEnv_str) {
            kmpp_free(p->info.val);
        }

        kmpp_free(p);
    }

    return rk_ok;
}

void *kmpp_env_to_ptr(KmppEnvNode node)
{
    if (node) {
        KmppEnvNodeImpl *impl = (KmppEnvNodeImpl *)node;

        return impl->info.val;
    }

    return NULL;
}

void *kmpp_env_to_seq(KmppEnvNode node)
{
    if (node) {
        KmppEnvNodeImpl *impl = (KmppEnvNodeImpl *)node;

        return impl->seq;
    }

    return NULL;
}

KmppEnvGrp kmpp_env_root(void)
{
    KmppEnvSrv *srv = get_env_srv_f();

    return srv ? srv->root : NULL;
}

KmppEnvGrp kmpp_env_osal(void)
{
    KmppEnvSrv *srv = get_env_srv_f();

    return srv ? srv->osal : NULL;
}

KmppEnvGrp kmpp_env_debug(void)
{
    KmppEnvSrv *srv = get_env_srv_f();

    return srv ? srv->debug : NULL;
}

EXPORT_SYMBOL(kmpp_env_log);
EXPORT_SYMBOL(kmpp_env_get);
EXPORT_SYMBOL(kmpp_env_put);
EXPORT_SYMBOL(kmpp_env_add);
EXPORT_SYMBOL(kmpp_env_del);
EXPORT_SYMBOL(kmpp_env_to_ptr);
EXPORT_SYMBOL(kmpp_env_to_seq);

EXPORT_SYMBOL(kmpp_env_root);
EXPORT_SYMBOL(kmpp_env_osal);
EXPORT_SYMBOL(kmpp_env_debug);
