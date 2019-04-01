/* TAGS: ext c++ tso */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< int > x, y;

template< int v >
void *worker( void * ) {
    auto old = x.fetch_add( v, std::memory_order_relaxed );
    assert( old <= 8 - (2 * v) );
    old = x.fetch_add( v, std::memory_order_relaxed );
    assert( old <= 8 - v );
    return nullptr;
}

int main() {
    pthread_t t1, t2;
    pthread_create( &t1, nullptr, &worker< 1 >, nullptr );
    pthread_create( &t2, nullptr, &worker< 3 >, nullptr );
    assert( x <= 8 );
    pthread_join( t1, nullptr );
    pthread_join( t2, nullptr );
    assert( x == 8 );
    return 0;
}
