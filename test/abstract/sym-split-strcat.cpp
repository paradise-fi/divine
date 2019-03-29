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
        auto dst = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 1, 5 }, '\0', Bound{ 7, 8 } );
        auto src = sym::segmentation( Bound{ 0, 1 }, 'b', Bound{ 1, 3 }, '\0', Bound{ 3, 4 } );

        strcat( &dst, &src );
        assert( dst.read( 0 ) == 'a' );
        auto c1 = dst.read( 1 );
        assert( c1 == 'b' || c1 == 'a' );
        auto c2 = dst.read( 2 );
        assert( c2 == 'b' || c2 == 'a' || c2 == '\0' );
        assert( dst.read( 4 ) == 'b' || dst.read( 4 ) == '\0' );
        assert( dst.read( 6 ) == '\0' );
    }
}
