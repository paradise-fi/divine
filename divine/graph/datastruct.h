// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <deque>
#include <memory>

#include <brick-assert.h>
#include <brick-shmem.h>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename Setup, typename Self >
struct QueueFrontend {
    Self &self() { return *static_cast< Self * >( this ); }
    bool deadlocked;
    bool axed;

    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::Store::Vertex Vertex;

    template< typename Next >
    void processOpen( Next next ) {
        deadlocked = true;
        axed = false;

        auto from = self().store().vertex( self().front() );
        self().g.successors( from, [&]( Node n, Label label ) {
                this->deadlocked = false;
                if ( !this->axed )
                    next( from, n, label );
            } );
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && !self().empty() ) {
            auto v = self().store().vertex( self().front() );
            dead( v );
        }
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( !self().empty() ) {
            auto qv = self().store().vertex( self().front() );
            close( qv );
            self().pop_front();
        }
    }

    QueueFrontend() : deadlocked( true ) {}
};

template< typename Setup >
struct Queue : QueueFrontend< Setup, Queue< Setup > >
{
    typedef typename Setup::Graph Graph;
    typedef typename Setup::Store Store;
    typedef typename Graph::Node Node;
    typedef typename Setup::Statistics Statistics;
    typedef typename Store::Handle Handle;

    Graph &g;
    Store &s;
    std::deque< Handle > _queue;

    int id;

    Store &store() { return s; }

    Handle front() { return _queue.front(); }
    void pop_front() {
        Statistics::global().dequeue( id, sizeof( Handle ) );
        _queue.pop_front();
    }

    void reserve( int n ) { _queue.reserve( n ); }
    int size() { return _queue.size(); } // XXX misleading?

    void push( Handle t )
    {
        Statistics::global().enqueue( id, sizeof( t ) );
        _queue.push_back( t );
    }

    bool empty() { return _queue.empty(); }
    void clear() { _queue.clear(); this->axed = true; }

    Queue( Graph &g, Store &s ) : g( g ), s( s ), id( 0 ) {}
};

template< typename Setup >
struct Stack {
    typedef typename Setup::Graph Graph;
    typedef typename Setup::Store Store;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Store::Handle Handle;
    typedef typename Store::Vertex Vertex;

    Graph &g;
    Store &s;

    enum Flag { Fresh, Expanded };

    struct StackItem {
        Flag flag;
        Label label;

        Node node() {
            ASSERT_EQ( flag, Fresh );
            return data.node;
        }

        Handle handle() {
            ASSERT_EQ( flag, Expanded );
            return data.handle;
        }

        StackItem() = delete;

        StackItem( Handle handle, Label label )
            : flag( Expanded ), label( label ), data( handle )
        { }

        StackItem( Node node, Label label )
            : flag( Fresh ), label( label ), data( node )
        { }

      private:
        union DataU {
            DataU( Node node ) : node( node ) {}
            DataU( Handle handle ) : handle( handle ) {}
            Node node;
            Handle handle;
        } data;
    };

    std::deque< StackItem > _stack;
    Vertex _from;
    bool deadlocked;

    int _pushes, _pops;

    void push( Handle h ) {
        _from = s.vertex( h );
        _stack.push_back( StackItem( h, Label() ) );
        deadlocked = true;
        g.successors( _from.node(), [&]( Node n, Label l ) {
                ++ this->_pushes;
                this->deadlocked = false;
                this->_stack.push_back( StackItem( n, l ) );
            } );
    }

    template< typename Next >
    void processOpen( Next next ) {
        if ( !deadlocked ) {
            ASSERT_EQ( _stack.back().flag, Fresh );
            Node n = _stack.back().node();
            Label l = _stack.back().label;
            _stack.pop_back();
            ++ _pops;
            next( _from, n, l );
        }
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && ! empty() ) {
            ASSERT_EQ( _stack.back().flag, Expanded );
            dead( s.vertex( _stack.back().handle() ) );
            deadlocked = false;
        }
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( empty() || _stack.back().flag != Expanded )
            return;

