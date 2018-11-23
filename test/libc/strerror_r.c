/* TAGS: min c */
#include <string.h>
#include <assert.h>

int main()
{
    char buf[25];
    int ret = strerror_r( 3, buf, 25 );
    assert( ret == 0 );
    assert( strcmp( buf, "ESRCH (no such process)" ) == 0 );

    int ret_einval = strerror_r( 900, buf, 25 );
    assert( ret_einval == 22 );
    assert( strcmp( buf, "Unknown error" ) == 0 );

    char smallbuf[10];
    int ret_erange = strerror_r( 2, smallbuf, 10 );
    assert( ret_erange == 34 );
    assert( strcmp( smallbuf, "ENOENT (n" ) == 0 );
}
