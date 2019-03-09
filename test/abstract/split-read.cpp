/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{
    { // simple segmentation
        auto split = Split( "abaaa", 6 );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'b' );
        assert( split.read( 2 ) == 'a' );
        assert( split.read( 3 ) == 'a' );
        assert( split.read( 4 ) == 'a' );
        assert( split.read( 5 ) == '\0' );
    }

    { // many segments
        auto split = Split( "aaabccaxxadd", 13 );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'a' );
        assert( split.read( 2 ) == 'a' );
        assert( split.read( 3 ) == 'b' );
        assert( split.read( 4 ) == 'c' );
        assert( split.read( 5 ) == 'c' );
        assert( split.read( 6 ) == 'a' );
        assert( split.read( 7 ) == 'x' );
        assert( split.read( 8 ) == 'x' );
        assert( split.read( 9 ) == 'a' );
        assert( split.read( 10 ) == 'd' );
        assert( split.read( 11 ) == 'd' );
        assert( split.read( 12 ) == '\0' );
    }

    { // multiple sections
        auto split = Split( "aacccc\0cab", 11 );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'a' );
        assert( split.read( 2 ) == 'c' );
        assert( split.read( 3 ) == 'c' );
        assert( split.read( 4 ) == 'c' );
        assert( split.read( 5 ) == 'c' );
        assert( split.read( 6 ) == '\0' );
        assert( split.read( 7 ) == 'c' );
        assert( split.read( 8 ) == 'a' );
        assert( split.read( 9 ) == 'b' );
        assert( split.read( 10 ) == '\0' );
    }

    { // multiple sections
        auto split = Split( "a\0b\0c\0d", 8 );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == '\0' );
        assert( split.read( 2 ) == 'b' );
        assert( split.read( 3 ) == '\0' );
        assert( split.read( 4 ) == 'c' );
        assert( split.read( 5 ) == '\0' );
        assert( split.read( 6 ) == 'd' );
        assert( split.read( 7 ) == '\0' );
    }

    { // zero prefixed
        auto split = Split( "\0\0ccabbb", 9 );
        assert( split.read( 0 ) == '\0' );
        assert( split.read( 1 ) == '\0' );
        assert( split.read( 2 ) == 'c' );
        assert( split.read( 3 ) == 'c' );
        assert( split.read( 4 ) == 'a' );
        assert( split.read( 5 ) == 'b' );
        assert( split.read( 6 ) == 'b' );
        assert( split.read( 7 ) == 'b' );
        assert( split.read( 8 ) == '\0' );
    }

    { // zero sequence in middle
        auto split = Split( "aa\0\0\0c", 7 );
        assert( split.read( 0 ) == 'a' );
        assert( split.read( 1 ) == 'a' );
        assert( split.read( 2 ) == '\0' );
        assert( split.read( 3 ) == '\0' );
        assert( split.read( 4 ) == '\0' );
        assert( split.read( 5 ) == 'c' );
        assert( split.read( 6 ) == '\0' );
    }

    { // zero sequences
        auto split = Split( "\0\0aa\0\0a\0c", 10 );
        assert( split.read( 0 ) == '\0' );
        assert( split.read( 1 ) == '\0' );
        assert( split.read( 2 ) == 'a' );
        assert( split.read( 3 ) == 'a' );
        assert( split.read( 4 ) == '\0' );
        assert( split.read( 5 ) == '\0' );
        assert( split.read( 6 ) == 'a' );
        assert( split.read( 7 ) == '\0' );
        assert( split.read( 8 ) == 'c' );
        assert( split.read( 9 ) == '\0' );
    }
}
