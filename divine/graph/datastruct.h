// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>
#include <divine/utility/statistics.h>
#include <divine/toolkit/lockedqueue.h>
#include <divine/graph/store.h>
#include <divine/toolkit/shmem.h>

#include <deque>
#include <memory>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename Setup, typename Self >
struct QueueFrontend {
    Self &self() { return *static_cast< Self * >( this ); }
    bool deadlocked;

    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::QueueVertex QueueVertex;

    template< typename Next >
    void processOpen( Next next ) {
        deadlocked = true;

        auto from = self().front().fromQueue( self().pool() );
        self().g.successors( from, [&]( Node n, Label label ) {
                deadlocked = false;
                next( from, n, label );
            } );
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && !self().empty() ) {
            auto v = self().front().fromQueue( self().pool() );
            dead( v );
        }
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( !self().empty() ) {
            auto qv = self().front().fromQueue( self().pool() );
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
    typedef typename Graph::Node Node;
    typedef typename Setup::QueueVertex QueueVertex;
    typedef typename Setup::Statistics Statistics;
    Graph &g;
    std::deque< QueueVertex > _queue;

    int id;

    Pool &pool() {
        return g.pool();
    }

    QueueVertex front() { return _queue.front(); }
    void pop_front() {
        Statistics::global().dequeue( id, sizeof( QueueVertex ) );
        _queue.pop_front();
    }

    void reserve( int n ) { _queue.reserve( n ); }
    int size() { return _queue.size(); } // XXX misleading?

    void push( const QueueVertex &t )
    {
        Statistics::global().enqueue( id, sizeof( t ) );
        _queue.push_back( t );
    }

    bool empty() { return _queue.empty(); }
    void clear() { _queue.clear(); }

    Queue( Graph &_g ) : g( _g ), id( 0 ) {}
};

template< typename Setup >
struct Stack {
    typedef typename Setup::Graph Graph;
    Graph &g;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::Vertex Vertex;

    enum Flag { Fresh, Expanded };

    struct StackItem {
        Flag flag;
        Label label;

        Node node() {
            assert( flag == Fresh );
            return data.node;
        }
        Vertex vertex() {
            assert( flag == Expanded );
            return data.vertex;
        }

        StackItem() = delete;

        StackItem( Vertex vertex, Label label )
            : flag( Expanded ), label( label ), data( vertex )
        { }

        StackItem( Node node, Label label )
            : flag( Fresh ), label( label ), data( node )
        { }

      private:
        union DataU {
            DataU( Node node ) : node( node )
            { }
            DataU( Vertex vertex ) : vertex( vertex )
            { }
            Node node;
            Vertex vertex;
        } data;
    };

    std::deque< StackItem > _stack;
    Vertex _from;
    bool deadlocked;

    int _pushes, _pops;

    Pool &pool() {
        return g.base().alloc.pool();
    }

    void push( Vertex tV ) {
        _from = tV;
        _stack.push_back( StackItem( tV, Label() ) );
        deadlocked = true;
        g.successors( tV, [&]( Node n, Label l ) {
                ++ this->_pushes;
                deadlocked = false;
                _stack.push_back( StackItem( n, l ) );
            } );
    }

    template< typename Next >
    void processOpen( Next next ) {
        if ( !deadlocked ) {
            assert_eq( _stack.back().flag, Fresh );
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
            assert_eq( _stack.back().flag, Expanded );
            dead(_stack.back().vertex() );
            deadlocked = false;
        }
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( empty() || _stack.back().flag != Expanded )
            return;

        while ( !empty() && _stack.back().flag == Expanded ) {
            close( _stack.back().vertex() );
            _stack.pop_back();
        }

        for ( auto i = _stack.rbegin(); i != _stack.rend(); ++i )
            if ( i->flag == Expanded ) {
                _from = i->vertex();
                break;
            }
    }

    bool empty() { return _stack.empty(); }
    void clear() { _stack.clear(); }

    Stack( Graph &_g ) : g( _g ) { _pushes = _pops = 0; }
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
    typedef typename Graph::Node Node;
    typedef typename Setup::Store::QueueVertex QueueVertex;
    typedef std::deque< QueueVertex > Chunk;
    typedef divine::LockedQueue< Chunk > ChunkQ;
    typedef ApproximateCounter Termination;
    typedef Termination::Shared Terminator;
    typedef std::shared_ptr< Terminator > TerminatorPtr;

    typedef std::shared_ptr< ChunkQ > ChunkQPtr;

    Graph &g;

    int id;

    unsigned maxChunkSize;
    unsigned chunkSize;
    ChunkQPtr _chunkq;
    Termination termination;

    Chunk outgoing;
    Chunk incoming;

    Pool &pool() {
        return g.pool();
    }

    ChunkQ &chunkq() { return *_chunkq; }
    SharedQueue( ChunkQPtr ch, Graph& g, TerminatorPtr t ) : g( g ), id( 0 ), maxChunkSize( 1024 ), chunkSize( 8 ), _chunkq( ch ), termination( *t )
    {}

    ~SharedQueue() { flush(); }

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

    void push( const QueueVertex &b ) {
        Statistics::global().enqueue( id, sizeof( QueueVertex ) );
        ++termination;
        outgoing.push_back( b );
        if ( outgoing.size() >= chunkSize )
            flush();
    }

    QueueVertex front() {
        return incoming.front();
    }
    void pop_front() {
        Statistics::global().dequeue( id, sizeof( QueueVertex ) );
        --termination;
        incoming.pop_front();
    }

    bool empty() {
        if ( incoming.empty() ) /* try to get a fresh one */
            incoming = chunkq().pop();
        return incoming.empty();
    }
    void clear() {
        incoming.clear();
        while ( !chunkq().empty )
            chunkq().pop();
    }
    // removed
    SharedQueue( void ) = delete;
    SharedQueue( const SharedQueue& s) = default;

    /* No r-value push because it's not necessary for Blob. */
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

#endif
