/* Diagnostics <assert.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include "_PDCLIB_aux.h"
#include "_PDCLIB_config.h"

#include <sys/cdefs.h>

#undef assert
#undef _assert

#ifdef NDEBUG
# define	assert(e)	((void)0)
# define	_assert(e)	((void)0)
#else
# define	_assert(e)	assert(e)
# define	assert(e)	((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __func__))
#endif

#ifndef _ASSERT_H_
#define _ASSERT_H_
__BEGIN_DECLS
void __assert_fail( const char *__assertion, const char *__file, unsigned int __line, const char *__function )
    _PDCLIB_nothrow;
__END_DECLS
#endif
