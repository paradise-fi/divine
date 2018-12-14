/* TAGS: c */
/* VERIFY_OPTS: */
#include <sys/time.h>
#include <stdlib.h>
#include <assert.h>

// V: fixed  V_OPT: -o clock-type:fixed  TAGS: min
// V: det    V_OPT: -o clock-type:det
// V: ndet   V_OPT: -o clock-type:ndet
// V: sym    V_OPT: -o clock-type:sym    --symbolic TAGS: todo

int main()
{
    struct timeval t1, t2;
    assert( gettimeofday( &t1, NULL ) == 0 );

    t1.tv_sec += 10;
    assert( settimeofday( &t1, NULL ) == 0 );
    assert( gettimeofday( &t2, NULL ) == 0 );
    assert( t2.tv_sec >= t1.tv_sec );

    t1.tv_sec -= 10;
    assert( settimeofday( &t1, NULL ) == 0 );
    assert( gettimeofday( &t2, NULL ) == 0 );
    assert( t2.tv_sec >= t1.tv_sec );
}
