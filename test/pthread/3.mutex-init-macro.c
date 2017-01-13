#include <pthread.h>
#include <assert.h>
#include <errno.h>

int main() {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int r = pthread_mutex_init( &mutex, NULL );
    assert( r == EBUSY );
}
