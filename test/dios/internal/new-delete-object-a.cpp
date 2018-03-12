/* TAGS: min c++ */
/* CC_OPTS: -std=c++11 */
#include <dios/sys/stdlibwrap.hpp>
#include <cassert>

volatile int done;

struct Item {
    int val;

    Item( int val = 42 ) : val( val ) { }
    ~Item() { done++; }
};

int main() {
    done = 0;

    auto a = __dios::new_object< Item >( );
    assert( a );
    assert( a->val == 42 );

    auto b = __dios::new_object< Item >( 1 );
    assert( b );
    assert( b->val == 1 );

    __dios::delete_object( a );
    assert( done == 1 );
    __dios::delete_object( b );
    assert( done == 2 );

    if ( __vm_choose( 2 ) )  a->val = 8; else  b->val = 8; /* ERROR */

    return 0;
}
