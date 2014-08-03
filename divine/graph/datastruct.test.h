// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <brick-shmem.h>
#include <wibble/test.h>
#include <divine/graph/datastruct.h>
#include <divine/algorithm/common.h> // Hasher
#include <divine/generator/dummy.h>

using namespace divine;

struct TestDatastruct {
#if 0
    typedef Blob Node;
    typedef generator::Dummy::Label Label;

    struct SeqSetup {
        typedef generator::Dummy Graph;
        typedef NoStatistics Statistics;
        typedef divine::visitor::Store< divine::visitor::PartitionedTable,
                Graph, divine::algorithm::Hasher, Statistics > Store;
        typedef typename Store::Vertex Vertex;
        typedef typename Store::VertexId VertexId;
        typedef typename Store::QueueVertex QueueVertex;
    };

    Node first, second, third;
    void init( generator::Dummy &g ) {
        int count = 0;
        g.initials( [&]( Node, Node n, Label ) { first = n; } );
        g.successors( first, [&]( Node n, Label ) {
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
        typedef typename SeqSetup::Vertex Vertex;
        auto getShort = [&d]( Node n, int p ) { return d.pool().template get< short >( n, p ); };
        int count = 0;

        init( d );

        assert( q.empty() );
        q.push( Vertex ( first ) );
        assert( !q.empty() );

        count = 0;
        q.processOpen( [&]( Vertex, Node, Label ) { ++count; } );
        q.processClosed( [&]( Vertex ) {} );
        assert_eq( count, 2 );
        assert( q.empty() );

        count = 0;
        q.push( Vertex( first ) );
        q.push( Vertex( second ) );
        assert( !q.empty() );

        q.processOpen( [&]( Vertex, Node n, Label ) {
                if ( count == 0 ) {
                    assert_eq( getShort( n, 0 ), 1 );
                    assert_eq( getShort( n, 2 ), 0 );
                    assert( d.pool().equal( n, second ) );
                }
                if ( count == 1 ) {
                    assert_eq( getShort( n, 0 ), 0 );
                    assert_eq( getShort( n, 2 ), 1 );
                    assert( d.pool().equal( n, third ) );
                }
                ++ count;
            } );
        q.processClosed( [&]( Vertex ) {} );

        assert_eq( count, 2 );
        assert( !q.empty() );

        count = 0;
        q.processOpen( [&]( Vertex, Node n, Label ) {
                if ( count == 0 ) {
                    assert_eq( getShort( n, 0 ), 2 );
                    assert_eq( getShort( n, 2 ), 0 );
                }
                if ( count == 1 ) {
                    assert_eq( getShort( n, 0 ), 1 );
                    assert_eq( getShort( n, 2 ), 1 );
                }
                ++ count;
            } );
        q.processClosed( [&]( Vertex ) {} );
        assert_eq( count, 2 );
        assert( q.empty() );
    }

    NoTest queue() {
        generator::Dummy d;
        Queue< SeqSetup > q( d );
        d.setPool( Pool() );
        _queue( d, q );
    }

    template < typename G >
    struct SharedSetup {
        typedef G Graph;
        typedef visitor::Store< visitor::SharedTable, G,
                algorithm::Hasher, NoStatistics > Store;
        typedef typename Store::Vertex Vertex;
        typedef typename Store::VertexId VertexId;
        typedef NoStatistics Statistics;
        typedef typename Store::QueueVertex QueueVertex;
    };

    NoTest sharedQueue() {
        generator::Dummy d;
        typedef SharedQueue< SharedSetup< generator::Dummy > > Queue;
        Queue::TerminatorPtr t = std::make_shared< Queue::Terminator >();
        Queue::ChunkQPtr ch = std::make_shared< Queue::ChunkQ >();

        d.setPool( Pool() );
        Queue q( ch, d, t );
        q.maxChunkSize = 1;
        q.chunkSize = 1;
        _queue( d, q );
    }

    template< typename Queue >
    struct Worker : brick::shmem::Thread
    {
        typedef typename Queue::QueueVertex QueueVertex;

        std::shared_ptr< Queue > queue;
        int add;
        int interleaveAdd;
        int count;
        int i;
        void main() {
            bool stopPushing = false;
            for ( int i = 0; i < add; ++i ) {
                queue->push( QueueVertex( i ) );
            }
            queue->termination.sync();
            while ( !queue->termination.isZero() ) {

                if ( queue->empty() ) {
                    queue->flush();
                    queue->termination.sync();
                    continue;
                }

                if ( i < interleaveAdd ) {
                    queue->push( QueueVertex( i ) );
                    ++i;
                }

                if ( !stopPushing && i == interleaveAdd ) {
                    stopPushing = true;
                }

                queue->pop_front();
                ++count;
            }
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

    NoTest sharedQueueMultiStress() {
        const int threads = 4;
        const int amount = 32 * 1024;

        typedef SharedQueue< SharedSetup< DummyGraph< int > > > Queue;
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

    NoTest stack() {
        typedef typename SeqSetup::Vertex Vertex;
        generator::Dummy d;
        Stack< SeqSetup > q( d );
        bool die = true;
        auto getShort = [&d]( Node f, int p ) { return d.pool().get< short >( f, p ); };

        d.setPool( Pool() );
        init( d );

        assert( q.empty() );
        q.push( Vertex( first ) );
        q.processClosed( []( Vertex ) { assert_die(); } );
        assert( !q.empty() );

        q.processOpen( [&]( Vertex, Node, Label ) { die = false; } );
        q.processClosed( []( Vertex ) { assert_die(); } );
        assert( !q.empty() );
        assert( !die );

        die = true;
        q.processOpen( [&]( Vertex, Node, Label ) { die = false; } );
        assert( !die );

        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        assert( !die );
        assert( q.empty() );

        q.push( Vertex( first ) );
        q.processClosed( []( Vertex ) { assert_die(); } );
        q.push( Vertex( second ) );

        // 1, 1, from 1, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                assert_eq( getShort( f.getNode(), 0 ), 1 );
                assert_eq( getShort( f.getNode(), 2 ), 0 );
                assert_eq( getShort( t, 0 ), 1 );
                assert_eq( getShort( t, 2 ), 1 );
            } );
        q.processClosed( []( Vertex ) { assert_die(); } );
        assert( !q.empty() );

        // 2, 0, from 1, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                assert_eq( getShort( f.getNode(), 0 ), 1 );
                assert_eq( getShort( f.getNode(), 2 ), 0 );
                assert_eq( getShort( t, 0 ), 2 );
                assert_eq( getShort( t, 2 ), 0 );
            } );
        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        assert( !die );
        assert( !q.empty() );

        // 0, 1, from 0, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                assert_eq( getShort( f.getNode(), 0 ), 0 );
                assert_eq( getShort( f.getNode(), 2 ), 0 );
                assert_eq( getShort( t, 0 ), 0 );
                assert_eq( getShort( t, 2 ), 1 );
            } );
        assert( !q.empty() );

        // 1, 0, from 0, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                assert_eq( getShort( f.getNode(), 0 ), 0 );
                assert_eq( getShort( f.getNode(),  2 ), 0 );
                assert_eq( getShort( t, 0 ), 1 );
                assert_eq( getShort( t, 2 ), 0 );
            } );

        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        assert( !die );

        assert( q.empty() );
    }
#endif
};
