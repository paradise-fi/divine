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
        auto split = Split( "aaabbb", 7 );

        assert( split.size() == 7 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        assert( sec[ 0 ].from() == 0 );
        assert( sec[ 0 ].to() == 6 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        assert( seg[ 0 ].from() == 0 );
        assert( seg[ 0 ].to() == 3 );
        assert( seg[ 0 ].value() == 'a' );
        assert( seg[ 1 ].from() == 3 );
        assert( seg[ 1 ].to() == 6 );
        assert( seg[ 1 ].value() == 'b' );
    }

    { // non zero ending
        char buff[4] = { 'a', 'a', 'b', 'b' };
        auto split = Split( buff, 4 );

        assert( split.size() == 4 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        assert( sec[ 0 ].from() == 0 );
        assert( sec[ 0 ].to() == 4 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 2 );
        assert( seg[ 0 ].from() == 0 );
        assert( seg[ 0 ].to() == 2 );
        assert( seg[ 0 ].value() == 'a' );
        assert( seg[ 1 ].from() == 2 );
        assert( seg[ 1 ].to() == 4 );
        assert( seg[ 1 ].value() == 'b' );
    }

    { // multiple sections
        auto split = Split( "aa\0ab", 6 );

        assert( split.size() == 6 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        assert( sec[ 0 ].from() == 0 );
        assert( sec[ 0 ].to() == 2 );
        assert( sec[ 1 ].from() == 3 );
        assert( sec[ 1 ].to() == 5 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        assert( seg0[ 0 ].from() == 0 );
        assert( seg0[ 0 ].to() == 2 );
        assert( seg0[ 0 ].value() == 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        assert( seg1[ 0 ].from() == 3 );
        assert( seg1[ 0 ].to() == 4 );
        assert( seg1[ 0 ].value() == 'a' );
        assert( seg1[ 1 ].from() == 4 );
        assert( seg1[ 1 ].to() == 5 );
        assert( seg1[ 1 ].value() == 'b' );
    }

    { // multiple zeros in sequence
        auto split = Split( "aa\0\0ab\0", 8 );

        assert( split.size() == 8 );

        const auto & sec = split.sections();
        assert( sec.size() == 2 );
        assert( sec[ 0 ].from() == 0 );
        assert( sec[ 0 ].to() == 2 );
        assert( sec[ 1 ].from() == 4 );
        assert( sec[ 1 ].to() == 6 );

        const auto & seg0 = sec[ 0 ].segments();
        assert( seg0.size() == 1 );
        assert( seg0[ 0 ].from() == 0 );
        assert( seg0[ 0 ].to() == 2 );
        assert( seg0[ 0 ].value() == 'a' );

        const auto & seg1 = sec[ 1 ].segments();
        assert( seg1.size() == 2 );
        assert( seg1[ 0 ].from() == 4 );
        assert( seg1[ 0 ].to() == 5 );
        assert( seg1[ 0 ].value() == 'a' );
        assert( seg1[ 1 ].from() == 5 );
        assert( seg1[ 1 ].to() == 6 );
        assert( seg1[ 1 ].value() == 'b' );
    }

    { // starts with a zero
        auto split = Split( "\0caab\0", 7 );

        assert( split.size() == 7 );

        const auto & sec = split.sections();
        assert( sec.size() == 1 );
        assert( sec[ 0 ].from() == 1 );
        assert( sec[ 0 ].to() == 5 );

        const auto & seg = sec[ 0 ].segments();
        assert( seg.size() == 3 );
        assert( seg[ 0 ].from() == 1 );
        assert( seg[ 0 ].to() == 2 );
        assert( seg[ 0 ].value() == 'c' );
        assert( seg[ 1 ].from() == 2 );
        assert( seg[ 1 ].to() == 4 );
        assert( seg[ 1 ].value() == 'a' );
        assert( seg[ 2 ].from() == 4 );
        assert( seg[ 2 ].to() == 5 );
        assert( seg[ 2 ].value() == 'b' );
    }

    { // null segmentation
        auto split = Split( nullptr, 0 );
        assert( split.empty() );
        assert( split.size() == 0 );
        assert( split.sections().size() == 0 );
    }
}
