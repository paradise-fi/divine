// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <stdint.h>

#ifndef __DIVINE_STAT_H__
#define __DIVINE_STAT_H__

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#endif

EXTERN_C

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_mode;
    uint64_t st_nlink;
    uint64_t st_uid;
    uint64_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;

    uint64_t st_atim_tv_sec;
    uint32_t st_atim_tv_nsec;

    uint64_t st_mtim_tv_sec;
    uint32_t st_mtim_tv_nsec;

    uint64_t st_ctim_tv_sec;
    uint32_t st_ctim_tv_nsec;
} _DivineStat;

CPP_END

#undef EXTERN_C
#undef CPP_END

#endif // __DIVINE_STAT_H__
