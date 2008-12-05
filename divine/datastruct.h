// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename T, int size >
struct Circular {
    T items[ size ];
    int _first, _count;

    T *start() {
        return items;
    }

    T *first() {
        return items + _first;
    }

    T *last() {
        return items + (_first + _count - 1) % size;
    }

    void add( T t ) {
        assert( _count < size );
        ++ _count;
        *last() = t;
    }

    bool full() {
        return _count == size;
    }

    void drop( int n ) {
        assert( n <= _count );
        _first += n;
        _count -= n;
        _first %= size;
    }

    Circular() : _count( 0 ), _first( 0 ) {}
};

template< typename Graph >
struct Queue {
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< Node > m_queue;
    typename Graph::Successors m_head;

    void pushSuccessors( const Node &t )
    {
        m_queue.push_back( t );
    }

    void pop() {
        m_head = m_head.tail();
        checkHead();
    }

    void checkHead() {
        if ( m_head.empty() && !m_queue.empty() ) {
            m_head = g.successors( m_queue.front() );
            m_queue.pop_front();
        }
    }

    std::pair< Node, Node > next() {
        checkHead();
        assert ( !empty() );
        return std::make_pair( m_head.from(), m_head.head() );
    }

    bool empty() {
        checkHead();
        return m_head.empty() && m_queue.empty();
    }

    Queue( Graph &_g ) : g( _g ) {}
};

}

#endif
