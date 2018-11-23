/* TAGS: c */
#define _GNU_SOURCE
#include <string.h>
#include <assert.h>

int main()
{
    char buf[25];
    char *ret = strerror_r( 3, buf, 25 );
    assert( strcmp( ret, "ESRCH (no such process)" ) == 0 );

    char *ret_einval = strerror_r( 900, buf, 25 );
    assert( strcmp( ret_einval, "Unknown error" ) == 0 );

    char smallbuf[10];
    char *ret_erange = strerror_r( 2, smallbuf, 10 );
    assert( strcmp( ret_erange, "ENOENT (n" ) == 0 );
}
