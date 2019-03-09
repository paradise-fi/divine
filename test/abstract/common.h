#pragma once

#include <cassert>
#include <rst/split.h>

using namespace abstract::mstring;

void test_section( const Section & sec, int from, int to ) {
    assert( sec.from() == from );
    assert( sec.to() == to );
};

void test_segment( const Segment & seg, int from, int to, char val ) {
    assert( seg.from == from );
    assert( seg.to == to );
    assert( seg.value == val );
};

