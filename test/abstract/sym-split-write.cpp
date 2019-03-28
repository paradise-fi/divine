/* TAGS: mstring min sym c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc --leakcheck exit */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rst/segmentation.h>

using namespace abstract::mstring;

int main() {
    { // simple rewrite begin
        auto split = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 2, 5 }, '\0', Bound{ 5, 6 } );
        split.write( 0, 'b' );
        assert( split.read( 0 ) == 'b' );
        assert( split.read( 1 ) == 'a' );
        assert( split.read( 2 ) == 'a' || split.read( 2 ) == '\0' );
        assert( split.read( 4 ) == '\0' );
    }

    { // simple rewrite middle
        auto split = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 3, 5 }, '\0', Bound{ 5, 6 } );
        split.write( 1, 'b' );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'b' );
        assert( split.read( 2 ) == 'a' || split.read( 2 ) == '\0' );
        assert( split.read( 4 ) == '\0' );
    }

    { // simple rewrite end
        auto split = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 4, 5 }, '\0', Bound{ 5, 6 } );
        split.write( 3, 'b' );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'a' );
        assert( split.read( 3 ) == 'b' );
        assert( split.read( 4 ) == '\0' );
    }

    { // simple rewrite single character segment
        auto split = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 1, 2 }, '\0', Bound{ 2, 3 } );
        split.write( 0, 'b' );
        assert( split.read( 0 ) == 'b' );
        assert( split.read( 1 ) == '\0' );
    }

    { // simple rewrite same character
        auto split = sym::segmentation( Bound{ 0, 1 }, 'a', Bound{ 1, 2 }, '\0', Bound{ 2, 3 } );
        split.write( 0, 'a' );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == '\0' );
    }

    { // write at the beging of second segment
    }

    { // write at the middle of second segment
    }

    { // write at the end of second segment
    }

    { // write at the beging of last segment
    }

    { // write at the middle of last segment
    }

    { // write at the end of last segment
    }
}
