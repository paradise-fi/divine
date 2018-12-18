/* TAGS: min threads c */
#include <pthread.h>

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t ch;

/* Thread function */
void * threaded( void * arg )
{
    pthread_mutex_lock( &mtx );
    pthread_mutex_unlock( &mtx );
    return 0;
}

int main( int argc, char * argv[] )
{
    pthread_mutex_lock( &mtx );
    pthread_create( &ch, NULL, threaded, NULL );
    return 0;
}
