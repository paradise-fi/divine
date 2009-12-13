// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <fstream>
#include <vector>

#include <divine/hashmap.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
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

enum ExpansionAction { ExpandState, TerminateOnState };

template< typename T >
inline bool alias( T a, T b ) {
    return false;
}

template<> inline bool alias< Blob >( Blob a, Blob b ) {
    return a.ptr == b.ptr;
}

template< typename T > inline bool permanent( T ) { return false; }
template< typename T > inline void setPermanent( T ) {}

template<> inline bool permanent( Blob b ) { return b.header().permanent; }
template<> inline void setPermanent( Blob b ) {
    if ( b.valid() )
        b.header().permanent = 1;
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

    static void finished( Notify &, Node n ) {}

    static TransitionAction transitionHint( Notify &n, Node a, Node b ) {
        return FollowTransition;
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

    void expand( Node n ) {
        if ( seen().has( n ) )
            return;
        exploreFrom( n );
    }

    void exploreFrom( Node _initial ) {
        TransitionAction tact;
        ExpansionAction eact;

        Node initial = seen().insert( _initial ).key;
        setPermanent( initial );
        m_graph.release( _initial );

        if ( S::expansion( m_notify, initial ) == ExpandState )
            m_queue.pushSuccessors( initial );

        processQueue();
    }

    void processQueue() {
        while ( true ) {
            while ( m_queue.finished() ) {
                S::finished( m_notify, m_queue.from() );
                m_queue.popFinished();
            }
            if ( m_queue.empty() )
                return;
            std::pair< Node, Node > c = m_queue.next();
            m_queue.pop();
            edge( c.first, c.second );
        }
    }

    void edge( Node from, Node _to ) {
        TransitionAction tact;
        ExpansionAction eact = ExpandState;

        bool had = true;
        hash_t hint = seen().hash( _to );

        if ( S::transitionHint( m_notify, from, _to ) == IgnoreTransition )
            return;

        Node to = seen().get( _to, hint ).key;

        if ( alias( _to, to ) )
            assert( seen().valid( to ) );

        if ( !seen().valid( to ) ) {
            assert( !alias( _to, to ) );
            had = false;
            to = _to;
        }

        tact = S::transition( m_notify, from, to );
        if ( tact != IgnoreTransition && !had ) {
            seen().insert( to, hint );
            setPermanent( to );
        }

        if ( tact == ExpandTransition ||
             (tact == FollowTransition && !had) ) {
            eact = S::expansion( m_notify, to );
            if ( eact == ExpandState )
                m_queue.pushSuccessors( to );
        }

        if ( tact != IgnoreTransition )
            m_graph.release( _to );

        if ( tact == TerminateOnTransition || eact == TerminateOnState )
            clearQueue();
    }

    void clearQueue() {
        while ( !m_queue.empty() ) {
            std::pair< Node, Node > elem = m_queue.next();
            m_queue.pop();

            m_graph.release( elem.first );
            m_graph.release( elem.second );
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
        Fifo< Blob, NoopMutex > &fifo
            = worker.queue( worker.globalId(), owner( to ) );
        fifo.push( unblob< Node >( from ) );
        fifo.push( unblob< Node >( to ) );
    }

    visitor::TransitionAction transition( Node f, Node t ) {
        visitor::TransitionAction tact = S::transition( notify, f, t );
        if ( tact == TerminateOnTransition )
            worker.interrupt();
        return tact;
    }

    visitor::ExpansionAction expansion( Node n ) {
        assert_eq( owner( n ), worker.globalId() );
        ExpansionAction eact = S::expansion( notify, n );
        if ( eact == TerminateOnState )
            worker.interrupt();
        return eact;
    }

    template< typename BFV >
    void run( BFV &bfv ) {
        worker.restart();
        while ( true ) {
            if ( worker.fifo.empty() ) {
                if ( worker.idle() )
                    return;
            } else {
                if ( worker.interrupted() ) {
                    while ( !worker.idle() ) {
                        while ( !worker.fifo.empty() )
                            worker.fifo.remove();
                    }
                    return;
                }
                Node f, t;
                f = worker.fifo.next();
                worker.fifo.remove();
                t = worker.fifo.next( true );
                worker.fifo.remove();

                bfv.edge( unblob< Node >( f ), unblob< Node >( t ) );
                bfv.processQueue();
            }
        }
    }

    typedef Parallel< S, Worker, _Hash > P;
    struct Ours : Setup< typename S::Graph, P, Seen >
    {
        typedef typename Setup< typename S::Graph, P, Seen >::Notify Notify;
        static TransitionAction transitionHint( Notify &n, Node f, Node t ) {
            if ( n.owner( t ) != n.worker.globalId() ) {
                assert_eq( n.owner( f ), n.worker.globalId() );
                n.queue( f, t );
                return visitor::IgnoreTransition;
            }
            return visitor::FollowTransition;
        }
    };

    void exploreFrom( Node initial ) {
        BFV< Ours > bfv( graph, *this, m_seen );
        if ( owner( initial ) == worker.globalId() ) {
            bfv.exploreFrom( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void processQueue() {
        BFV< Ours > bfv( graph, *this, m_seen );
        run( bfv );
    }

    Parallel( typename S::Graph &g, Worker &w,
              typename S::Notify &n, _Hash h = _Hash(), Seen *s = 0 )
        : worker( w ), notify( n ), graph( g ), hash( h ), m_seen( s )
    {}
};

}
}

#endif
