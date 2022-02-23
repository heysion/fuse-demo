/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     heysion@deepin.com
 *
 * Maintainer: heysion@deepin.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 30
#endif

#include <iostream>
#include <fuse.h>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

static const std::string prefix = "/opt/rootfs/";

struct proxy_dirp {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

inline std::string get_fix_real_path(const char *path)
{
    if (path) {
        return prefix + path;
    }
    return prefix;
}

static void *fuse_register_demo_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    cfg->kernel_cache = 1;
    conn->want |= FUSE_CAP_SPLICE_READ;
    conn->want |= FUSE_CAP_SPLICE_WRITE;
    conn->want |= FUSE_CAP_SPLICE_MOVE;
    conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
    return NULL;
}

static int fuse_register_demo_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    auto res = 0;

    if (fi != NULL) {
        res = fstat((int)fi->fh, stbuf);
    } else {
        auto real_path = prefix + path;
        res = lstat(real_path.c_str(), stbuf);
    }
    if (res == -1)
        return -errno;

    return res;
}

static int fuse_register_demo_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    auto fd = -1;
    auto res = -1;

    auto real_path = get_fix_real_path(path);

    if (fi == NULL)
        fd = open(real_path.c_str(), O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close(fd);
    return res;
}

static int fuse_register_demo_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                                      struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    //    DIR *dp;
    struct dirent *de;
    auto real_path = get_fix_real_path(path);
    auto dp = opendir(real_path.c_str());
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st {
            0,
        };
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, FUSE_FILL_DIR_PLUS))
            break;
    }

    closedir(dp);
    return 0;
    return 0;
}

static int fuse_register_demo_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    auto res = -1;
    auto real_path = get_fix_real_path(path);
    auto fd = open(real_path.c_str(), fi->flags, mode);

    if (fd == -1) {
        return -errno;
    } else {
        res = lchown(real_path.c_str(), fuse_get_context()->uid, fuse_get_context()->gid);
        if (res == -1)
            return -errno;
    }
    fi->fh = (unsigned long)fd;
    return 0;
}

static int fuse_register_demo_write(const char *path, const char *buf, size_t size, off_t offset,
                                    struct fuse_file_info *fi)
{
    if (fi == NULL) {
        if (path) {
            auto real_path = get_fix_real_path(path);
            auto newfd = open(real_path.c_str(), fi->flags, 0);
            if (newfd == -1)
                return -errno;
            auto res = pwrite(newfd, buf, size, offset);
            if (res == -1) {
                close(newfd);
                return -errno;
            }
            fi->fh = (unsigned long)newfd;
            return 0;
        }
        return -ENOENT;
    }

    auto fd = (unsigned long)fi->fh;

    auto res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        if (path) {
            auto real_path = get_fix_real_path(path);
            auto newfd = open(real_path.c_str(), fi->flags);
            if (newfd == -1)
                return -errno;
            res = 0;
            errno = 0;
            res = pwrite(newfd, buf, size, offset);
            if (res == -1) {
                close(newfd);
                return -errno;
            } else {
                close(fd);
                fi->fh = (unsigned long)newfd;
                return 0;
            }
        }
        return -errno;
    }
    return res;
}

static int fuse_register_demo_access(const char *path, int mask)
{
    int res;

    std::string real_path = get_fix_real_path(path);
    res = access(real_path.c_str(), mask);

    if (res == -1)
        return -errno;

    return 0;
}

static int fuse_register_demo_open(const char *path, struct fuse_file_info *fi)
{
    auto real_path = get_fix_real_path(path);

    auto fd = open(real_path.c_str(), fi->flags);

    if (fd == -1) {
//        perror("open file failed!");
        return -errno;
    }

    fi->fh = (unsigned long)fd;

    return 0;
}

static int fuse_register_demo_opendir(const char *path, struct fuse_file_info *fi)
{
    auto res = -1;

    struct proxy_dirp *d = (proxy_dirp *)malloc(sizeof(struct proxy_dirp));
    if (d == NULL)
        return -ENOMEM;

    auto real_path = get_fix_real_path(path);

    d->dp = opendir(real_path.c_str());

    if (d->dp == NULL) {
        res = -errno;
        free(d);
        return res;
    }
    d->offset = 0;
    d->entry = NULL;

    fi->fh = (unsigned long)d;
    return 0;
}

static int fuse_register_demo_release(const char *path, struct fuse_file_info *fi)
{
    if (fi && fi->fh > 0) {
        close((unsigned long)fi->fh);
        fi->fh = 0;
        return 0;
    }
    return 0;
}

static int fuse_register_demo_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct proxy_dirp *d = (proxy_dirp *)(uintptr_t)fi->fh;
    closedir(d->dp);
    free(d);
    return 0;
}

