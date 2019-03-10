/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

void test_abc( const Split & split )
{
    const auto & sec = split.sections();
    assert( sec.size() == 1 );
    test_section( sec[ 0 ], 0, 3 );

    const auto & seg = sec[ 0 ].segments();
    assert( seg.size() == 3 );
    test_segment( seg[ 0 ], 0, 1, 'a' );
    test_segment( seg[ 1 ], 1, 2, 'b' );
    test_segment( seg[ 2 ], 2, 3, 'c' );
}

int main()
{
    { // simple
        auto split = Split( "abc", 4 );
        auto other = Split( "bbb", 4 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 3, 'b' );
    }

    { // overwrite first section
        auto split = Split( "aaa\0cc", 7 );
        auto other = Split( "bbb", 4 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 3, 'b' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 6, 'c' );
    }

    { // overwrite multiple sections
        auto split = Split( "a\0b\0", 5 );
        auto other = Split( "abcd", 5 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
        test_segment( seg[ 2 ], 2, 3, 'c' );
        test_segment( seg[ 3 ], 3, 4, 'd' );
    }

    { // overwrite multiple segments
        auto split = Split( "aabb", 5 );
        auto other = Split( "abc", 4 );
        split.strcpy( &other );

        assert( split.size() == 5 );

        test_abc( split );
    }

    { // partialy overwrite first segment
        auto split = Split( "aaaaabbb", 9 );
        auto other = Split( "abc", 4 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 8 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 3 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );
        test_segment( seg0[ 1 ], 1, 2, 'b' );
        test_segment( seg0[ 2 ], 2, 3, 'c' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 4, 5, 'a' );
        test_segment( seg1[ 1 ], 5, 8, 'b' );
    }

    { // partialy overwrite first segment with emptry string
        auto split = Split( "aaabbb", 7 );
        auto other = Split( "", 1 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 1, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 1, 3, 'a' );
        test_segment( seg[ 1 ], 3, 6, 'b' );
    }

    { // overwrite first segment with emptry string
        auto split = Split( "abbb", 5 );
        auto other = Split( "", 1 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 1, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 1, 4, 'b' );
    }

    { // partialy overwrite segment
        auto split = Split( "aabbb", 6 );
        auto other = Split( "abc", 4 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 5 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 3 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );
        test_segment( seg0[ 1 ], 1, 2, 'b' );
        test_segment( seg0[ 2 ], 2, 3, 'c' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 5, 'b' );
    }

    { // copy into next section
        auto split = Split( "aa\0bb", 6 );
        auto other = Split( "abc", 4 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 5 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 3 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );
        test_segment( seg0[ 1 ], 1, 2, 'b' );
        test_segment( seg0[ 2 ], 2, 3, 'c' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 5, 'b' );
    }

    { // copy empty string
        auto split = Split( "aaabbb", 7 );
        auto other = Split( "\0aabb", 6 );
        split.strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 1, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 1, 3, 'a' );
        test_segment( seg[ 1 ], 3, 6, 'b' );
    }

    { // copy into empty string
        char buff[10] = {};
        auto split = Split( buff, 10 );
        auto other = Split( "abc", 4 );
        split.strcpy( &other );

        test_abc( split );
    }
}
