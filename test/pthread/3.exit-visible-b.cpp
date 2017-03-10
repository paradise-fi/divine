#include <stdlib.h>
#include <pthread.h>
#include <cassert>
#include <cstddef>

int x, y;

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, []( void * ) -> void * {
            x = 1;
            exit( 0 );
        }, nullptr );
    assert( x == 0 || y == 1 ); /* ERROR */
    return 0;
}
