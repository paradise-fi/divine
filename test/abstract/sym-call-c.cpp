/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int id( int a ) { return a; }

int plus( int a, int b ) { return id( a ) + id( b ); }

int main() {
    int input = __sym_val_i32();
    assert( plus( input, 10 ) == input + 10 );
}
