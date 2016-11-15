#include <pthread.h>
#include <assert.h>

void *thread_func(void *t)
{
    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
    return NULL;
}

int main(void)
{
    pthread_t thr;
    pthread_create( &thr, NULL, &thread_func, NULL );
    pthread_cancel( thr );

    void *res;
    pthread_join( thr, &res );
    if (res == PTHREAD_CANCELED)
        assert( 0 ); /* ERROR */
}
