/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{
    auto test_aaabbb = [] ( auto & split ) {
        assert( split.size() == 7 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 3, 'a' );
        test_segment( seg[ 1 ], 3, 6, 'b' );
    };

    { // simple
        char buff[7] = "aaa";
        auto split = Split( buff, 7 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        test_aaabbb( split );
    }

    { // overwrite last section
        char buff[7] = "aaa\0cc";
        auto split = Split( buff, 7 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        test_aaabbb( split );
    }

    { // overwrite next section
        char buff[11] = "aaa\0bb\0ccc";
        auto split = Split( buff, 11 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        assert( split.size() == 11 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 6 );
        test_section( sec[ 1 ], 7, 10 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );
        test_segment( seg0[ 1 ], 3, 6, 'b' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 7, 10, 'c' );
    }

    { // overwrite next section
        char buff[12] = "aaa\0bbb\0ccc";
        auto split = Split( buff, 12 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 6 );
        test_section( sec[ 1 ], 8, 11 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );
        test_segment( seg0[ 1 ], 3, 6, 'b' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 8, 11, 'c' );
    }

    { // overwrite part of next section
        char buff[11] = "aa\0xxxxx\0c";
        auto split = Split( buff, 11 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 3 );
        test_section( sec[ 0 ], 0, 5 );
        test_section( sec[ 1 ], 6, 8 );
        test_section( sec[ 2 ], 9, 10 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );
        test_segment( seg0[ 1 ], 2, 5, 'b' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 6, 8, 'x' );

        const auto & seg2 = sec[ 2 ];
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 9, 10, 'c' );
    }

    { // overwrite part of next with multiple sections
        char buff[11] = "aa\0abcde\0c";
        auto split = Split( buff, 11 );
        auto other = Split( "bbb", 4 );
        split.strcat( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 3 );
        test_section( sec[ 0 ], 0, 5 );
        test_section( sec[ 1 ], 6, 8 );
        test_section( sec[ 2 ], 9, 10 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );
        test_segment( seg0[ 1 ], 2, 5, 'b' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 6, 7, 'd' );
        test_segment( seg1[ 1 ], 7, 8, 'e' );

        const auto & seg2 = sec[ 2 ];
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 9, 10, 'c' );
    }

    { // merge concated segments
        char buff[7] = "aaa\0cc";
        auto split = Split( buff, 7 );
        auto other = Split( "aaa", 4 );
        split.strcat( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 6, 'a' );
    }

    auto test_abc = [] ( Split & split ) {
        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
        test_segment( seg[ 2 ], 2, 3, 'c' );
    };

    { // empty string
        char buff[5] = "\0abc";
        auto split = Split( buff, 5 );
        auto other = Split( "abc", 4 );
        split.strcat( &other );

        test_abc( split );
    }

    { // rewrite zeros
        char buff[5] = "\0\0\0\0";
        auto split = Split( buff, 5 );
        auto other = Split( "abc", 4 );
        split.strcat( &other );

        test_abc( split );
    }

    { // rewrite zeros
        char buff[8] = "\0\0\0\0aaa";
        auto split = Split( buff, 8 );
        auto other = Split( "abc", 4 );
        split.strcat( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 7 );

        const auto & seg1 = sec[ 0 ];
        assert( seg1.size() == 3 );
        test_segment( seg1[ 0 ], 0, 1, 'a' );
        test_segment( seg1[ 1 ], 1, 2, 'b' );
        test_segment( seg1[ 2 ], 2, 3, 'c' );

        const auto & seg2 = sec[ 1 ];
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 4, 7, 'a' );
    }
}
