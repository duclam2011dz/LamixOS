/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_M26_GPT_H
#define LAMIX_KERNEL_M26_GPT_H
#include "block.h"
#include "vfs.h"
vfs_status m26_gpt_user_extent(block_device *, uint64_t *, uint32_t *);
#endif
