// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>
#include <divine/circular.h>

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
wibble::Unit fillCircularTedious( G &g, C1 &in, C2 &out )
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
    return fillCircularTedious( g, in, out );
}

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
        while ( m_head.empty() && !m_queue.empty() ) {
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

    void clear() { while ( !empty() ) pop(); }

    bool finished() { return false; }
    void popFinished() {}
    Node from() { return Node(); }

    Queue( Graph &_g ) : g( _g ) {}
};

template< typename Graph >
struct BufferedQueue {
    typedef typename Graph::Node Node;
    Circular< Node, 256 > m_in;
    Circular< Node, 4096 > m_out;
    std::deque< Node > m_queue;
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

    void clear() { while ( !empty() ) pop(); }

    bool finished() { return false; }
    void popFinished() {}
    Node from() { return Node(); }

    BufferedQueue( Graph &_g ) : g( _g ) {}
};

template< typename Graph >
struct Stack {
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< typename Graph::Successors > m_stack;

    int pushes, pops;

    void pushSuccessors( Node t ) {
        m_stack.push_back( g.successors( t ) );
        typename Graph::Successors s = g.successors( t );
        while ( !s.empty() ) {
            ++ pushes;
            s = s.tail();
        }
    }

    void clearFinished() {
        while ( finished() )
            popFinished();
    }

    void popFinished() {
        assert( finished() );
        m_stack.pop_back();
    }

    void pop() {
        clearFinished();
        assert( !empty() );
        assert( !m_stack.back().empty() );
        m_stack.back() = m_stack.back().tail();
        ++ pops;
        assert( pops <= pushes );
    }

    bool finished() {
        return !m_stack.empty() && m_stack.back().empty();
    }

    Node from() {
        assert( !m_stack.empty() );
        return m_stack.back().from();
    }

    std::pair< Node, Node > next() {
        assert( !empty() );
        clearFinished();
        return std::make_pair( m_stack.back().from(), m_stack.back().head() );
    }

    bool empty() {
        clearFinished();
        return m_stack.empty();
    }

    void clear() { while ( !empty() ) pop(); }

    Stack( Graph &_g ) : g( _g ) { pushes = pops = 0; }
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
