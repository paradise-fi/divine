/* TAGS: threads c++ */
#include <stdlib.h>
#include <pthread.h>
#include <cassert>
#include <cstddef>

int x, y;

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, []( void * ) -> void * {
            assert( x == 0 || y == 1 ); /* ERROR */
            return nullptr;
        }, nullptr );
    x = 1;
    exit( 0 );
}
