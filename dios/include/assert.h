/* Diagnostics <assert.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include "_PDCLIB/cdefs.h"
#include "_PDCLIB/config.h"

#include <sys/cdefs.h>

#undef assert
#undef _assert

#ifdef NDEBUG
# define assert(e)  ((void)0)
# define _assert(e) ((void)0)
#else
# define _assert(e) assert(e)
# define assert(e)  ( (e) ? (void) 0 : __assert_fail(#e, __FILE__, __LINE__, __PRETTY_FUNCTION__) )
#endif

#ifndef _ASSERT_H_
#define _ASSERT_H_
__BEGIN_DECLS
void __assert_fail( const char *, const char *, unsigned, const char * ) __nothrow;
__END_DECLS
#endif
