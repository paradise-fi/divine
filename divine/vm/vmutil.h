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
#define _VMUTIL_INLINE extern inline
#define _VMUTIL_INLINE_UNDEF
#endif


#ifdef __cplusplus
extern "C" {
#define _VMUTIL_CAST( T, X ) reinterpret_cast< T >( X )
#else
#define _VMUTIL_CAST( T, X ) ((T)(X))
#endif

#if defined( __divine__ )

_VMUTIL_INLINE int __vmutil_mask( int set ) {
    return ( _VMUTIL_CAST( uint64_t,
                __vm_control( _VM_CA_Get, _VM_CR_Flags,
                              _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask,
                                                        set ? _VM_CF_Mask : _VM_CF_None ) )
              & _VM_CF_Mask) != 0;
}

/* Performs forceful interrupt (even if under mask) and resets interrupted
 * flag to 0.
 *  It should NOT be used to invoke syscall as the syscall is a visible action.
 */
_VMUTIL_INLINE void __vmutil_interrupt()
{
    uint64_t fl = _VMUTIL_CAST( uint64_t,
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted,
                  fl & ~_VM_CF_Interrupted );
}

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
    }
    __builtin_unreachable();
}

static const char *__vmutil_flag_name( uint64_t val )
{
    switch ( val )
    {
        DIVM_FLAG_NAME( _VM_CF_Mask );
        DIVM_FLAG_NAME( _VM_CF_Interrupted );
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
