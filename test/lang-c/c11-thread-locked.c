/* TAGS: min threads c c11 */
/* CC_OPTS: -std=c11 */
#include <assert.h>
#include <threads.h>

int shared = 0;
mtx_t mutex;

int thread( void *x )
{
    mtx_lock( &mutex );
    ++shared;
    mtx_unlock( &mutex );
    return 42;
}

int main()
{
    thrd_t tid;
    mtx_init( &mutex, mtx_plain );
    thrd_create( &tid, thread, NULL );
    mtx_lock( &mutex );
    ++shared;
    mtx_unlock( &mutex );
    int res;
    thrd_join( tid, &res );
    assert( res == 42 );
    assert( shared == 2 );
    return 0;
}

