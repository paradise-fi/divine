/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic --lart stubs */
/* CC_OPTS: */

#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

void foo( int val ) {
    assert( val < 0 );
}

extern void vbari( int val );

int main() {
    int val = __lamp_any_i32();
    auto fn = val < 0 ? &foo : &vbari;
    fn( val ); /* ERROR */
}
