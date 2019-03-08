/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include <dios.h>
#include <rst/split.h>
#include <cassert>

using namespace abstract::mstring;

int main()
{
    { // simple segmentation
        auto split = Split( "aaa", 4 );
        assert( split.strlen() == 3 );
    }

    { // many segments
        auto split = Split( "aaabccaxxadd", 13 );
        assert( split.strlen() == 12 );
    }

    { // multiple sections
        auto split = Split( "aaabbb\0caa", 11 );
        assert( split.strlen() == 6 );
    }

    { // zero prefixed
        auto split = Split( "\0aaabbb", 8 );
        assert( split.strlen() == 0 );
    }

    { // null split
        auto split = Split( nullptr, 0 );
        assert( split.strlen() == 0 );
    }
}
