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

    { // simple
        auto dst = sym::segmentation( '\0', Bound{ 3, 10 } );
        auto src = sym::segmentation(  'x', Bound{ 2, 3 } );
        memcpy( &dst, &src, src.size() );

        assert( dst.read( 0 ) == 'x' );
        assert( dst.read( 1 ) == 'x' || dst.read( 1 ) == '\0' );
        assert( dst.read( 2 ) == '\0' );
    }

    { // merge neighbours left

    }

    { // merge neighbours right

    }

    { // insert into one segment

    }

    { // insert into zeroed mstring

    }

    { // copy zeroes

    }

    { // copy only part of segmentation

    }

    { // test offset

    }
}
