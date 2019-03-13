/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

void check( const Split * split, const char * expected ) {
    int i = 0;
    while( expected[i] != '\0') {
        assert( split->read( i ) == expected[ i ] );
        ++i;
    }
    assert( split->read( i ) == '\0' );
}

int main()
{
   { // simple
        auto split = Split( "aabbcc", 7 );
        auto rest = split.strchr( 'b' );

        assert( rest->strlen() == 4 );
        check( rest, "bbcc" );
    }

    { // first character
        auto split = Split( "aabbcc", 7 );
        auto rest = split.strchr( 'a' );

        assert( rest->strlen() == 6 );

        check( rest, "aabbcc" );
    }

    { // last character
        auto split = Split( "abbc", 5 );
        auto rest = split.strchr( 'c' );

        assert( rest->strlen() == 1 );
        check( rest, "c" );
    }

    { // multiple consecutive bounds
        auto split = Split( "aabbcc", 7 );
        auto rest1 = split.strchr( 'b' );

        assert( rest1->strlen() == 4 );
        check( rest1, "bbcc" );

        rest1 = rest1->offset( 1 );

        check( rest1, "bcc" );
        auto rest2 = rest1->strchr( 'b' );

        assert( rest2->strlen() == 3 );
        check( rest2, "bcc" );
    }

    { // missing character
        auto split = Split( "aabbcc", 7 );
        auto rest = split.strchr( 'd' );
        assert( rest == nullptr );
    }

    { // empty string of interest
        auto split = Split( "\0aa", 4 );
        auto rest = split.strchr( 'a' );
        assert( rest == nullptr );
    }

    { // empty string
        auto split = Split( "", 1 );
        auto rest = split.strchr( 'a' );
        assert( rest == nullptr );
    }

    { // multiple sections
        auto split = Split( "abc\0abc", 8 );

        auto rest1 = split.strchr( 'a' )->offset( 1 );
        check( rest1, "bc" );

        auto rest2 = rest1->strchr( 'a' );
        assert( rest2 == nullptr );
    }

    { // cooperation with strlen
        auto split = Split( "abbcc", 6 );
        auto rest = split.strchr( 'c' );
        assert( rest->strlen() == 2 );
    }

    { // cooperation with strcpy (from)
        auto split = Split( "abc", 4 );
        auto other = Split( "abbbb", 6 );
        auto shifted = other.strchr( 'b' )->offset( 1 );
        split.strcpy( shifted );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 3, 'b' );
    }

    { // cooperation with strcpy (to)
        auto split = Split( "abbbb", 6 );
        auto shifted = split.strchr( 'b' )->offset( 1 );
        auto other = Split( "bcd", 4 );
        shifted->strcpy( &other );

        const auto & sec = shifted->sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 3, 'b' );
        test_segment( seg[ 2 ], 3, 4, 'c' );
        test_segment( seg[ 3 ], 4, 5, 'd' );
    }

    { // cooperation with strcat (from)
        char buff[7] = "baa\0cc";
        auto split = Split( buff, 7 );
        auto other = Split( "baaa", 5 );
        auto shifted = other.strchr( 'a' );
        split.strcat( shifted );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 7 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 1, 'b' );
        test_segment( seg[ 1 ], 1, 3, 'a' );
        test_segment( seg[ 2 ], 3, 4, 'b' );
        test_segment( seg[ 3 ], 4, 7, 'a' );
    }

    { // cooperation with strcat (to)
        char buff[7] = "baa\0cc";
        auto split = Split( buff, 7 );
        auto shifted = split.strchr( 'a' );
        auto other = Split( "aaa", 4 );
        shifted->strcat( &other );

        const auto & sec = shifted->sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'b' );
        test_segment( seg[ 1 ], 1, 6, 'a' );
    }

    { // strchr rewrite
        auto split = Split( "aaXaaXaa", 9 );

        auto first = split.strchr( 'X' );
        first->write( 0, 'b' ); // aabaaXaa

        first = first->offset( 1 );

        auto second = first->strchr( 'X' );
        second->write( 0, 'a' ); // aabaaaaa

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 8 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 2, 'a' );
        test_segment( seg[ 1 ], 2, 3, 'b' );
        test_segment( seg[ 2 ], 3, 8, 'a' );
    }

    { // strchr cycle
        auto split = Split( "aaXaaXaa", 9 );

        auto str = &split;
        while ( ( str = str->strchr( 'X' ) ) ) {
            str->write( 0, 'a' );
            str->offset( 1 );
        }

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 8 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 8, 'a' );
    }
}
