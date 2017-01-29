#include <pthread.h>
#include <assert.h>

volatile int i = 1, j = 1;

void *thread( void* arg )
{
    i += j;
    i += j;
    return 0;
}

int main()
{
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );

    j += i;
    j += i;

    assert( j < 8 ); /* ERROR */

    return 0;
}
