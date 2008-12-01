// -*- C++ -*-

#include <fstream>
#include <vector>

#include <divine/hashmap.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
#include <divine/generator.h>

#ifndef DIVINE_VISITOR_H
#define DIVINE_VISITOR_H

namespace divine {
namespace visitor {

enum TransitionAction { ExpandTransition, // force expansion on the target state
                        FollowTransition, // expand the target state if it's
                                          // not been expanded yet
                        ForgetTransition, // do not act upon this transition;
                                          // target state is freed by visitor
                        IgnoreTransition, // pretend the transition does not
                                          // exist; this also means that the
                                          // target state of the transition is
                                          // NOT FREED
                        TerminateOnTransition
};
enum ExpansionAction { ExpandState };

template< typename T >
bool alias( T a, T b ) {
    return false;
}

template<> bool alias< Blob >( Blob a, Blob b ) {
    return a.ptr == b.ptr;
}

template<
    typename G, // graph
    typename N, // notify
    typename S = HashMap< typename G::Node, Unit >,
    TransitionAction (N::*tr)(typename G::Node, typename G::Node) = &N::transition,
    ExpansionAction (N::*exp)(typename G::Node) = &N::expansion >
struct Setup {
    typedef G Graph;
    typedef N Notify;
    typedef S Seen;
    typedef typename Graph::Node Node;
    static TransitionAction transition( Notify &n, Node a, Node b ) {
        return (n.*tr)( a, b );
    }

    static ExpansionAction expansion( Notify &n, Node a ) {
        return (n.*exp)( a );
    }
};

template< typename T >
struct Queue {
    std::deque< T > m_queue;
    void push( const T &t ) { m_queue.push_back( t ); }
    void pop() { m_queue.pop_front(); }
    T &next() { return m_queue.front(); }
    bool empty() { return m_queue.empty(); }
};

template< typename T >
struct Stack {
    std::deque< T > m_stack;
    void push( const T &t ) { m_stack.push_back( t ); }
    void pop() { m_stack.pop_back(); }
    T &next() { return m_stack.back(); }
    bool empty() { return m_stack.empty(); }
};

// the _ suffix is to avoid clashes with the old-style visitors
template<
    template< typename > class Queue, typename S >
struct Common_ {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Notify Notify;
    typedef typename Graph::Successors Successors;
    Queue< Successors > m_queue;
    typedef typename S::Seen Seen;
    Graph &m_graph;
    Notify &m_notify;
    Seen *m_seen;

    Seen &seen() {
        return *m_seen;
    }

    void visit( Node initial ) {
        TransitionAction tact;
        ExpansionAction eact;
        if ( seen().has( initial ) )
            return;
        seen().insert( initial );
        S::expansion( m_notify, initial );
        m_queue.push( m_graph.successors( initial ) );
        visit();
    }

    void visit() {
        while ( !m_queue.empty() ) {
            Successors &s = m_queue.next();
            if ( s.empty() ) {
                // finished with s
                Node hashed = seen().get( s.from() ).key;
                if ( (!seen().valid( hashed )) || !alias( hashed, s.from() ) )
                    m_graph.release( s.from() );
                m_queue.pop();
                continue;
            } else {
                Node current = s.head();
                s = s.tail();

                visit( s.from(), current );
            }
        }
    }

    void visit( Node from, Node _to ) {
        TransitionAction tact;
        ExpansionAction eact;

        bool had = true;
        Node to = seen().get( _to ).key;

        if ( alias( _to, to ) )
            assert( seen().valid( to ) );

        if ( !seen().valid( to ) ) {
            assert( !alias( _to, to ) );
            had = false;
            to = _to;
        }

        tact = S::transition( m_notify, from, to );
        if ( tact == ExpandTransition ||
             (tact == FollowTransition && !had) ) {
            if ( !had )
                seen().insert( to );
            eact = S::expansion( m_notify, to );
            if ( eact == ExpandState )
                m_queue.push( m_graph.successors( to ) );
        }

        if ( tact != IgnoreTransition && had ) {
            // for the IgnoreTransition case, the transition handler
            // is responsible for freeing up the _to state as needed
            assert( had );
            assert( seen().valid( to ) );
            assert( seen().valid( _to ) );
            // we do not want to release a state we are revisiting, that
            // has already been in the table...
            if ( !alias( to, _to ) )
                m_graph.release( _to );
        }
    }

    Common_( Graph &g, Notify &n, Seen *s ) :
        m_graph( g ), m_notify( n ), m_seen( s )
    {
        if ( !m_seen )
            m_seen = new Seen();
    }
};

template< typename S >
struct BFV : Common_< Queue, S > {
    typedef typename S::Seen Seen;
    BFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common_< Queue, S >( g, n, s ) {}
};

template< typename S >
struct DFV : Common_< Stack, S > {
    typedef typename S::Seen Seen;
    DFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common_< Stack, S >( g, n, s ) {}
};

template< typename S, typename Domain >
struct Parallel {
    typedef typename S::Node Node;

    Domain &dom;
    typename S::Notify &notify;
    typename S::Graph &graph;
    typedef typename S::Seen Seen;

    Seen *m_seen;

    int owner( Node n ) const {
        return unblob< int >( n ) % dom.peers();
    }

    void queue( Node from, Node to ) {
        Fifo< Blob > &fifo = dom.queue( owner( to ) );
        MutexLock __l( fifo.writeMutex );
        fifo.push( __l, unblob< Node >( from ) );
        fifo.push( __l, unblob< Node >( to ) );
    }

    visitor::TransitionAction transition( Node f, Node t ) {
        if ( owner( t ) != dom.id() ) {
            queue( f, t );
            return visitor::IgnoreTransition;
        }
        return S::transition( notify, f, t );
    }
    
    visitor::ExpansionAction expansion( Node n ) {
        assert_eq( owner( n ), dom.id() );
        return S::expansion( notify, n );
    }

    template< typename BFV >
    void run( BFV &bfv ) {
        while ( true ) {
            if ( dom.fifo.empty() ) {
                if ( dom.master().m_barrier.idle( &dom ) )
                    return;
            } else {
                Node f, t;
                f = dom.fifo.front();
                dom.fifo.pop();
                t = dom.fifo.front( true );
                dom.fifo.pop();

                bfv.visit( unblob< Node >( f ), unblob< Node >( t ) );
                bfv.visit();
            }
        }
    }

    void visit( Node initial ) {
        typedef Setup< typename S::Graph, Parallel< S, Domain >, Seen > Ours;
        BFV< Ours > bfv( graph, *this, m_seen );
        if ( bfv.seen().valid( initial ) && owner( initial ) == dom.id() ) {
            bfv.visit( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void visit() {
        // assert( !seen().valid( Node() ) );
        return visit( Node() ); // assuming Node().valid() == false
    }

    Parallel( typename S::Graph &g, Domain &d, typename S::Notify &n, Seen *s = 0 )
        : dom( d ), notify( n ), graph( g ), m_seen( s )
    {}
};

}
}

#endif
