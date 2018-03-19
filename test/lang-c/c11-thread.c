/* TAGS: min threads c c11 */
/* CC_OPTS: -std=c11 */
#include <assert.h>
#include <threads.h>

int shared = 0;

int thread( void *x )
{
    ++shared;
    return 42;
}

int main()
{
    thrd_t tid;
    thrd_create( &tid, thread, NULL );
    ++shared;
    int res;
    thrd_join( tid, &res );
    assert( res == 42 );
    assert( shared == 2 ); /* ERROR */
    return 0;
}

