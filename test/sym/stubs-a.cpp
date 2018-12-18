/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic --lart stubs */
/* CC_OPTS: */

#include <rst/domains.h>

#include <cstdint>
#include <cassert>

extern int stubbed( int val );

int main() {
    int val = __sym_val_i32();
    stubbed( val ); /* ERROR */
}