static int fuse_register_demo_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    auto res = -1;

    if (isdatasync)
        res = fdatasync((int)fi->fh);
    else
        res = fsync((int)fi->fh);
    if (res == -1)
        return -errno;
    return 0;
}

static int fuse_register_demo_getxattr(const char *path, const char *name, char *value, size_t size)
{
    auto real_path = get_fix_real_path(path);

    lgetxattr(real_path.c_str(), name, value, size);

    return 0;
}

static int fuse_register_demo_listxattr(const char *path, char *list, size_t size)
{
    auto real_path = get_fix_real_path(path);
    llistxattr(real_path.c_str(), list, size);
    return 0;
}

static int fuse_register_demo_statfs(const char *path, struct statvfs *stbuf)
{
    auto res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}

static int fuse_register_demo_readlink(const char *filename, char *buf, size_t size)
{
    auto real_path = get_fix_real_path(filename);

    auto r = readlink(real_path.c_str(), buf, size - 1);

    if (r == -1)
        return -errno;
    buf[r] = '\0';
    return 0;
}

static int fuse_register_demo_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset,
                                       struct fuse_file_info *fi)
{
    struct fuse_bufvec *src;

    src = (fuse_bufvec *)malloc(sizeof(struct fuse_bufvec));
    if (src == NULL)
        return -ENOMEM;

    *src = FUSE_BUFVEC_INIT(size);

    src->buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    src->buf[0].fd = fi->fh;
    src->buf[0].pos = offset;
    *bufp = src;

    return 0;
}

static int fuse_register_demo_write_buf(const char *path, struct fuse_bufvec *buf, off_t offset,
                                        struct fuse_file_info *fi)
{
    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    dst.buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dst.buf[0].fd = fi->fh;
    dst.buf[0].pos = offset;

    auto ret = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
    return ret;
}

static int fuse_register_demo_link(const char *from, const char *to)
{
    auto real_from = get_fix_real_path(from);
    auto real_to = get_fix_real_path(to);
    auto res = link(real_from.c_str(), real_to.c_str());
    if (res == -1)
        return -errno;

    return 0;
}

static int fuse_register_demo_unlink(const char *path)
{
    auto real_path = get_fix_real_path(path);
    auto res = unlink(real_path.c_str());
    if (res == -1)
        return -errno;
    return 0;
}

static int fuse_register_demo_fallocate(const char *path, int mode, off_t offset, off_t length,
                                        struct fuse_file_info *fi)
{
    if (mode)
        return -EOPNOTSUPP;

    auto res = posix_fallocate(fi->fh, offset, length);
    if (res == -1)
        return -errno;
    return 0;
}

static int fuse_register_demo_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int res;

    if (fi) {
        res = fchmod(fi->fh, mode);
    } else {
        auto real_path = get_fix_real_path(path);
        res = chmod(real_path.c_str(), mode);
    }

    if (res == -1)
        return -errno;

    return 0;
}

static int fuse_register_demo_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    int res;

    if (fi) {
        res = fchown(fi->fh, uid, gid);
    } else {
        auto real_path = get_fix_real_path(path);
        res = chown(real_path.c_str(), uid, gid);
    }

    if (res == -1)
        return -errno;

    return 0;
}

static int fuse_register_demo_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    int res;

    if (fi) {
        res = ftruncate(fi->fh, size);
    } else {
        auto real_path = get_fix_real_path(path);
        res = truncate(real_path.c_str(), size);
    }

    if (res == -1)
        return -errno;

    return 0;
}

static const struct fuse_operations fuse_register_demo_hook = {
    .getattr = fuse_register_demo_getattr,
    .readlink = fuse_register_demo_readlink,
    .unlink = fuse_register_demo_unlink,
    .link = fuse_register_demo_link,
    .chmod = fuse_register_demo_chmod,
    .chown = fuse_register_demo_chown,
    .truncate = fuse_register_demo_truncate,
    .open = fuse_register_demo_open,
    .read = fuse_register_demo_read,
    .write = fuse_register_demo_write,
    .statfs = fuse_register_demo_statfs,
    .release = fuse_register_demo_release,
    .fsync = fuse_register_demo_fsync,
    .getxattr = fuse_register_demo_getxattr,
    .listxattr = fuse_register_demo_listxattr,
    .opendir = fuse_register_demo_opendir,
    .readdir = fuse_register_demo_readdir,
    .releasedir = fuse_register_demo_releasedir,
//    .init = fuse_register_demo_init,
    .access = fuse_register_demo_access,
    .create = fuse_register_demo_create,
//    .write_buf = fuse_register_demo_write_buf,
//    .read_buf = fuse_register_demo_read_buf,
    .fallocate = fuse_register_demo_fallocate,
};

int main(int argc, char *argv[])
{
    auto ret = fuse_main(argc, argv, &fuse_register_demo_hook, NULL);
    return ret;
}
