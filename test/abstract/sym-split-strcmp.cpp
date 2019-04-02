/* TAGS: segmentation min sym c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc --leakcheck exit */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rst/segmentation.h>

using namespace abstract::mstring;

int main() {
    { // equal
        auto lhs = sym::segmentation( 'a', Bound{ 1, 2 }, 'b',
                                     Bound{ 2, 3 }, '\0', Bound{ 3, 4 } );
        auto rhs = sym::segmentation( 'a', Bound{ 1, 2 }, 'b',
                                     Bound{ 2, 3 }, '\0', Bound{ 3, 4 } );
        assert( strcmp( &lhs, &rhs ) == 0 );
    }

    { // nonequal
        auto lhs = sym::segmentation( 'a',  Bound{ 2, 3 }, '\0', Bound{ 3, 4 } );
        auto rhs = sym::segmentation( 'a', Bound{ 1, 2 }, 'b', Bound{ 2, 3 }, '\0', Bound{ 3, 4 } );
        assert( strcmp( &lhs, &rhs ) < 0 );
    }

    { // maybe equal
        auto lhs = sym::segmentation( 'a', Bound{ 2, 3 }, '\0', Bound{ 3, 4 } );
        auto rhs = sym::segmentation( 'a', Bound{ 1, 3 }, '\0', Bound{ 3, 4 } );
        auto res = strcmp( &lhs, &rhs );
        assert( res >= 0 );
    }

    { // offset equal

    }

    { // offset nonequal

    }

    { // different suffix

    }
}
