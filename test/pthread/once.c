#include <pthread.h>
#include <assert.h>

int x = 0;
pthread_once_t once = PTHREAD_ONCE_INIT;

void callee( void ) {
    ++x;
}

void init( void ) {
    pthread_once( &once, callee );
}

void *thread( void *_ ) {
    init();
    return 0;
}

int main() {
    pthread_t t;
    pthread_create( &t, 0, thread, 0 );
    init();
    pthread_join( t, 0 );
    assert( x == 1 );
}
