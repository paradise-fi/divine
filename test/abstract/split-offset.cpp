/* TAGS: mstring min c++ */
/* CC_OPTS: -std=c++17 -I$SRC_ROOT/bricks */
/* VERIFY_OPTS: -o nofail:malloc */

#include "common.h"

int main()
{
    { // offset read
        auto split = Split( "abc", 4 );
        auto off = split.offset( 1 );

        assert( off->read( 0 ) == 'b' );
        assert( off->read( 1 ) == 'c' );
    }

    { // offset read zero
        auto split = Split( "abc\0cba", 8 );
        auto off = split.offset( 3 );

        assert( off->read( 0 ) == '\0' );
        assert( off->read( 1 ) == 'c' );
        assert( off->read( 2 ) == 'b' );
        assert( off->read( 3 ) == 'a' );
    }

    { // offset read next section
        auto split = Split( "abc\0cba", 8 );
        auto off = split.offset( 4 );

        assert( off->read( 0 ) == 'c' );
        assert( off->read( 1 ) == 'b' );
        assert( off->read( 2 ) == 'a' );
    }

    { // offset read in the middle of segment
        auto split = Split( "aaabb", 6 );
        auto off = split.offset( 1 );

        assert( off->read( 0 ) == 'a' );
        assert( off->read( 1 ) == 'a' );
        assert( off->read( 2 ) == 'b' );
        assert( off->read( 3 ) == 'b' );
    }

    { // offset read in next segment
        auto split = Split( "aaabb", 6 );
        auto off = split.offset( 4 );

        assert( off->read( 0 ) == 'b' );
        assert( off->read( 1 ) == '\0' );
    }

    { // offset strlen
        auto split = Split( "aabb", 5 );
        auto off = split.offset( 1 );
        assert( off->strlen() == 3 );
    }

    { // offset strlen
        auto split = Split( "aa\0bbbb", 8 );
        auto off = split.offset( 4 );
        assert( off->strlen() == 3 );
    }

    { // offset write
        auto split = Split( "aabb", 5 );
        auto off = split.offset( 1 );

        off->write( 0, 'b' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 4, 'b' );
    }

    { // offset write
        auto split = Split( "aabb", 5 );
        auto off = split.offset( 1 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'c' );
        test_segment( seg[ 2 ], 2, 4, 'b' );
    }

    { // offset write
        auto split = Split( "aabb", 5 );
        auto off = split.offset( 1 );

        off->write( 1, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 2, 'a' );
        test_segment( seg[ 1 ], 2, 3, 'c' );
        test_segment( seg[ 2 ], 3, 4, 'b' );
    }

    { // offset write
        auto split = Split( "aaabb", 6 );
        auto off = split.offset( 1 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'c' );
        test_segment( seg[ 2 ], 2, 3, 'a' );
        test_segment( seg[ 3 ], 3, 5, 'b' );
    }

    { // offset write next section
        auto split = Split( "aaa\0bb", 7 );
        auto off = split.offset( 4 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 4, 5, 'c' );
        test_segment( seg1[ 1 ], 5, 6, 'b' );
    }

    { // offset write next section
        auto split = Split( "aaa\0bb", 7 );
        auto off = split.offset( 4 );

        off->write( 1, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 4, 5, 'b' );
        test_segment( seg1[ 1 ], 5, 6, 'c' );
    }

    { // offset rewrite zero between sections
        auto split = Split( "aaa\0bb", 7 );
        auto off = split.offset( 3 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        test_segment( seg[ 0 ], 0, 3, 'a' );
        test_segment( seg[ 1 ], 3, 4, 'c' );
        test_segment( seg[ 2 ], 4, 6, 'b' );
    }

    { // offset rewrite zero between sections with merge
        auto split = Split( "aaa\0aa", 7 );
        auto off = split.offset( 3 );

        off->write( 0, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 6, 'a' );
    }

    { // offset strcpy (to)
        auto split = Split( "abbbb", 6 );
        auto off = split.offset( 1 );
        auto other = Split( "abcd", 5 );
        off->strcpy( &other );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 2, 'a' );
        test_segment( seg[ 1 ], 2, 3, 'b' );
        test_segment( seg[ 2 ], 3, 4, 'c' );
        test_segment( seg[ 3 ], 4, 5, 'd' );
    }

    { // offset strcpy (from)
        auto split = Split( "abc", 4 );
        auto other = Split( "abbb", 5 );
        auto off = other.offset( 1 );
        split.strcpy( off );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 3 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 3, 'b' );
    }

    { // negative offset
        auto split = Split( "aaabb", 6 );
        auto off = split.offset( 3 );
        off = off->offset( -2 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 4 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
        test_segment( seg[ 1 ], 1, 2, 'c' );
        test_segment( seg[ 2 ], 2, 3, 'a' );
        test_segment( seg[ 3 ], 3, 5, 'b' );
    }

    { // negative to previous section
        auto split = Split( "aaa\0bb", 7 );
        auto off = split.offset( 4 );
        off = off->offset( -2 );

        off->write( 0, 'c' );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 2, 'a' );
        test_segment( seg0[ 1 ], 2, 3, 'c' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 6, 'b' );
    }

    { // offset of offset of offset
        auto split = Split( "aaa\0aa", 7 );
        auto off = split.offset( 1 );
        off = off->offset( 1 );
        off = off->offset( 1 );
        off->write( 0, 'a' );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 6, 'a' );
    }

    { // strcpy from offset to offset
        auto split = Split( "aaa\0aa", 7 );
        auto off1 = split.offset( 1 );

        auto other = Split( "\0bb", 4 );
        auto off2 = other.offset( 1 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 2 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );
        test_segment( seg0[ 1 ], 1, 3, 'b' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 6, 'a' );
    }

    { // strcpy from offset to offset
        auto split = Split( "aaa\0aa", 7 );
        auto off1 = split.offset( 4 );

        auto other = Split( "a\0bb", 5 );
        auto off2 = other.offset( 2 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 6, 'b' );
    }

    { // strcpy from offset to offset
        auto split = Split( "aaa\0aaa", 8 );
        auto off1 = split.offset( 4 );

        auto other = Split( "a\0bb\0cc", 8 );
        auto off2 = other.offset( 6 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 3 );
        test_section( sec[ 0 ], 0, 3 );
        test_section( sec[ 1 ], 4, 5 );
        test_section( sec[ 2 ], 6, 7 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 3, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 4, 5, 'c' );
        const auto & seg2 = sec[ 2 ].segments();
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 6, 7, 'a' );
    }

    { // strcpy from offset to offset
        auto split = Split( "aaa\0aa", 7 );
        auto off1 = split.offset( 1 );

        auto other = Split( "a\0b", 4 );
        auto off2 = other.offset( 3 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 3 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 2, 3 );
        test_section( sec[ 2 ], 4, 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 1 );
        test_segment( seg1[ 0 ], 2, 3, 'a' );

        const auto & seg2 = sec[ 2 ].segments();
        assert( seg2.size() == 1 );
        test_segment( seg2[ 0 ], 4, 6, 'a' );
    }

    { // overwrite section by offset
        auto split = Split( "a\0a", 4 );
        auto off1 = split.offset( 2 );

        auto other = Split( "a\0b", 4 );
        auto off2 = other.offset( 1 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        test_section( sec[ 0 ], 0, 1 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 1 );
        test_segment( seg[ 0 ], 0, 1, 'a' );
    }

    { // strcat with offset in next sections
        char buff[5] = "a\0a";
        auto split = Split( buff, 5 );
        auto off1 = split.offset( 3 );

        auto other = Split( "a\0b", 4 );
        auto off2 = other.offset( 2 );

        off1->strcpy( off2 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        test_section( sec[ 0 ], 0, 1 );
        test_section( sec[ 1 ], 2, 4 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        test_segment( seg0[ 0 ], 0, 1, 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        test_segment( seg1[ 0 ], 2, 3, 'a' );
        test_segment( seg1[ 1 ], 3, 4, 'b' );
    }
}
