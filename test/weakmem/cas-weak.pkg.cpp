/* TAGS: min c++ tso ext */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

#include <cassert>
#include <atomic>

// V: cas-fail-reachable CC_OPT: -DWRAP=id
// V: cas-success-reachable CC_OPT: -DWRAP=not

inline bool id( bool v ) { return v; }

int main() {
    std::atomic< int > x{ 0 };
    int c = 0;
    bool r = x.compare_exchange_strong( c, 1 );
    assert( r );
    assert( x == 1 );
    c = 1;
    r = x.compare_exchange_weak( c, 2 );
    assert( (r && x == 2) || (!r && x == 1) );
    assert( WRAP( r ) ); /* ERROR */
}
