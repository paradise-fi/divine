/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <rst/domains.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

int main()
{
    int x = __sym_val_i32();
    auto y = abs( x );
    if( x > INT_MIN )   // abs( INT_MIN ) == INT_MIN
        assert( y >= 0 );

    // long gets zero-extended, so it does not
    // have the INT_MIN problem
    long z = __sym_val_i32();
    assert( z >= 0 );

    return 0;
}

