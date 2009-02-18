// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <fstream>
#include <vector>

#include <divine/hashmap.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
#include <divine/generator.h>
#include <divine/datastruct.h>

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

template< typename T > bool disposable( T ) { return false; }
template< typename T > void setDisposable( T ) {}

template<> bool disposable( Blob b ) { return b.header().disposable; }
template<> void setDisposable( Blob b ) {
    if ( b.valid() )
        b.header().disposable = 1;
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

template<
    template< typename > class Queue, typename S >
struct Common {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Notify Notify;
    typedef typename Graph::Successors Successors;
    typedef typename S::Seen Seen;
    Graph &m_graph;
    Notify &m_notify;
    Seen *m_seen;
    Queue< Graph > m_queue;

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
        m_queue.pushSuccessors( initial );
        visit();
    }

    void visit() {
        while ( !m_queue.empty() ) {
            std::pair< Node, Node > c = m_queue.next();
            m_queue.pop();
            visit( c.first, c.second );
            while ( !m_queue.finishedEmpty() ) {
                Node n = m_queue.nextFinished();
                if ( disposable( n ) )
                    m_graph.release( n );
                m_queue.popFinished();
            }
        }
    }

    void visit( Node from, Node _to ) {
        TransitionAction tact;
        ExpansionAction eact;

        bool had = true;
        hash_t hint = seen().hash( _to );
        Node to = seen().get( _to, hint ).key;

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
                seen().insert( to, hint );
            eact = S::expansion( m_notify, to );
            if ( eact == ExpandState )
                m_queue.pushSuccessors( to );
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

    Common( Graph &g, Notify &n, Seen *s ) :
        m_graph( g ), m_notify( n ), m_seen( s ), m_queue( g )
    {
        if ( !m_seen )
            m_seen = new Seen();
    }
};

template< typename S >
struct BFV : Common< BufferedQueue, S > {
    typedef typename S::Seen Seen;
    BFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common< BufferedQueue, S >( g, n, s ) {}
};

template< typename S >
struct DFV : Common< Stack, S > {
    typedef typename S::Seen Seen;
    DFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common< Stack, S >( g, n, s ) {}
}; 

template< typename S, typename Worker,
          typename _Hash = divine::hash< typename S::Node > >
struct Parallel {
    typedef typename S::Node Node;

    Worker &worker;
    typename S::Notify &notify;
    typename S::Graph &graph;
    typedef typename S::Seen Seen;

    _Hash hash;
    Seen *m_seen;

    int owner( Node n ) const {
        return hash( n ) % worker.peers();
    }

    void queue( Node from, Node to ) {
        Fifo< Blob > &fifo = worker.queue( owner( to ) );
        MutexLock __l( fifo.writeMutex );
        fifo.push( __l, unblob< Node >( from ) );
        fifo.push( __l, unblob< Node >( to ) );
    }

    visitor::TransitionAction transition( Node f, Node t ) {
        if ( owner( t ) != notify.globalId() ) {
            queue( f, t );
            return visitor::IgnoreTransition;
        }
        return S::transition( notify, f, t );
    }
    
    visitor::ExpansionAction expansion( Node n ) {
        assert_eq( owner( n ), notify.globalId() );
        return S::expansion( notify, n );
    }

    template< typename BFV >
    void run( BFV &bfv ) {
        while ( true ) {
            if ( worker.fifo.empty() ) {
                if ( worker.idle() )
                    return;
            } else {
                Node f, t;
                f = worker.fifo.front();
                worker.fifo.pop();
                t = worker.fifo.front( true );
                worker.fifo.pop();

                bfv.visit( unblob< Node >( f ), unblob< Node >( t ) );
                bfv.visit();
            }
        }
    }

    void visit( Node initial ) {
        // TBD. Tell the graph that this is our thread and it wants to init its
        // allocator. Or something.
        typedef Setup< typename S::Graph,
            Parallel< S, Worker, _Hash >, Seen > Ours;
        BFV< Ours > bfv( graph, *this, m_seen );
        if ( bfv.seen().valid( initial ) &&
             owner( initial ) == notify.globalId() ) {
            bfv.visit( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void visit() {
        // assert( !seen().valid( Node() ) );
        return visit( Node() ); // assuming Node().valid() == false
    }

    Parallel( typename S::Graph &g, Worker &w,
              typename S::Notify &n, _Hash h = _Hash(), Seen *s = 0 )
        : worker( w ), notify( n ), graph( g ), hash( h ), m_seen( s )
    {}
};

}
}

#endif
