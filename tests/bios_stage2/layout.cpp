// SPDX-License-Identifier: GPL-3.0-only
#include "lamix/boot/loader.h"
static_assert(sizeof(lbp_header) == 64);
static_assert(sizeof(lamix_elf_segment) == 40);
static_assert(sizeof(lamix_elf_plan) == 1304);
