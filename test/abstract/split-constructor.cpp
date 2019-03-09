/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{
    { // simple segmentation
        auto split = Split( "aaabbb", 7 );

        assert( split.size() == 7 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 3, 'a' );
        test_segment( seg[ 1 ], 3, 6, 'b' );
    }

    { // non zero ending
        char buff[4] = { 'a', 'a', 'b', 'b' };
        auto split = Split( buff, 4 );

        assert( split.size() == 4 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 2, 'a' );
        test_segment( seg[ 1 ], 2, 4, 'b' );
    }

    { // multiple sections
        auto split = Split( "aa\0ab", 6 );

        assert( split.size() == 6 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 2 );
        test_section( sec[ 1 ], 3, 5 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 3, 4, 'a' );
        test_segment( seg1[ 1 ], 4, 5, 'b' );
    }

    { // multiple zeros in sequence
        auto split = Split( "aa\0\0ab\0", 8 );

        assert( split.size() == 8 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 2 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 4, 5, 'a' );
        test_segment( seg1[ 1 ], 5, 6, 'b' );
    }

    { // starts with a zero
        auto split = Split( "\0caab\0", 7 );

        assert( split.size() == 7 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 1, 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 1, 2, 'c' );
        test_segment( seg[ 1 ], 2, 4, 'a' );
        test_segment( seg[ 2 ], 4, 5, 'b' );
    }

    { // null segmentation
        auto split = Split( nullptr, 0 );
        assert( split.empty() );
        assert( split.size() == 0 );
        assert( split.sections().size() == 0 );
    }
}
