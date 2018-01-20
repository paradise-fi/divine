/* TAGS: min threads c */
#include <pthread.h>

volatile int i = 0;

void *thr( void *mtx ) {
    pthread_mutex_t *mutex1 = ( (pthread_mutex_t **)mtx )[ 0 ];
    pthread_mutex_t *mutex2 = ( (pthread_mutex_t **)mtx )[ 1 ];
    pthread_mutex_lock( mutex1 );
    pthread_mutex_lock( mutex2 ); /* ERROR */
    i = (i + 1) % 3;
    pthread_mutex_unlock( mutex2 );
    pthread_mutex_unlock( mutex1 );
    return 0;
}

int main() {
    pthread_mutex_t a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *ab[] = { &a, &b };
    pthread_mutex_t *ba[] = { &b, &a };

    pthread_t ta;

    pthread_create( &ta, 0, thr, &ab );
    thr( ba );
    pthread_join( ta, 0 );

    return 0;
}
