/* TAGS: min threads c */
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

__attribute__((constructor)) void foo( void ) { init(); }
__attribute__((constructor)) void bar( void ) { init(); }

int main() {
    assert( x == 1 );
}
