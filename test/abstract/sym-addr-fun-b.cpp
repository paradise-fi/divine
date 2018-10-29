/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic --lart stubs */
/* CC_OPTS: */

#include <rst/domains.h>

#include <cstdint>
#include <cassert>

void foo( int val ) {
    assert( val < 0 );
}

extern void bar( int val );

int main() {
    int val = __sym_val_i32();
    auto fn = val < 0 ? &foo : &bar;
    fn( val ); /* ERROR */
}
