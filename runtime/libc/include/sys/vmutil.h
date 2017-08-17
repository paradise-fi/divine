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

#include <sys/divm.h>
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

_VMUTIL_INLINE int __vmutil_mask( int set ) {
    return ( _VMUTIL_CAST( uint64_t,
                __vm_control( _VM_CA_Get, _VM_CR_Flags,
                              _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask,
                                                        set ? _VM_CF_Mask : _VM_CF_None ) )
              & _VM_CF_Mask) != 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE
#endif

#endif // _SYS_VMUTIL_H_
