// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename T, int size >
struct Circular {
    T items[ size ];
    int _count, _first;

    T *start() { return items; }
    T *nth( int i ) { return items + (_first + i) % size; }
    T *first() { return nth( 0 ); }
    T *last() { return nth( _count - 1 ); }
    T &operator[]( int i ) { return *nth( i ); }

    void add( T t ) {
        assert( _count < size );
        ++ _count;
        *last() = t;
    }

    int space() {
        return size - _count;
    }

    bool full() {
        return _count == size;
    }

    bool empty() {
        return !_count;
    }

    void drop( int n ) {
        assert( n <= _count );
        _first += n;
        _count -= n;
        _first %= size;
    }

    void unadd( int n ) {
        _count -= n;
    }

    Circular() : _count( 0 ), _first( 0 ) {}
};

template< typename Graph >
struct Queue {
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< Node > m_queue;
    std::deque< Node > m_finished;
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
        while ( m_head.empty() && !m_queue.empty() ) {
            m_head = g.successors( m_queue.front() );
            m_finished.push_back( m_queue.front() );
            m_queue.pop_front();
        }
    }

    std::pair< Node, Node > next() {
        checkHead();
        assert ( !empty() );
        return std::make_pair( m_head.from(), m_head.head() );
    }

    Node nextFinished() {
        return m_finished.front();
    }

    void popFinished() {
        m_finished.pop_front();
    }

    bool finishedEmpty() {
        return m_finished.empty();
    }

    bool empty() {
        checkHead();
        return m_head.empty() && m_queue.empty();
    }

    Queue( Graph &_g ) : g( _g ) {}
};

}

#endif
