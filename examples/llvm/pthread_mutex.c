#include "divine-llvm.h"

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *other(void *x) {
    pthread_mutex_t *mutex = (pthread_mutex_t*) x;
    pthread_mutex_lock( mutex );
    critical();
    pthread_mutex_unlock( mutex );
    return NULL;
}

int main() {
    pthread_t id;
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;

    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

    pthread_mutex_init( &mutex, &attr );

    void *result;
    pthread_create( &id, NULL, other, &mutex );

    pthread_mutex_lock( &mutex );

    pthread_mutex_lock( &mutex );
    pthread_mutex_unlock( &mutex );

    critical();

    pthread_mutex_unlock( &mutex );

    trace( "waiting for %d", id );
    pthread_join( id, &result );
    return 0;
}
