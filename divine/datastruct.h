// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename T, typename Enable = wibble::Unit >
struct HasCircular {
    static const bool value = false;
};

template< typename T >
struct HasCircular< T, typename T::CircularSupport > {
    static const bool value = true;
};

template< typename G, typename C1, typename C2 >
typename wibble::EnableIf< HasCircular< G > >::T
fillCircular( G &g, C1 &in, C2 &out ) {
    g.fillCircular( in, out );
    return wibble::Unit();
}

template< typename G, typename C1, typename C2 >
typename wibble::EnableIf< wibble::TNot< HasCircular< G > > >::T
fillCircular( G &g, C1 &in, C2 &out )
{
    assert_eq( out.space(), out.size() );
    while ( !in.empty() ) {
        int i = 0;
        typename G::Successors s = g.successors( in[ 0 ] );
        while ( !s.empty() ) {
            if ( out.space() < 2 ) {
                out.unadd( i );
                return wibble::Unit();
            }
            out.add( s.from() );
            out.add( s.head() );
            s = s.tail();
            i += 2;
        }
        in.drop( 1 );
    }
    return wibble::Unit();
}

template< typename T, int _size >
struct Circular {
    int __size, // for non-C++ access
        _count, _first;
    T items[ _size ];

    T *start() { return items; }
    T *nth( int i ) { return items + (_first + i) % _size; }
    T *first() { return nth( 0 ); }
    T *last() { return nth( _count - 1 ); }
    T &operator[]( int i ) { return *nth( i ); }

    void add( T t ) {
        assert( _count < _size );
        ++ _count;
        *last() = t;
    }

    int count() { return _count; }
    int space() { return _size - _count; }
    int size() { return _size; }
    bool full() { return _count == _size; }
    bool empty() { return !_count; }

    void drop( int n ) {
        assert( n <= _count );
        _first += n;
        _count -= n;
        _first %= _size;
    }

    void unadd( int n ) {
        _count -= n;
    }

    Circular() : __size( _size ), _count( 0 ), _first( 0 ) {}
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

    Node nextFinished() { return m_finished.front(); }
    void popFinished() { m_finished.pop_front(); }
    bool finishedEmpty() { return m_finished.empty(); }

    bool empty() {
        checkHead();
        return m_head.empty() && m_queue.empty();
    }

    Queue( Graph &_g ) : g( _g ) {}
};

template< typename Graph >
struct BufferedQueue {
    typedef typename Graph::Node Node;
    Circular< Node, 64 > m_in;
    Circular< Node, 512 > m_out;
    std::deque< Node > m_queue;
    std::deque< Node > m_finished;
    Graph &g;

    void pushSuccessors( const Node &t )
    {
        if ( !m_in.full() ) {
            m_in.add( t );
        } else {
            m_queue.push_back( t );
        }
    }

    void checkFilled() {
        if ( m_out.count() >= 2 )
            return;
        while ( !m_in.full() && !m_queue.empty() ) {
            m_in.add( m_queue.front() );
            m_queue.pop_front();
        }
        fillCircular( g, m_in, m_out );
    }

    std::pair< Node, Node > next() {
        checkFilled();
        Node a = m_out[0],
             b = m_out[1];
        return std::make_pair( a, b );
    }

    bool empty() {
        checkFilled();
        return m_out.empty();
    }

    void pop() {
        checkFilled();
        m_out.drop( 2 );
        checkFilled();
    }

    Node nextFinished() { return m_finished.front(); }
    void popFinished() { m_finished.pop_front(); }
    bool finishedEmpty() { return m_finished.empty(); }

    BufferedQueue( Graph &_g ) : g( _g ) {}
};

template< typename Graph >
struct Stack {
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< typename Graph::Successors > m_stack;
    std::deque< Node > m_finished;

    int pushes, pops;

    void pushSuccessors( Node t ) {
        m_stack.push_back( g.successors( t ) );
        typename Graph::Successors s = g.successors( t );
        while ( !s.empty() ) {
            ++ pushes;
            s = s.tail();
        }
    }

    void checkTop() {
        while ( !m_stack.empty() && m_stack.back().empty() ) {
            m_finished.push_back( m_stack.back().from() );
            m_stack.pop_back();
        }
    }

    void pop() {
        checkTop();
        assert( !empty() );
        assert( !m_stack.back().empty() );
        m_stack.back() = m_stack.back().tail();
        checkTop();
        ++ pops;
        assert( pops <= pushes );
    }

    std::pair< Node, Node > next() {
        assert( !empty() );
        checkTop();
        return std::make_pair( m_stack.back().from(), m_stack.back().head() );
    }

    bool empty() {
        checkTop();
        return m_stack.empty();
    }

    Node nextFinished() { return m_finished.front(); }
    void popFinished() { m_finished.pop_front(); }
    bool finishedEmpty() { return m_finished.empty(); }

    Stack( Graph &_g ) : g( _g ) { pushes = pops = 0; }
};

}

#endif
