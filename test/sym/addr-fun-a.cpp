/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

void foo( int val ) {
    assert( val < 0 );
}

void bar( int val ) {
    assert( val >= 0 );
}

int main() {
    int val = __lamp_any_i32();
    auto fn = val < 0 ? &foo : &bar;
    fn( val );
}
