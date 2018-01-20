/* TAGS: c++ */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory pso:5 */

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< int > x, y;
int rx, ry;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

int main() {
    __sync_synchronize();
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            pthread_mutex_lock( &mtx );
            x.store( 1, std::memory_order_relaxed );
            y.store( 1, std::memory_order_relaxed );
            pthread_mutex_unlock( &mtx );
            return nullptr;
        }, nullptr );
    pthread_mutex_lock( &mtx );
    ry = y.load( std::memory_order_relaxed );
    rx = x.load( std::memory_order_relaxed );
    pthread_mutex_unlock( &mtx );
    pthread_join( t, nullptr );
    assert( ry != 1 || rx == 1 );
}


