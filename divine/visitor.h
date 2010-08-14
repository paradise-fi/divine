// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <fstream>
#include <vector>

#include <divine/statistics.h>
#include <divine/hashmap.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
#include <divine/datastruct.h>

#ifndef DIVINE_VISITOR_H
#define DIVINE_VISITOR_H

namespace divine {
namespace visitor {

enum TransitionAction { TerminateOnTransition,
                        ExpandTransition, // force expansion on the target state
                        FollowTransition, // expand the target state if it's
                                          // not been expanded yet
                        ForgetTransition, // do not act upon this transition;
                                          // target state is freed by visitor
                        IgnoreTransition, // pretend the transition does not
                                          // exist; this also means that the
                                          // target state of the transition is
                                          // NOT FREED
};

enum ExpansionAction { TerminateOnState, ExpandState };
enum DeadlockAction { TerminateOnDeadlock, IgnoreDeadlock };

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
    typename _Statistics = NoStatistics,
    TransitionAction (N::*tr)(typename G::Node, typename G::Node) = &N::transition,
    ExpansionAction (N::*exp)(typename G::Node) = &N::expansion >
struct Setup {
    typedef G Graph;
    typedef N Notify;
    typedef S Seen;
    typedef typename Graph::Node Node;
    typedef _Statistics Statistics;

    static TransitionAction transition( Notify &n, Node a, Node b ) {
        return (n.*tr)( a, b );
    }

    static ExpansionAction expansion( Notify &n, Node a ) {
        return (n.*exp)( a );
    }

    static void finished( Notify &, Node n ) {}
    static DeadlockAction deadlocked( Notify &, Node n ) { return IgnoreDeadlock; }

    static TransitionAction transitionHint( Notify &n, Node a, Node b, hash_t hint ) {
        return FollowTransition;
    }

};

template<
    template< typename, typename > class Queue, typename S >
struct Common {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Notify Notify;
    typedef typename Graph::Successors Successors;
    typedef typename S::Seen Seen;
    typedef typename S::Statistics Statistics;
    Graph &m_graph;
    Notify &m_notify;
    Seen *m_seen;
    Queue< Graph, Statistics > m_queue;

    int id;

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

    void processDeadlocks() {
        while ( m_queue.deadlocked() ) {
            Node dead = m_queue.nextFrom();
            m_queue.removeDeadlocked();
            if ( S::deadlocked( m_notify, dead ) == TerminateOnDeadlock )
                return terminate();
        }
    }

    void processFinished() {
            while ( m_queue.finished() ) {
                S::finished( m_notify, m_queue.from() );
                m_queue.popFinished();
            }
    }

    void processQueue() {
        while ( true ) {
            processFinished();
            processDeadlocks();
            if ( m_queue.empty() )
                return;
            std::pair< Node, Node > c = m_queue.next();
            m_queue.pop();
            processDeadlocks();
            edge( c.first, c.second );
        }
    }

    void edge( Node from, Node _to ) {
        TransitionAction tact;
        ExpansionAction eact = ExpandState;

        bool had = true;
        hash_t hint = seen().hash( _to );

        if ( S::transitionHint( m_notify, from, _to, hint ) == IgnoreTransition )
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
            Statistics::global().hashadded( id );
            Statistics::global().hashsize( id, seen().size() );
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
            terminate();
    }

    void terminate() {
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
        m_graph( g ), m_notify( n ), m_seen( s ), m_queue( g ), id( 0 )
    {
        if ( !m_seen )
            m_seen = new Seen();
    }
};

template< typename S >
struct BFV : Common< Queue, S > {
    typedef typename S::Seen Seen;
    BFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common< Queue, S >( g, n, s ) {}
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
    typedef typename S::Statistics Statistics;

    _Hash hash;
    Seen *m_seen;

    int owner( Node n, hash_t hint = 0 ) const {
        if ( !hint )
            return hash( n ) % worker.peers();
        else
            return hint % worker.peers();
    }

    inline void queue( Node from, Node to, hash_t hint = 0 ) {
        if ( owner( to, hint ) != worker.globalId() )
            return;
        queueAny( from, to, hint );
    }

    inline void queueAny( Node from, Node to, hash_t hint = 0 ) {
        int _to = owner( to, hint ), _from = worker.globalId();
        Fifo< Blob > &fifo = worker.queue( _from, _to );
        Statistics::global().sent( _from, _to );
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

                while ( !worker.fifo.empty() ) {
                    Node f, t;
                    f = worker.fifo.next();
                    worker.fifo.remove();
                    t = worker.fifo.next( true );
                    worker.fifo.remove();
                    int from_id = worker.fifo.m_last - 1; // FIXME m_last?
                    if ( from_id < 0 )
                        from_id = owner( f );
                    Statistics::global().received( from_id, worker.globalId() );
                    bfv.edge( unblob< Node >( f ), unblob< Node >( t ) );
                }

                bfv.processQueue();
            }
        }
    }

    typedef Parallel< S, Worker, _Hash > P;
    struct Ours : Setup< typename S::Graph, P, Seen, Statistics >
    {
        typedef typename Setup< typename S::Graph, P, Seen >::Notify Notify;
        static inline TransitionAction transitionHint( Notify &n, Node f, Node t, hash_t hint ) {
            if ( n.owner( t, hint ) != n.worker.globalId() ) {
                assert_eq( n.owner( f ), n.worker.globalId() );
                n.queueAny( f, t, hint );
                return visitor::IgnoreTransition;
            }
            return visitor::FollowTransition;
        }

        static DeadlockAction deadlocked( P &p, Node n ) {
            return S::deadlocked( p.notify, n );
        }
    };

    template< typename T >
    void setIds( T &bfv ) {
        bfv.id = worker.globalId();
        bfv.m_queue.id = worker.globalId();
    }

    void exploreFrom( Node initial ) {
        BFV< Ours > bfv( graph, *this, m_seen );
        setIds( bfv );
        if ( owner( initial ) == worker.globalId() ) {
            bfv.exploreFrom( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void processQueue() {
        BFV< Ours > bfv( graph, *this, m_seen );
        setIds( bfv );
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
