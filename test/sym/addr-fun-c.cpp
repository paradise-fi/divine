/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic --lart stubs */
/* CC_OPTS: */

#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

int foo( int val ) {
    return val;
}

extern int ibari( int val );

int main() {
    int val = __lamp_any_i32();
    if ( val < 0 ) {
        auto fn = val < 0 ? &foo : &ibari;
        int ret = fn( val );
        assert( val < 0 );
    }
}
