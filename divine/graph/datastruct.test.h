// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/graph/datastruct.h>
#include <divine/generator/dummy.h>
#include <wibble/sys/thread.h>

using namespace divine;

struct TestDatastruct {
    typedef Blob Node;
    typedef generator::Dummy::Label Label;

    Node first, second, third;
    void init( generator::Dummy &g ) {
        int count = 0;
        g.initials( [&]( Node, Node n, Label ) { first = n; } );
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

    template< typename Queue >
    struct Worker : wibble::sys::Thread
    {

        std::shared_ptr< Queue > queue;
        int add;
        int interleaveAdd;
        int count;
        int i;
        void* main() {
            bool stopPushing = false;
            for ( int i = 0; i < add; ++i ) {
                queue->push( i );
            }
            queue->flush();
            queue->termination.sync();
            while ( !queue->termination.isZero() ) {

                if ( queue->empty() ) {
                    queue->termination.sync();
                    continue;
                }

                if ( i < interleaveAdd ) {
                    queue->push( i );
                    ++i;
                }

                if ( !stopPushing && i == interleaveAdd ) {
                    queue->flush();
                    stopPushing = true;
                }

                queue->pop_front();
                ++count;
            }
            return nullptr;
        }

        Worker() : queue(), add( 0 ), count( 0 ), i( 0 )
        {}

    };

    template< typename N >
    struct DummyGraph {
        typedef N Node;

        struct Label {
            short probability;
            Label( short p = 0 ) : probability( p ) {}
        };
    };

    Test sharedQueueMultiStress() {

        const int threads = 4;
        const int amount = 32 * 1024;

        typedef SharedQueue< DummyGraph< int >, NoStatistics > Queue;
        Queue::TerminatorPtr t = std::make_shared< Queue::Terminator >();
        Queue::ChunkQPtr ch = std::make_shared< Queue::ChunkQ >();
        DummyGraph< int > g;

        Worker< Queue >* workers = new Worker< Queue >[ threads ];

        std::size_t shouldBe = 0;

        for ( int i = 0;  i < threads; ++i ) {
            workers[ i ].queue = std::make_shared< Queue >( ch, g, t );
            shouldBe += workers[ i ].add = amount * (1 + i);
            workers[ i ].interleaveAdd = amount;
            workers[ i ].start();
        }

        std::size_t sum = 0;
        for ( int i = 0; i < threads; ++i) {
            workers[ i ].join();
            sum += workers[ i ].count;
            shouldBe+= workers[ i ].i;
        }

        assert_eq( sum, shouldBe );
        delete[] workers;
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
