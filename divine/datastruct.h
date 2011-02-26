// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // assert
#include <wibble/sfinae.h>
#include <divine/statistics.h>

#include <deque>

#ifndef DIVINE_DATASTRUCT_H
#define DIVINE_DATASTRUCT_H

namespace divine {

template< typename Graph, typename Statistics >
struct Queue {
    Graph &g;
    typedef typename Graph::Node Node;
    std::deque< Node > m_queue;
    typename Graph::Successors m_head;
    bool maybe_deadlock;

    int id;

    void pushSuccessors( const Node &t )
    {
        Statistics::global().enqueue( id );
        m_queue.push_back( t );
    }

    void pop() {
        m_head = m_head.tail();
        maybe_deadlock = false;
        if ( m_head.empty() && !m_queue.empty() )
            dropEmptyHead();
    }

    inline bool deadlocked() {
        if (m_head.empty() && !m_queue.empty() && !maybe_deadlock)
            dropEmptyHead();
        return maybe_deadlock && m_head.empty();
    }

    void removeDeadlocked() {
        assert( m_head.empty() );
        if ( !m_queue.empty() )
            dropEmptyHead();
        else
            maybe_deadlock = false;
    }

    Node nextFrom() {
        return m_head.from();
    }

    void checkHead() {
        while ( m_head.empty() && !m_queue.empty() )
            dropEmptyHead();
    }

    void dropEmptyHead() {
        assert( m_head.empty() && !m_queue.empty() );
        Statistics::global().dequeue( id );
        m_head = g.successors( m_queue.front() );
        maybe_deadlock = true;
        m_queue.pop_front();
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

    Queue( Graph &_g ) : g( _g ), maybe_deadlock( false ), id( 0 ) {}
};

template< typename Graph, typename Statistics >
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

    bool deadlocked() { return false; }
    bool removeDeadlocked() { return false; }
    Node nextFrom() {
        return m_stack.back().from();
    }
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
