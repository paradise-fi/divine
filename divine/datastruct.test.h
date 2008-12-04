// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/datastruct.h>

using namespace divine;

struct TestDatastruct {
    Test circular() {
        Circular< int, 5 > circ;
        assert( !circ.full() );
        circ.add( 1 );
        assert_eq( *circ.first(), 1 );
        circ.add( 2 );
        circ.add( 3 );
        assert( !circ.full() );
        assert_eq( *circ.first(), 1 );
        assert_eq( *circ.last(), 3 );
        circ.add( 4 );
        assert( !circ.full() );
        circ.add( 5 );
        assert( circ.full() );
        assert_eq( *circ.first(), 1 );
        assert_eq( *circ.last(), 5 );
        circ.drop( 2 );
        assert_eq( *circ.first(), 3 );
        assert_eq( *circ.last(), 5 );
        assert( !circ.full() );
        circ.add( 6 );
        circ.add( 7 );
        assert( circ.full() );
        assert_eq( *circ.last(), 7 );
    }
};
