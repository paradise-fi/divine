/*
 * The name '__errno_location' is for glibc compatibility.
*/

#include <errno.h>
#include <dios.h>
#ifndef REGTEST
#include <threads.h>

void *__errno_location()
{
    return __dios_get_errno();
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    errno = 0;
    TESTCASE( errno == 0 );
    errno = EDOM;
    TESTCASE( errno == EDOM );
    errno = ERANGE;
    TESTCASE( errno == ERANGE );
    return TEST_RESULTS;
}

#endif
