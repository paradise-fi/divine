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
        auto dst = sym::segmentation( 'a', Bound{ 1, 5 }, '\0', Bound{ 5, 6 } );
        auto src = sym::segmentation( 'b', Bound{ 1, 3 }, '\0', Bound{ 3, 4 } );

        strcpy( &dst, &src );
        assert( dst.read( 0 ) == 'b' );
        auto c1 = dst.read( 1 );
        assert( c1 == 'b' || c1 == '\0' );
        auto c2 = dst.read( 2 );
        assert( c2 == 'b' || c2 == 'a' || c2 == '\0' );
        assert( dst.read( 3 ) == 'a' || dst.read( 3 ) == '\0' );
        assert( dst.read( 4 ) == '\0' );
    }

    { // simple two letter
        auto dst = sym::segmentation( 'a',  Bound{ 1, 5 }, '\0', Bound{ 5, 6 } );
        auto src = sym::segmentation( 'b',  Bound{ 1, 3 }, 'c', Bound{ 4, 5 }, '\0', Bound{ 5, 6 } );

        strcpy( &dst, &src );
        assert( dst.read( 0 ) == 'b' );
        auto c1 = dst.read( 1 );
        assert( c1 == 'b' || c1 == 'c' );
        auto c2 = dst.read( 2 );
        assert( c2 == 'b' || c2 == 'c' || c2 == '\0' );
        auto c3 = dst.read( 3 );
        assert( c3 == 'a' || c3 == 'c' || c3 == '\0' );
        assert( dst.read( 4 ) == '\0' );
    }
}
