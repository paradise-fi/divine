// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/graph/datastruct.h>
#include <divine/generator/dummy.h>

using namespace divine;

struct TestDatastruct {
    typedef Blob Node;
    typedef generator::Dummy::Label Label;

    Node first, second, third;
    void init( generator::Dummy &g ) {
        int count = 0;
        first = g.initial();
        visitor::setPermanent( first );
        g.successors( first, [&]( Node n, Label ) {
                visitor::setPermanent( n );
                if ( count == 0 )
                    second = n;
                if (count == 1 )
                    third = n;
                ++ count;
            } );
        assert_eq( count, 2 );
    }

    template< typename Q >
    void _queue(generator::Dummy& d, Q& q) {
        int count = 0;

        init( d );

        assert( q.empty() );
        q.push( first );
        assert( !q.empty() );

        count = 0;
        q.processOpen( [&]( Node, Node, Label ) { ++count; } );
        q.processClosed( [&]( Node ) {} );
        assert_eq( count, 2 );
        assert( q.empty() );

        count = 0;
        q.push( first );
        q.push( second );
        assert( !q.empty() );

        q.processOpen( [&]( Node, Node n, Label ) {
                if ( count == 0 ) {
                    assert_eq( n.template get< short >(), 1 );
                    assert_eq( n.template get< short >( 2 ), 0 );
                    assert( n == second );
                }
                if ( count == 1 ) {
                    assert_eq( n.template get< short >(), 0 );
                    assert_eq( n.template get< short >( 2 ), 1 );
                    assert( n == third );
                }
                ++ count;
            } );
        q.processClosed( [&]( Node ) {} );

        assert_eq( count, 2 );
        assert( !q.empty() );

        count = 0;
        q.processOpen( [&]( Node, Node n, Label ) {
                if ( count == 0 ) {
                    assert_eq( n.template get< short >(), 2 );
                    assert_eq( n.template get< short >( 2 ), 0 );
                }
                if ( count == 1 ) {
                    assert_eq( n.template get< short >(), 1 );
                    assert_eq( n.template get< short >( 2 ), 1 );
                }
                ++ count;
            } );
        q.processClosed( [&]( Node ) {} );
        assert_eq( count, 2 );
        assert( q.empty() );
    }

    Test queue() {
        generator::Dummy d;
        Queue< generator::Dummy, NoStatistics > q( d );
        _queue( d, q );
    }

    Test sharedQueue() {
        generator::Dummy d;
        typedef SharedQueue< generator::Dummy, NoStatistics > Queue;
        Queue::TerminatorPtr t = std::make_shared< Queue::Terminator >();
        Queue::ChunkQPtr ch = std::make_shared< Queue::ChunkQ >();

        Queue q( ch, d, t );
        q.maxChunkSize = 1;
        q.chunkSize = 1;
        _queue( d, q );
    }

    Test stack() {
        generator::Dummy d;
        Stack< generator::Dummy, NoStatistics > q( d );
        bool die = true;

        init( d );

        assert( q.empty() );
        q.push( first );
        q.processClosed( []( Node ) { assert_die(); } );
        assert( !q.empty() );

        q.processOpen( [&]( Node, Node, Label ) { die = false; } );
        q.processClosed( []( Node ) { assert_die(); } );
        assert( !q.empty() );
        assert( !die );

        die = true;
        q.processOpen( [&]( Node, Node, Label ) { die = false; } );
        assert( !die );

        die = true;
        q.processClosed( [&]( Node ) { die = false; } );
        assert( !die );
        assert( q.empty() );

        q.push( first );
        q.processClosed( []( Node ) { assert_die(); } );
        q.push( second );

        // 1, 1, from 1, 0
        q.processOpen( []( Node f, Node t, Label ) {
                assert_eq( f.get< short >(), 1 );
                assert_eq( f.get< short >( 2 ), 0 );
                assert_eq( t.get< short >(), 1 );
                assert_eq( t.get< short >( 2 ), 1 );
            } );
        q.processClosed( []( Node ) { assert_die(); } );
        assert( !q.empty() );

        // 2, 0, from 1, 0
        q.processOpen( []( Node f, Node t, Label ) { 
                assert_eq( f.get< short >(), 1 );
                assert_eq( f.get< short >( 2 ), 0 );
                assert_eq( t.get< short >(), 2 );
                assert_eq( t.get< short >( 2 ), 0 );
            } );
        die = true;
        q.processClosed( [&]( Node ) { die = false; } );
        assert( !die );
        assert( !q.empty() );

        // 0, 1, from 0, 0
        q.processOpen( []( Node f, Node t, Label ) {
                assert_eq( f.get< short >(), 0 );
                assert_eq( f.get< short >( 2 ), 0 );
                assert_eq( t.get< short >(), 0 );
                assert_eq( t.get< short >( 2 ), 1 );
            } );
        assert( !q.empty() );

        // 1, 0, from 0, 0
        q.processOpen( []( Node f, Node t, Label ) {
                assert_eq( f.get< short >(), 0 );
                assert_eq( f.get< short >( 2 ), 0 );
                assert_eq( t.get< short >(), 1 );
                assert_eq( t.get< short >( 2 ), 0 );
            } );

        die = true;
        q.processClosed( [&]( Node ) { die = false; } );
        assert( !die );

        assert( q.empty() );
    }

};
