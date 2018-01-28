/* TAGS: c++ weakmem pso */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory pso:5 */

#include <atomic>
#include <pthread.h>
#include <cassert>

// V: static-init CC_OPT: -DSTATIC_INIT=1
// V: dynamic-init CC_OPT: -DSTATIC_INIT=0

#ifndef STATIC_INIT
#define STATIC_INIT 1
#endif

std::atomic< int > x, y;
int rx, ry;
#if STATIC_INIT
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
#else
pthread_mutex_t mtx;
#endif

int main() {
    __sync_synchronize();
#if !STATIC_INIT
    pthread_mutex_init( &mtx, nullptr );
#endif
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
