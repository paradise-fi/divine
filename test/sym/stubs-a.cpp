/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic --lart stubs */
/* CC_OPTS: */

#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

extern int stubbed( int val );

int main() {
    int val = __lamp_any_i32();
    stubbed( val ); /* ERROR */
}
