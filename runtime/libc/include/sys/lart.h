/*
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
 *
 * LART flags for use in _VM_CR_Flags
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

#ifndef _SYS_LART_H_
#define _SYS_LART_H_

#include <sys/divm.h>

#ifdef __cplusplus
extern "C" {
#endif

// there are 16 flags reserved for transformations, see <sys/divm.h>
static const uint64_t _LART_CF_RelaxedMemRuntime = _VM_CFB_Transform << 0;

#ifdef __cplusplus
}
#endif

#endif // _SYS_LART_H_
