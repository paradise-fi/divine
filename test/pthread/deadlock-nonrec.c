/* TAGS: min threads c */
#include <pthread.h>

int main()
{
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &b );
    pthread_mutex_lock( &b ); /* ERROR: deadlock */

    return 0;
}
