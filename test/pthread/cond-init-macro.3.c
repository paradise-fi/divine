#include <pthread.h>
#include <assert.h>
#include <errno.h>

int main() {
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    int r = pthread_cond_init( &cond, NULL );
    assert( r == EBUSY );
}