        while ( !empty() && _stack.back().flag == Expanded ) {
            close( s.vertex( _stack.back().handle() ) );
            _stack.pop_back();
        }

        for ( auto i = _stack.rbegin(); i != _stack.rend(); ++i )
            if ( i->flag == Expanded ) {
                _from = s.vertex( i->handle() );
                break;
            }
    }

    bool empty() { return _stack.empty(); }
    void clear() { _stack.clear(); }

    Stack( Graph &g, Store &s ) : g( g ), s( s ) { _pushes = _pops = 0; }
};

/**
 * An adaptor over a LockedQueue to lump access into big chunks. Implements a
 * graph-traversal-friendly inteface, same as the above two.
 */
template < typename Setup >
struct SharedQueue : QueueFrontend< Setup, SharedQueue< Setup > >
{
    typedef typename Setup::Graph Graph;
    typedef typename Setup::Statistics Statistics;

    using Store = typename Setup::Store;
    using Handle = typename Store::Handle;

    typedef std::deque< Handle > Chunk;
    typedef brick::shmem::LockedQueue< Chunk > ChunkQ;
    typedef brick::shmem::ApproximateCounter Termination;
    typedef Termination::Shared Terminator;
    typedef std::shared_ptr< Terminator > TerminatorPtr;
    typedef std::shared_ptr< ChunkQ > ChunkQPtr;

    Graph &g;
    Store &s;

    int id;

    unsigned maxChunkSize;
    unsigned chunkSize;
    ChunkQPtr _chunkq;
    Termination termination;

    Chunk outgoing;
    Chunk incoming;

    ChunkQ &chunkq() { return *_chunkq; }
    SharedQueue( Graph &g, Store &s, ChunkQPtr ch, TerminatorPtr t )
        : g( g ), s( s ), id( 0 ), maxChunkSize( 64 ),
          chunkSize( 2 ), _chunkq( ch ), termination( *t )
    {}

    ~SharedQueue() { flush(); }

    Store &store() { return s; }

    /**
     * Push current chunk even though it's not full. To be called by threads
     * when the queue is empty.
     */
    void flush() {
        if ( !outgoing.empty() ) {
            Chunk tmp;
            std::swap( outgoing, tmp );
            chunkq().push( std::move( tmp ) );

            /* A quickstart trick -- make first few chunks smaller. */
            if ( chunkSize < maxChunkSize )
                chunkSize = std::min( 2 * chunkSize, maxChunkSize );
        }
    }

    void push( Handle h ) {
        Statistics::global().enqueue( id, sizeof( Handle ) );
        ++termination;
        outgoing.push_back( h );
        if ( outgoing.size() >= chunkSize )
            flush();
    }

    Handle front() {
        ASSERT( !incoming.empty() );
        return incoming.front();
    }

    void pop_front() {
        Statistics::global().dequeue( id, sizeof( Handle ) );
        --termination;
        ASSERT( !incoming.empty() );
        incoming.pop_front();
    }

    bool empty() {
        if ( incoming.empty() ) /* try to get a fresh one */
            incoming = chunkq().pop();
        return incoming.empty();
    }

    void clear() {
        incoming.clear();
        outgoing.clear();
        chunkq().clear();
        this->axed = true;
    }

    SharedQueue( void ) = delete;
    SharedQueue( const SharedQueue& ) = default;
};

template< typename T >
void safe_delete( T* &ptr ) {
    if ( ptr != NULL ) {
        delete ptr;
        ptr = NULL;
    }
}

template< typename T >
void safe_delete_array( T* &ptr ) {
    if ( ptr != NULL ) {
        delete [] ptr;
        ptr = NULL;
    }
}

}

#include <divine/generator/dummy.h>
#include <divine/utility/statistics.h>
#include <divine/graph/visitor.h>
#include <divine/graph/store.h>

