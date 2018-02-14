/* _PDCLIB_assert( char const * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifndef REGTEST

#include "_PDCLIB_aux.h"

void __assert_fail( const char *__assertion, const char *__file, unsigned int __line, const char *__function )
{
    __dios_trace_f( "Assertion failed: %s, file %s, line %u.", __assertion, __file, __line );
    __dios_fault( _VM_F_Assert, NULL );
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
