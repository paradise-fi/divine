/* TAGS: min c++ */
#include <atomic>
#include <cassert>

int main() {
    std::atomic< int > a = ATOMIC_VAR_INIT( 0 );
    assert( a == 0 );
    a = 42;
    assert( a == 42 );
    assert( a.exchange( 16 ) == 42 );
    assert( a == 16 );
    int compare = 8;
    assert( !a.compare_exchange_strong( compare, 0 ) );
    assert( a.compare_exchange_strong( compare, 0 ) );
    assert( a == 0 );
}