namespace divine_test {

struct TestDatastruct {
    typedef Blob Node;
    typedef generator::Dummy::Label Label;

    struct SeqSetup {
        typedef generator::Dummy Graph;
        typedef NoStatistics Statistics;
        using Store = StoreFor< Graph, PartitionedProvider >;
        typedef typename Store::Handle Handle;
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
        ASSERT_EQ( count, 2 );
    }

    template< typename Q >
    void _queue(generator::Dummy& d, Q& q) {
        typedef typename Q::Vertex Vertex;
        auto getShort UNUSED = [&d]( Node n, int p ) { return d.pool().template get< short >( n, p ); };
        int count = 0;

        init( d );

        ASSERT( q.empty() );
        q.push( TrivialHandle( first, 0 ) );
        ASSERT( !q.empty() );

        count = 0;
        q.processOpen( [&]( Vertex, Node, Label ) { ++count; } );
        q.processClosed( [&]( Vertex ) {} );
        ASSERT_EQ( count, 2 );
        ASSERT( q.empty() );

        count = 0;
        q.push( TrivialHandle( first, 0 ) );
        q.push( TrivialHandle( second, 0 ) );
        ASSERT( !q.empty() );

        q.processOpen( [&]( Vertex, Node n, Label ) {
                if ( count == 0 ) {
                    ASSERT_EQ( getShort( n, 0 ), 1 );
                    ASSERT_EQ( getShort( n, 2 ), 0 );
                    ASSERT( d.pool().equal( n, second ) );
                }
                if ( count == 1 ) {
                    ASSERT_EQ( getShort( n, 0 ), 0 );
                    ASSERT_EQ( getShort( n, 2 ), 1 );
                    ASSERT( d.pool().equal( n, third ) );
                }
                ++ count;
            } );
        q.processClosed( [&]( Vertex ) {} );

        ASSERT_EQ( count, 2 );
        ASSERT( !q.empty() );

        count = 0;
        q.processOpen( [&]( Vertex, Node n, Label ) {
                if ( count == 0 ) {
                    ASSERT_EQ( getShort( n, 0 ), 2 );
                    ASSERT_EQ( getShort( n, 2 ), 0 );
                }
                if ( count == 1 ) {
                    ASSERT_EQ( getShort( n, 0 ), 1 );
                    ASSERT_EQ( getShort( n, 2 ), 1 );
                }
                ++ count;
            } );
        q.processClosed( [&]( Vertex ) {} );
        ASSERT_EQ( count, 2 );
        ASSERT( q.empty() );
    }

    TEST(queue) {
        generator::Dummy d;
        d.setPool( Pool() );

        SeqSetup::Store s( d, 0 );
        Queue< SeqSetup > q( d, s );
        _queue( d, q );
    }

    template < typename G >
    struct SharedSetup {
        typedef G Graph;
        typedef NoStatistics Statistics;
        using Store = StoreFor< Graph, SharedProvider >;
        typedef typename Store::Handle Handle;
    };

    TEST(sharedQueue) {
        generator::Dummy d;
        typedef SharedSetup< generator::Dummy > Setup;
        typedef SharedQueue< Setup > Queue;
        Queue::TerminatorPtr t = std::make_shared< Queue::Terminator >();
        Queue::ChunkQPtr ch = std::make_shared< Queue::ChunkQ >();

        d.setPool( Pool() );

        Setup::Store s( d, 0 );
        Queue q( d, s, ch, t );
        q.maxChunkSize = 1;
        q.chunkSize = 1;
        _queue( d, q );
    }

