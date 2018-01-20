/* TAGS: min threads c */
#include <pthread.h>

void *thr( void *mtx ) {
    pthread_mutex_lock( mtx );
    return 0; /* thread terminates without unlocking */
}

int main() {
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_t t;
    pthread_create( &t, 0, thr, &b );
    pthread_mutex_lock( &b ); /* ERROR: deadlock */
    pthread_join( t, 0 );

    return 0;
}
