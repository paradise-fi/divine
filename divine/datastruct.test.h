// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/datastruct.h>
#include <divine/generator.h>

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

    Test queue() {
        generator::Dummy d;
        Queue< generator::Dummy > q( d );
        assert( q.empty() );
        q.pushSuccessors( d.initial() );
        assert( !q.empty() );
        q.pop();
        assert( !q.empty() );
        q.pop();
        assert( q.empty() );
        q.pushSuccessors( d.initial() );
        q.pushSuccessors( q.next().second );
        assert( !q.empty() );
        assert_eq( q.next().second.get< short >(), 1 );
        assert_eq( q.next().second.get< short >( 2 ), 0 );
        assert( q.next().second == d.successors( d.initial() ).head() );
        q.pop();
        assert_eq( q.next().second.get< short >(), 0 );
        assert_eq( q.next().second.get< short >( 2 ), 1 );
        assert( q.next().second
                == d.successors( d.initial() ).tail().head() );
        q.pop();
        assert_eq( q.next().second.get< short >(), 2 );
        assert_eq( q.next().second.get< short >( 2 ), 0 );
        assert( q.next().second
                == d.successors( d.successors( d.initial() ).head() ).head() );
        q.pop();
        assert_eq( q.next().second.get< short >(), 1 );
        assert_eq( q.next().second.get< short >( 2 ), 1 );
        assert( q.next().second
                == d.successors(
                    d.successors( d.initial() ).head() ).tail().head() );
        q.pop();
        assert( q.empty() );
    }
};