    template< typename Queue >
    struct Worker : brick::shmem::Thread
    {
        std::shared_ptr< Queue > queue;
        int add;
        int interleaveAdd;
        int count;
        int i;
        void main() {
            bool stopPushing = false;
            for ( int i = 0; i < add; ++i ) {
                queue->push( i );
            }
            queue->termination.sync();
            while ( !queue->termination.isZero() ) {

                if ( queue->empty() ) {
                    queue->flush();
                    queue->termination.sync();
                    continue;
                }

                if ( i < interleaveAdd ) {
                    queue->push( i );
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

/*  TEST(sharedQueueMultiStress) {
        const int threads = 4;
        const int amount = 32 * 1024;

        typedef SharedQueue< SharedSetup< DummyGraph< Int > > > Queue;
        Queue::TerminatorPtr t = std::make_shared< Queue::Terminator >();
        Queue::ChunkQPtr ch = std::make_shared< Queue::ChunkQ >();
        DummyGraph< Int > g;

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

        ASSERT_EQ( sum, shouldBe );
        delete[] workers;
    } */

    TEST(stack) {
        typedef typename SeqSetup::Store::Vertex Vertex;

        generator::Dummy d;
        d.setPool( Pool() );

        SeqSetup::Store s( d, 0 );
        Stack< SeqSetup > q( d, s );

        bool die = true;
        auto getShort UNUSED = [&d]( Node f, int p ) { return d.pool().get< short >( f, p ); };

        d.setPool( Pool() );
        init( d );

        ASSERT( q.empty() );
        q.push( TrivialHandle( first, 0 ) );
        q.processClosed( []( Vertex ) { ASSERT_UNREACHABLE( "unreachable" ); } );
        ASSERT( !q.empty() );

        q.processOpen( [&]( Vertex, Node, Label ) { die = false; } );
        q.processClosed( []( Vertex ) { ASSERT_UNREACHABLE( "unreachable" ); } );
        ASSERT( !q.empty() );
        ASSERT( !die );

        die = true;
        q.processOpen( [&]( Vertex, Node, Label ) { die = false; } );
        ASSERT( !die );

        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        ASSERT( !die );
        ASSERT( q.empty() );

        q.push( TrivialHandle( first, 0 ) );
        q.processClosed( []( Vertex ) { ASSERT_UNREACHABLE( "unreachable" ); } );
        q.push( TrivialHandle( second, 0 ) );

        // 1, 1, from 1, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                ASSERT_EQ( getShort( f.node(), 0 ), 1 );
                ASSERT_EQ( getShort( f.node(), 2 ), 0 );
                ASSERT_EQ( getShort( t, 0 ), 1 );
                ASSERT_EQ( getShort( t, 2 ), 1 );
            } );
        q.processClosed( []( Vertex ) { ASSERT_UNREACHABLE( "unreachable" ); } );
        ASSERT( !q.empty() );

        // 2, 0, from 1, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                ASSERT_EQ( getShort( f.node(), 0 ), 1 );
                ASSERT_EQ( getShort( f.node(), 2 ), 0 );
                ASSERT_EQ( getShort( t, 0 ), 2 );
                ASSERT_EQ( getShort( t, 2 ), 0 );
            } );
        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        ASSERT( !die );
        ASSERT( !q.empty() );

        // 0, 1, from 0, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                ASSERT_EQ( getShort( f.node(), 0 ), 0 );
                ASSERT_EQ( getShort( f.node(), 2 ), 0 );
                ASSERT_EQ( getShort( t, 0 ), 0 );
                ASSERT_EQ( getShort( t, 2 ), 1 );
            } );
        ASSERT( !q.empty() );

        // 1, 0, from 0, 0
        q.processOpen( [&]( Vertex f, Node t, Label ) {
                ASSERT_EQ( getShort( f.node(), 0 ), 0 );
                ASSERT_EQ( getShort( f.node(),  2 ), 0 );
                ASSERT_EQ( getShort( t, 0 ), 1 );
                ASSERT_EQ( getShort( t, 2 ), 0 );
            } );

        die = true;
        q.processClosed( [&]( Vertex ) { die = false; } );
        ASSERT( !die );

        ASSERT( q.empty() );
    }
};

}

#endif
