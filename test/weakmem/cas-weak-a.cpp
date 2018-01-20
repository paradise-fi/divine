/* TAGS: min c++ */
/* VERIFY_OPTS: --relaxed-memory pso */

#include <cassert>
#include <atomic>

int main() {
    std::atomic< int > x{ 0 };
    int c = 0;
    bool r = x.compare_exchange_strong( c, 1 );
    assert( r );
    assert( x == 1 );
    c = 1;
    r = x.compare_exchange_weak( c, 2 );
    assert( (r && x == 2) || (!r && x == 1) );
    assert( r ); /* ERROR */
}
