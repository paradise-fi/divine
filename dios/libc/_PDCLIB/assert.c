/* _PDCLIB_assert( char const * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/trace.h>
#include <sys/fault.h>

#ifndef REGTEST

#include <_PDCLIB/cdefs.h>

void __assert_fail( const char *stmt, const char *file, unsigned line, const char *fun )
{
    int masked = __dios_mask( 1 );
    char buffer[ 200 ];
    snprintf( buffer, 200, "%s:%u: %s: assertion '%s' failed", file, line, fun, stmt );
    __dios_fault( _VM_F_Assert, buffer );
    __dios_mask( masked );
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"
#include <signal.h>

static int EXPECTED_ABORT = 0;
static int UNEXPECTED_ABORT = 1;

static void aborthandler( int sig )
{
    TESTCASE( ! EXPECTED_ABORT );
    exit( (signed int)TEST_RESULTS );
}

#define NDEBUG
#include <assert.h>

static int disabled_test( void )
{
    int i = 0;
    assert( i == 0 ); /* NDEBUG set, condition met */
    assert( i == 1 ); /* NDEBUG set, condition fails */
    return i;
}

#undef NDEBUG
#include <assert.h>

int main( void )
{
    TESTCASE( signal( SIGABRT, &aborthandler ) != SIG_ERR );
    TESTCASE( disabled_test() == 0 );
    assert( UNEXPECTED_ABORT ); /* NDEBUG not set, condition met */
    assert( EXPECTED_ABORT ); /* NDEBUG not set, condition fails - should abort */
    return TEST_RESULTS;
}

#endif
