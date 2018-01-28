/* TAGS: todo ext c++ tso */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< bool > flag[2];
std::atomic< int > turn;

constexpr int other( int x ) { return x == 0 ? 1 : 0; }

std::atomic< int > critical;

template< int tid >
void *worker( void * ) {
    flag[ tid ] = true;
    turn = other( tid );
    while ( flag[ other( tid ) ] && turn == other( tid ) ) { }
    // critical start
    critical.fetch_add( 1, std::memory_order_relaxed );
    critical.fetch_sub( 1, std::memory_order_relaxed );
    // end
    flag[ tid ] = false;
    return nullptr;
}

int main() {
    pthread_t t1, t2;
    pthread_create( &t1, nullptr, &worker< 0 >, nullptr );
    pthread_create( &t2, nullptr, &worker< 1 >, nullptr );
    while ( true )
        assert( critical <= 1 );
    pthread_join( t1, nullptr );
    pthread_join( t2, nullptr );
    return 0;
}
