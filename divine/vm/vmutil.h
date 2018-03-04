/*
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
 *
 * Utility functions for DiVM
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __divine__
#include <sys/divm.h>
#include <sys/lart.h>
#else
#include <divine/vm/divm.h>
#include <divine/vm/lart.h>
#endif

#include <stdint.h>

#ifndef _SYS_VMUTIL_H_
#define _SYS_VMUTIL_H_

#ifndef _VMUTIL_INLINE
#define _VMUTIL_INLINE static inline
#define _VMUTIL_INLINE_UNDEF
#endif


#ifdef __cplusplus
extern "C" {
#define _VMUTIL_CAST( T, X ) reinterpret_cast< T >( X )
#else
#define _VMUTIL_CAST( T, X ) ((T)(X))
#endif

#define DIVM_REG_NAME( X )  case X: return #X + 7
#define DIVM_FLAG_NAME( X ) case X: return #X;

static const char *__vmutil_reg_name( int id )
{
    switch ( id )
    {
        DIVM_REG_NAME( _VM_CR_Constants );
        DIVM_REG_NAME( _VM_CR_Globals );
        DIVM_REG_NAME( _VM_CR_Frame );
        DIVM_REG_NAME( _VM_CR_PC );

        DIVM_REG_NAME( _VM_CR_Scheduler );
        DIVM_REG_NAME( _VM_CR_State );
        DIVM_REG_NAME( _VM_CR_IntFrame );
        DIVM_REG_NAME( _VM_CR_Flags );

        DIVM_REG_NAME( _VM_CR_FaultHandler );
        DIVM_REG_NAME( _VM_CR_ObjIdShuffle );
        DIVM_REG_NAME( _VM_CR_User1 );
        DIVM_REG_NAME( _VM_CR_User2 );
        DIVM_REG_NAME( _VM_CR_User3 );
        DIVM_REG_NAME( _VM_CR_User4 );
    }
    __builtin_unreachable();
}

static const char *__vmutil_flag_name( uint64_t val )
{
    switch ( val )
    {
        DIVM_FLAG_NAME( _VM_CF_IgnoreLoop );
        DIVM_FLAG_NAME( _VM_CF_IgnoreCrit );
        DIVM_FLAG_NAME( _VM_CF_Accepting );
        DIVM_FLAG_NAME( _VM_CF_Error );
        DIVM_FLAG_NAME( _VM_CF_Cancel );
        DIVM_FLAG_NAME( _VM_CF_KernelMode );
        DIVM_FLAG_NAME( _VM_CF_DebugMode );

        DIVM_FLAG_NAME( _LART_CF_RelaxedMemRuntime );
    }
    return 0;
}

typedef struct
{
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
} __vmutil_stat;

#ifdef __cplusplus
} // extern "C"
#endif

#undef DIVM_REG_NAME
#undef DIVM_FLAG_NAME

#ifdef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE
#endif

#endif // _SYS_VMUTIL_H_
