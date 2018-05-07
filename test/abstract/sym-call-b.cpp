/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>


int plus( int a, int b ) { return a + b; }

int main() {
    int a = __sym_val_i32();
    int b = __sym_val_i32();
    assert( plus( a, 10 ) == a + 10 );
    assert( plus( 10, b ) == 10 + b );
    assert( plus( a, b ) == a + b );
}
