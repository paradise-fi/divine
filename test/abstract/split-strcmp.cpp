/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{

    { // triviel case
        auto split = Split( "", 1 );
        auto other = Split( "", 1 );
        assert( split.strcmp( &other ) == 0 );
    }

    { // identity
        auto split = Split( "a", 2 );
        auto other = Split( "a", 2 );
        assert( split.strcmp( &other ) == 0 );
    }

    { // multicharacter identity
        auto split = Split( "abbccc", 7 );
        auto other = Split( "abbccc", 7 );
        assert( split.strcmp( &other ) == 0 );
    }

    { // multicharacter identity with differen suffix section
        auto split = Split( "abbc\0abc", 9 );
        auto other = Split( "abbc\0cba", 9 );
        assert( split.strcmp( &other ) == 0 );
    }

    { // length mismatch
        auto split = Split( "abbc", 5 );
        auto other = Split( "abbcc", 6 );
        assert( split.strcmp( &other ) < 0 );
    }

    { // length mismatch
        auto split = Split( "abbcc", 6 );
        auto other = Split( "abbc", 5 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // length mismatch
        auto split = Split( "", 1 );
        auto other = Split( "abbcc", 6 );
        assert( split.strcmp( &other ) < 0 );
    }

    { // length mismatch
        auto split = Split( "abbcc", 6 );
        auto other = Split( "", 1 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // typo
        auto split = Split( "abbd", 5 );
        auto other = Split( "abbc", 5 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // typo
        auto split = Split( "abbc", 5 );
        auto other = Split( "abbd", 5 );
        assert( split.strcmp( &other ) < 0 );
    }

    { // typo
        auto split = Split( "bbbbbc", 7 );
        auto other = Split( "abbbbc", 7 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // typo
        auto split = Split( "abcdef", 7 );
        auto other = Split( "abcccc", 7 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // missmatching segments
        auto split = Split( "bbbacc", 7 );
        auto other = Split( "bbaaac", 7 );
        assert( split.strcmp( &other ) > 0 );
    }

    { // missmatching second segments
        auto split = Split( "bbaacc", 7 );
        auto other = Split( "bbaaac", 7 );
        assert( split.strcmp( &other ) > 0 );
    }
}
