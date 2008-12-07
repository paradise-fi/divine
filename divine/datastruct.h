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
    Graph &g;

    // TODO this should only be used as a fallback mechanism for graphs that do
    // not implement fillCircular themselves... (therefore it also needs to be
    // moved somewhere to an utility class for graphs)
    template< typename C1, typename C2 >
    void fillCircular( C1 &in, C2 &out ) {
        while ( !in.empty() ) {
            int i = 0;
            typename Graph::Successors s = g.successors( in[ 0 ] );
            while ( !s.empty() ) {
                if ( out.space() < 2 ) {
                    out.unadd( i );
                    return;
                }
                out.add( s.from() );
                out.add( s.head() );
                s = s.tail();
                ++ i;
            }
            in.drop( 1 );
        }
    }

    void pushSuccessors( const Node &t )
    {
        if ( !m_in.full() ) {
            m_in.add( t );
        } else {
            m_queue.push_back( t );
        }
    }

    void fill() {
        while ( !m_in.full() && !m_queue.empty() ) {
            m_in.add( m_queue.front() );
            m_queue.pop_front();
        }
        fillCircular( m_in, m_out );
    }

    std::pair< Node, Node > next() {
        if ( m_out.empty() )
            fill();
        Node a = m_out[0],
             b = m_out[1];
        return std::make_pair( a, b );
    }

    bool empty() {
        return m_out.empty() && m_in.empty() && m_queue.empty();
    }

    void pop() {
        if ( m_out.empty() )
            fill();
        m_out.drop( 2 );
    }

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
