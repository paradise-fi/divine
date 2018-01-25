/* TAGS: threads c++ */
#include <pthread.h>
#include <signal.h>
#include <cassert>

int x, y;

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, []( void * ) -> void * {
            assert( x == 0 || y == 1 ); /* ERROR */
            return nullptr;
        }, nullptr );
    x = 1;
    raise( SIGKILL );
    return 0;
}
