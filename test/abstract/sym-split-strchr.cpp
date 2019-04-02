/* TAGS: segmentation min sym c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc --leakcheck exit */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rst/segmentation.h>

using namespace abstract::mstring;

template< typename Segmentation >
void clean( Segmentation * seg )
{
    seg->~Segmentation();
    __dios_safe_free( seg );
}

int main() {
    { // simple one letter
        auto split = sym::segmentation( 'a', Bound{ 1, 5 }, '\0', Bound{ 5, 6 } );
        auto chr = strchr( &split, 'a' );
        assert( chr->read( 0 ) == 'a' );
        auto c1 = chr->read( 1 );
        assert( c1 == 'a' || c1 == '\0' );
        auto c2 = chr->read( 2 );
        assert( c2 == 'a' || c2 == '\0' );
        auto c3 = chr->read( 4 );
        assert( c3 == '\0' );

        clean( chr );
    }

    { // missing letter
        auto split = sym::segmentation( 'a', Bound{ 1, 2 }, 'b', Bound{ 2, 4 }, '\0', Bound{ 4, 5 } );
        auto chr = strchr( &split, 'b' );
        assert( chr->read( 0 ) == 'b' );
        auto c1 = chr->read( 1 );
        assert( c1 == 'b' || c1 == '\0' );
        assert( chr->read( 2 ) == '\0' );

        clean( chr );
    }

    { // missing letter
        auto split = sym::segmentation( 'a', Bound{ 1, 2 }, 'b', Bound{ 2, 4 }, '\0', Bound{ 4, 5 } );
        assert( strchr( &split, 'c' ) == nullptr );
    }

    { // possibly missing letter
        auto split = sym::segmentation( 'a', Bound{ 1, 2 }, 'b', Bound{ 1, 3 }, '\0', Bound{ 3, 4 } );
        auto chr = strchr( &split, 'b' );

        assert( chr == nullptr || chr->read( 0 ) == 'b' );

        if ( chr )
            clean( chr );
    }
}
