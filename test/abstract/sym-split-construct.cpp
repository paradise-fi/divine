/* TAGS: segmentation min sym c++ todo */
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
        assert( split.read( 0 ) == 'a' );
        auto c1 = split.read( 1 );
        assert( c1 == 'a' || c1 == '\0' );
        auto c2 = split.read( 2 );
        assert( c2 == 'a' || c2 == '\0' );
        auto c3 = split.read( 4 );
        assert( c3 == '\0' );
    }

    { // simple two letters
        auto split = sym::segmentation( 'a', Bound{ 1, 5 }, 'b', Bound{ 5, 6 }, '\0', Bound{ 6, 7 } );
        assert( split.read( 0 ) == 'a' );
        auto c1 = split.read( 1 );
        assert( c1 == 'a' || c1 == 'b' );
        auto c2 = split.read( 2 );
        assert( c2 == 'a' || c2 == 'b' );
        auto c3 = split.read( 4 );
        assert( c3 == 'b' );
    }
}

