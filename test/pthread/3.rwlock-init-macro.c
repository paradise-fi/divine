#include <pthread.h>
#include <assert.h>
#include <errno.h>

int main() {
    pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
    int r = pthread_rwlock_init( &rwlock, NULL );
    assert( r == EBUSY );
}
