// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>
#include <divine/utility/statistics.h>

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename Graph, typename Statistics >
struct Queue {
    Graph &g;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    std::deque< Node > _queue;

    bool deadlocked;
    int id;

    void reserve( int n ) { _queue.reserve( n ); }
    int size() { return _queue.size(); } // XXX misleading?

    void push( const Node &t )
    {
        Statistics::global().enqueue( id, sizeof( t ) );
        _queue.push_back( t );
    }

    template< typename Next >
    void processOpen( Next next ) {
        deadlocked = true;
        Node from = _queue.front();
        g.successors( from, [&]( Node n, Label label ) {
                deadlocked = false;
                next( from, n, label );
            } );
    }

    template< typename Dead >
    void processDead( Dead dead ) {
        if ( deadlocked && !empty() )
            dead( _queue.front() );
    };

    template< typename Close >
    void processClosed( Close close ) {
        if ( !empty() ) {
            close( _queue.front() );
            _queue.pop_front();
        }
    };

    bool empty() { return _queue.empty(); }
    void clear() { _queue.clear(); }

    Queue( Graph &_g ) : g( _g ), deadlocked( true ), id( 0 ) {}
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
