#include <pthread.h>
#include <cassert>

int x, y;

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, []( void * ) -> void * {
            assert( x == 0 || y == 1 ); /* ERROR */
            return nullptr;
        }, nullptr );
    x = 1;
    __dios_kill_process( 0 );
    return 0;
}
