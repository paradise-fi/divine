/* TAGS: segmentation min sym c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc --leakcheck exit */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rst/segmentation.h>

using namespace abstract::mstring;

int main() {
    { // simple one letter
        auto split = sym::segmentation( 'a', Bound{ 1, 5 }, '\0', Bound{ 5, 6 } );
        auto len = strlen( &split );
        assert( len >= 1 && len < 5 );
    }

    { // multisection strlen
        auto split = sym::segmentation( 'x', Bound{ 3, 4 }, 'y', Bound{ 3, 5 },
                                        'z', Bound{ 7, 10 }, '\0', Bound{ 10, 11 } );
        auto len = strlen( &split );
        assert( len >= 7 && len < 10 );
    }

    { // offseted strlen

    }

    { // empty string strlen
        auto split = sym::segmentation( '\0', Bound{ 5, 6 } );
        assert( strlen( &split ) == 0 );
    }
}
