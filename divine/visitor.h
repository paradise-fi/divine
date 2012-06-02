// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <fstream>
#include <vector>

#include <divine/statistics.h>
#include <divine/hashset.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
#include <divine/datastruct.h>
#include <divine/store.h>

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

enum ExpansionAction { TerminateOnState, ExpandState, IgnoreState };
enum DeadlockAction { TerminateOnDeadlock, IgnoreDeadlock };

template<
    typename G, // graph
    typename N, // notify
    typename S = HashSet< typename G::Node >,
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

    static void finished( Notify &, Node ) {}
    static DeadlockAction deadlocked( Notify &, Node ) { return IgnoreDeadlock; }

    static TransitionAction transitionHint( Notify &, Node, Node, hash_t ) {
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
    Store< Node, Graph, Seen, Statistics > store;
    Queue< Graph, Statistics > m_queue;

    void expand( Node n ) {
        if ( store.has( n ) )
            return;
        exploreFrom( n );
    }

    void exploreFrom( Node _initial ) {
        queue( Node(), _initial );
        processQueue();
    }

    void queue( Node from, Node to ) {
        edge( from, to );
    }

    void processDeadlocks() {
        while ( m_queue.deadlocked() ) {
            Node dead = m_queue.nextFrom();
            auto action = S::deadlocked( m_notify, dead );
            m_queue.removeDeadlocked(); // _dead_ is released by this call
            if ( action == TerminateOnDeadlock )
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

    // process an edge and free both nodes
    void edge( Node from, Node _to ) {
        TransitionAction tact;
        ExpansionAction eact = ExpandState;

        bool had = true;
        hash_t hint = store.hash( _to );

        if ( S::transitionHint( m_notify, from, _to, hint ) == IgnoreTransition )
            return;

        Node to = store.fetchNode( _to, hint, &had );

        tact = S::transition( m_notify, from, to );
        if ( tact != IgnoreTransition && !had ) {
            store.storeNode( to, hint );
        }

        if ( tact == ExpandTransition ||
             (tact == FollowTransition && !had) ) {
            eact = S::expansion( m_notify, to );
            if ( eact == ExpandState )
                m_queue.pushSuccessors( m_graph.clone( to ) );
        }

        if ( tact != IgnoreTransition ) {
            store.notifyUpdate( to, hint );
            m_graph.release( to );
            m_graph.release( from );
        }
        if ( !store.isSame( to, _to ) )
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

    Common( Graph &g, Notify &n, Seen *s, bool hashCompaction ) :
        m_graph( g ), m_notify( n ), store( g, s, hashCompaction ), m_queue( g )
    {
    }
};

template< typename S >
struct BFV : Common< Queue, S > {
    typedef typename S::Seen Seen;
    BFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0, bool hashCompaction = false )
        : Common< Queue, S >( g, n, s, hashCompaction ) {}
};

template< typename S >
struct DFV : Common< Stack, S > {
    typedef typename S::Seen Seen;
    DFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0, bool hashCompaction = false )
        : Common< Stack, S >( g, n, s, hashCompaction ) {}
};

template< typename S, typename Worker,
          typename _Hash = divine::hash< typename S::Node > >
struct Partitioned {
    typedef typename S::Node Node;
    typedef std::pair< Node, Node > NodePair;

    Worker &worker;
    typename S::Notify &notify;
    typename S::Graph &graph;
    typedef typename S::Seen Seen;
    typedef typename S::Statistics Statistics;

    _Hash hash;
    Seen *m_seen;
    bool hashCompaction;

    int owner( Node n, hash_t hint = 0 ) const {
        return graph.owner( hash, worker, n, hint );
    }

    inline void queue( Node from, Node to, hash_t hint = 0 ) {
        if ( owner( to, hint ) != worker.id() )
            return;
        queueAny( from, to, hint );
    }

    inline void queueAny( Node from, Node to, hash_t hint = 0 ) {
        int _to = owner( to, hint ), _from = worker.id();
        Statistics::global().sent( _from, _to, sizeof(from) + memSize(to) );
        worker.submit( _from, _to, NodePair( unblob< Node >( from ),  unblob< Node >( to ) ) );
    }

    visitor::TransitionAction transition( Node f, Node t ) {
        visitor::TransitionAction tact = S::transition( notify, f, t );
        if ( tact == TerminateOnTransition )
            worker.interrupt();
        return tact;
    }

    visitor::ExpansionAction expansion( Node n ) {
        assert_eq( owner( n ), worker.id() );
        ExpansionAction eact = S::expansion( notify, n );
        if ( eact == TerminateOnState )
            worker.interrupt();
        return eact;
    }

    template< typename BFV >
    void run( BFV &bfv ) {
        worker.restart();
        while ( true ) {
            if ( worker.workWaiting() ) {

                int to = worker.id();

                if ( worker.interrupted() ) {
                    while ( !worker.idle() )
                        while ( worker.comms().pending( to ) )
                            worker.comms().take( to );
                    return;
                }

                for ( int from = 0; from < worker.peers(); ++from ) {
                    while ( worker.comms().pending( from, to ) ) {
                        NodePair p;
                        p = worker.comms().take( from, to );
                        Statistics::global().received( from, to, sizeof(p.first) + memSize(p.second) );
                        bfv.edge( unblob< Node >( p.first ), unblob< Node >( p.second ) );
                    }
                }

                bfv.processQueue();

            } else {
                if ( worker.idle() )
                    return;
            }
        }
    }

    typedef Partitioned< S, Worker, _Hash > P;
    struct Ours : Setup< typename S::Graph, P, Seen, Statistics >
    {
        typedef typename Setup< typename S::Graph, P, Seen >::Notify Notify;
        static inline TransitionAction transitionHint( Notify &n, Node f, Node t, hash_t hint ) {
            if ( n.owner( t, hint ) != n.worker.id() ) {
                assert_eq( n.owner( f ), n.worker.id() );
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
        bfv.store.id = worker.id();
        bfv.m_queue.id = worker.id();
    }

    void exploreFrom( Node initial ) {
        BFV< Ours > bfv( graph, *this, m_seen, hashCompaction );
        setIds( bfv );
        if ( owner( initial ) == worker.id() ) {
            bfv.exploreFrom( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void processQueue() {
        BFV< Ours > bfv( graph, *this, m_seen, hashCompaction );
        setIds( bfv );
        run( bfv );
    }

    Partitioned( typename S::Graph &g, Worker &w,
                 typename S::Notify &n, _Hash h = _Hash(), Seen *s = 0,
                 bool hashCompaction = false )
        : worker( w ), notify( n ), graph( g ), hash( h ), m_seen( s ),
          hashCompaction( hashCompaction )
    {}
};

}
}

#endif
