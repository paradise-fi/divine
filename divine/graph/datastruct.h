// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>
#include <divine/utility/statistics.h>
#include <divine/toolkit/lockedqueue.h>
#include <divine/graph/store.h>

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename Graph, typename Self >
struct QueueFrontend {
    Self &self() { return *static_cast< Self * >( this ); }
    bool deadlocked;

    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;

    template< typename Next >
    void processOpen( Next next ) {
        deadlocked = true;

        Node from = self().front();
        bool permanent = visitor::permanent( from );
        visitor::setPermanent( from );

        self().g.successors( from, [&]( Node n, Label label ) {
                deadlocked = false;
                next( from, n, label );
            } );

        visitor::setPermanent( from, permanent );
        self().g.release( from ); // Hm.
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && !self().empty() )
            dead( self().front() );
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( !self().empty() ) {
            close( self().front() );
            self().pop_front();
        }
    }

    QueueFrontend() : deadlocked( true ) {}
};

template< typename Graph, typename Statistics >
struct Queue : QueueFrontend< Graph, Queue< Graph, Statistics > >
{
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< Node > _queue;

    int id;

    Node front() { return _queue.front(); }
    void pop_front() { _queue.pop_front(); }

    void reserve( int n ) { _queue.reserve( n ); }
    int size() { return _queue.size(); } // XXX misleading?

    void push( const Node &t )
    {
        Statistics::global().enqueue( id, sizeof( t ) );
        _queue.push_back( t );
    }

    bool empty() { return _queue.empty(); }
    void clear() { _queue.clear(); }

    Queue( Graph &_g ) : g( _g ), id( 0 ) {}
};

template< typename Graph, typename Statistics >
struct Stack {
    Graph &g;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    enum Flag { Fresh, Expanded };

    std::deque< std::pair< Flag, std::pair< Node, Label > > > _stack;
    Node _from;
    bool deadlocked;

    int _pushes, _pops;

    void push( Node t ) {
        _from = t;
        _stack.push_back( std::make_pair( Expanded , std::make_pair( t, Label() ) ) );
        deadlocked = true;
        g.successors( t, [&]( Node n, Label l ) {
                ++ this->_pushes;
                deadlocked = false;
                _stack.push_back( std::make_pair( Fresh, std::make_pair( n, l ) ) );
            } );
    }

    template< typename Next >
    void processOpen( Next next ) {
        if ( !deadlocked ) {
            assert_eq( _stack.back().first, Fresh );
            Node n = _stack.back().second.first;
            Label l = _stack.back().second.second;
            _stack.pop_back();
            ++ _pops;
            next( _from, n, l );
        }
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && ! empty() ) {
            assert_eq( _stack.back().first, Expanded );
            dead( _stack.back().second.first );
            deadlocked = false;
        }
    }

    template< typename Close >
    void processClosed( Close close ) {
        if ( empty() || _stack.back().first != Expanded )
            return;

        while ( !empty() && _stack.back().first == Expanded ) {
            close( _stack.back().second.first );
            _stack.pop_back();
        }

        for ( auto i = _stack.rbegin(); i != _stack.rend(); ++i )
            if ( i->first == Expanded ) {
                _from = i->second.first;
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
template < typename G, typename Statistics >
struct SharedQueue : QueueFrontend< G, SharedQueue< G, Statistics > >
{
    typedef std::deque< typename G::Node > Chunk;
    typedef divine::LockedQueue< Chunk > ChunkQ;
    typedef typename G::Node T;
    typedef G Graph;

    Graph &g;

    const unsigned maxChunkSize;
    unsigned chunkSize;
    ChunkQ *_chunkq;

    Chunk outgoing;
    Chunk incoming;

    ChunkQ &chunkq() { assert( _chunkq ); return *_chunkq; }
    SharedQueue( ChunkQ &q ) : maxChunkSize( 1024 ), chunkSize( 8 ), _chunkq( &q )
    {
	outgoing.reserve( chunkSize );
    }

    ~SharedQueue() { flush(); }

    /**
     * Push current chunk even though it's not full. To be called by threads
     * when the queue is empty.
     */
    void flush() {
	if ( !outgoing.empty() ) {
	    Chunk tmp;
	    tmp.reserve( chunkSize );
	    std::swap( outgoing, tmp );
	    chunkq().push( std::move( tmp ) );

	    /* A quickstart trick -- make first few chunks smaller. */
	    if ( chunkSize < maxChunkSize )
		chunkSize = std::max( 2 * chunkSize, maxChunkSize );
	}
    }

    void push( const T &b ) {
        /* TODO statistics */
	outgoing.push_back( b );
	if ( outgoing.size() >= chunkSize )
	    flush();
    }

    T front() { return incoming.front(); }
    void pop_front() {
        incoming.pop_front();
        if ( incoming.empty() ) /* try to get a fresh one */
            incoming = chunkq().pop();
    }

    bool empty() { return incoming.empty(); }
    void clear() {
        incoming.clear();
        while ( !chunkq().empty )
            chunkq().pop();
    }

    SharedQueue( Graph &_g ) : g( _g ) {}

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
