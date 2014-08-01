// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <deque>
#include <memory>

#include <wibble/test.h> // assert

#include <divine/toolkit/lockedqueue.h>
#include <divine/toolkit/shmem.h>

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
            assert( flag == Fresh );
            return data.node;
        }

        Handle handle() {
            assert( flag == Expanded );
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
        g.successors( _from, [&]( Node n, Label l ) {
                ++ this->_pushes;
                this->deadlocked = false;
                this->_stack.push_back( StackItem( n, l ) );
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
    typedef divine::LockedQueue< Chunk > ChunkQ;
    typedef ApproximateCounter Termination;
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
        assert( !incoming.empty() );
        return incoming.front();
    }

    void pop_front() {
        Statistics::global().dequeue( id, sizeof( Handle ) );
        --termination;
        assert( !incoming.empty() );
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

#endif
