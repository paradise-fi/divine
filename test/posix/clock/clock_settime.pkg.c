/* TAGS: c */
/* VERIFY_OPTS: */
#include <time.h>
#include <assert.h>

// V: fixed  V_OPT: -o clock-type:fixed  TAGS: min
// V: det    V_OPT: -o clock-type:det
// V: ndet   V_OPT: -o clock-type:ndet
// V: sym    V_OPT: -o clock-type:sym    --symbolic TAGS: todo

int main()
{
    struct timespec t1, t2;
    assert( clock_gettime( CLOCK_REALTIME, &t1 ) == 0 );

    t1.tv_sec += 10;
    assert( clock_settime( CLOCK_MONOTONIC, &t1 ) < 0 );
    assert( clock_settime( CLOCK_REALTIME, &t1 ) == 0 );
    assert( clock_gettime( CLOCK_REALTIME, &t2 ) == 0 );
    assert( t2.tv_sec >= t1.tv_sec );

    t1.tv_sec -= 10;
    assert( clock_settime( CLOCK_REALTIME, &t1 ) == 0 );
    assert( clock_gettime( CLOCK_REALTIME, &t2 ) == 0 );
    assert( t2.tv_sec >= t1.tv_sec );
}
