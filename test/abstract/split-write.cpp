/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{
    { // simple write
        auto split = Split( "abc", 4 );
        split.write( 1, 'd' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'd' );
        test_segment( seg[ 2 ], 2, 3, 'c' );
    }

    { // simple segment join
        auto split = Split( "abc", 4 );
        split.write( 1, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 2, 'a' );
        test_segment( seg[ 1 ], 2, 3, 'c' );
    }

    { // simple segment join
        auto split = Split( "abc", 4 );
        split.write( 1, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 3, 'c' );
    }

    { // simple front segment join
        auto split = Split( "abc", 4 );
        split.write( 0, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 2, 'b' );
        test_segment( seg[ 1 ], 2, 3, 'c' );
    }

    { // simple back segment join
        auto split = Split( "abc", 4 );
        split.write( 2, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 3, 'b' );
    }

    { // simple front write zero
        auto split = Split( "abc", 4 );
        split.write( 0, '\0' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 1, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 1, 2, 'b' );
        test_segment( seg[ 1 ], 2, 3, 'c' );
    }

    { // simple middle write zero
        auto split = Split( "abc", 4 );
        split.write( 1, '\0' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 2, 3 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 2, 3, 'c' );
    }

    { // middle write zero
        auto split = Split( "abbbc", 6 );
        split.write( 2, '\0' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 2 );
        test_section( sec[ 1 ], 3, 5 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );
        test_segment( seg0[ 1 ], 1, 2, 'b' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 3, 4, 'b' );
        test_segment( seg1[ 1 ], 4, 5, 'c' );
    }

    { // simple back write zero
        auto split = Split( "abc", 4 );
        split.write( 2, '\0' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 2 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
    }

    auto test_abc = [] ( const auto & split ) {
        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
        test_segment( seg[ 2 ], 2, 3, 'c' );
    };

    { // rewrite front zero
        auto split = Split( "\0bc", 4 );
        split.write( 0, 'a' );

        test_abc( split );
    }

    { // rewrite middle zero
        auto split = Split( "a\0c", 4 );
        split.write( 1, 'b' );

        test_abc( split );
    }

    { // rewrite back zero
        auto split = Split( "ab\0", 4 );
        split.write( 2, 'c' );

        test_abc( split );
    }

    { // rewrite back zero
        auto split = Split( "abc", 4 );
        split.write( 3, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
        test_segment( seg[ 2 ], 2, 4, 'c' );
    }

    { // destroy section
        auto split = Split( "a\0c\0a", 6 );
        split.write( 2, '\0' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 4, 5 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 5, 'a' );
    }

    { // join section
        auto split = Split( "a\0a", 4 );
        split.write( 1, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 3, 'a' );
    }

    { // extend section
        auto split = Split( "a\0\0a", 5 );
        split.write( 1, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 2 );
        test_section( sec[ 1 ], 3, 4 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 3, 4, 'a' );
    }

    { // extend section
        auto split = Split( "a\0\0a", 5 );
        split.write( 2, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 2, 4 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 2, 4, 'a' );
    }

    { // extend section
        auto split = Split( "a\0\0a", 5 );
        split.write( 4, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 3, 5 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 3, 5, 'a' );
    }

    { // small string
        auto split = Split( "x", 2 );
        split.write( 0, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 1 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 1, 'a' );

        split.write( 0, '\0' );
        assert( split.sections().empty() );

        split.write( 0, 'a' );

        const auto & sec1 = split.sections();
        assert( sec1.size() == 1 );
        test_section( sec1[ 0 ], 0, 1 );

        const auto & seg1 = sec[ 0 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 0, 1, 'a' );
    }

    { // split section
        auto split = Split( "aaaa", 5 );
        split.write( 0, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'b' );
        test_segment( seg[ 1 ], 1, 4, 'a' );
    }

    { // split section
        auto split = Split( "aaaa", 5 );
        split.write( 1, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'b' );
        test_segment( seg[ 2 ], 2, 4, 'a' );
    }

    { // split section
        auto split = Split( "aaaa", 5 );
        split.write( 3, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 3, 'a' );
        test_segment( seg[ 1 ], 3, 4, 'b' );
    }

    { // create section
        auto split = Split( "a\0\0\0a", 6 );
        split.write( 2, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 3 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 2, 3 );
        test_section( sec[ 2 ], 4, 5 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 2, 3, 'a' );

        const auto & seg2 = sec[ 2 ];
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 4, 5, 'a' );
    }

    { // create section
        auto split = Split( "\0\0\0a", 5 );
        split.write( 1, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 1, 2 );
        test_section( sec[ 1 ], 3, 4 );

        const auto & seg0 = sec[ 0 ];
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 1, 2, 'a' );

        const auto & seg1 = sec[ 1 ];
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 3, 4, 'a' );
    }

    { // create section in empty split
        auto split = Split( "\0\0", 3 );
        split.write( 2, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 2, 3 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 2, 3, 'a' );
    }

    { // split section
        auto split = Split( "bbbaaa", 7 );
        split.write( 4, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ];
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 3, 'b' );
        test_segment( seg[ 1 ], 3, 4, 'a' );
        test_segment( seg[ 2 ], 4, 5, 'b' );
        test_segment( seg[ 3 ], 5, 6, 'a' );
    }

}
