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

#include <fuse.h>

static const struct fuse_operations fuse_register_demo_hook = {
};

int main(int argc, char *argv[]) {
    auto ret = fuse_main(argc, argv, &fuse_register_demo_hook, NULL);
    return ret;
}
