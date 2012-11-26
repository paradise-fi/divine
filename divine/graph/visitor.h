// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <fstream>
#include <vector>

#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/fifo.h>
#include <divine/toolkit/sharedhashset.h>
#include <divine/toolkit/shmem.h>
#include <divine/graph/datastruct.h>
#include <divine/graph/store.h>

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

struct SetupBase {
    template< typename Listener, typename Node, typename Label >
    static TransitionAction transition( Listener &, Node, Node, Label ) {
        return FollowTransition;
    }

    template< typename Listener, typename Node >
    static ExpansionAction expansion( Listener &, Node ) {
        return ExpandState;
    }

    template< typename Listener, typename Node >
    static void finished( Listener &, Node ) {}

    template< typename Listener, typename Node >
    static DeadlockAction deadlocked( Listener &, Node ) {
        return IgnoreDeadlock;
    }

    template< typename Listener, typename Node, typename Label >
    static TransitionAction transitionHint( Listener &, Node, Node, Label, hash_t ) {
        return FollowTransition;
    }
};

template< typename A, typename BListener >
struct SetupOverride : A {
    typedef typename A::Node Node;
    typedef typename A::Label Label;
    typedef std::pair< typename A::Listener *, BListener * > Listener;

    static TransitionAction transition( Listener &l, Node a, Node b, Label label ) {
        return A::transition( *l.first, a, b, label );
    }

    static ExpansionAction expansion( Listener &l, Node x ) {
        return A::expansion( *l.first, x );
    }

    static void finished( Listener &l, Node n ) {
        A::finished( *l.first, n );
    }

    static DeadlockAction deadlocked( Listener &l, Node x ) {
        return A::deadlocked( *l.first, x );
    }

    template< typename Listener, typename Node >
    static TransitionAction transitionHint( Listener &l, Node a, Node b, Label label, hash_t h ) {
        return A::transitionHint( *l.first, a, b, label, h );
    }
};


template<
    template< typename, typename > class _Queue, typename S >
struct Common {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Label Label;
    typedef typename S::Listener Listener;
    typedef typename S::Store Store;
    typedef typename S::Statistics Statistics;
    typedef _Queue< Graph, Statistics > Queue;

    Graph &graph;
    Listener &notify;
    Store &store;
    Queue _queue;

    void expand( Node n ) {
        if ( store.has( n ) )
            return;
        exploreFrom( n );
    }

    void exploreFrom( Node _initial ) {
        queue( Node(), _initial, Label() );
        processQueue();
    }

    void queue( Node from, Node to, Label l ) {
        edge( from, to, l );
    }

    void processQueue() {
        while ( ! _queue.empty() ) {
            _queue.processOpen( [&]( Node f, Node t, Label l ) { this->edge( f, t, l ); } );
            _queue.processDead( [&]( Node n ) {
                    if ( S::deadlocked( notify, n ) == TerminateOnDeadlock )
                        this->terminate();
                } );
            _queue.processClosed( [&]( Node n ) { S::finished( notify, n ); } );
        }
    }

    // process an edge and free both nodes
    void edge( Node from, Node _to, Label label ) {
        TransitionAction tact;
        ExpansionAction eact = ExpandState;

        bool had = true;
        hash_t hint = store.hash( _to );

        if ( S::transitionHint( notify, from, _to, label, hint ) == IgnoreTransition )
            return;

        Node to = store.fetch( _to, hint, &had );

        tact = S::transition( notify, from, to, label );
        if ( tact != IgnoreTransition && !had ) {
            store.store( to, hint );
        }

        if ( tact == ExpandTransition ||
             (tact == FollowTransition && !had) ) {
            eact = S::expansion( notify, to );
            if ( eact == ExpandState )
                _queue.push( graph.clone( to ) );
        }

        if ( tact != IgnoreTransition )
            store.update( to, hint );

        if ( !store.alias( to, _to ) )
            graph.release( _to );

        if ( tact != IgnoreTransition )
            graph.release( to );

        if ( tact == TerminateOnTransition || eact == TerminateOnState )
            this->terminate();
    }

    void terminate() { _queue.clear(); }

    Common( Listener &n, Graph &g, Store &s, Queue q ) :
        graph( g ), notify( n ), store( s ), _queue( q )
    {
    }
};

template< typename S >
struct BFV : Common< Queue, S > {
    typedef Common< Queue, S > Super;
    typedef typename S::Store Store;
    BFV( typename S::Listener &n,
         typename S::Graph &g,
         typename S::Store &s )
        : Common< Queue, S >( n, g, s, typename Super::Queue( g ) ) {}
};

template< typename S >
struct DFV : Common< Stack, S > {
    typedef Common< Stack, S > Super;
    typedef typename S::Store Store;
    DFV( typename S::Listener &n,
         typename S::Graph &g,
         typename S::Store &s )
        : Common< Stack, S >( n, g, s, typename Super::Queue( g ) ) {}
};

template< typename S, typename Worker >
struct Partitioned {
    typedef typename S::Node Node;
    typedef typename S::Label Label;
    typedef typename S::Graph Graph;

    Worker &worker;
    typename S::Listener &notify;
    typename S::Graph &graph;
    typedef typename S::Store Store;
    typedef typename Store::Hasher Hasher;
    typedef typename S::Statistics Statistics;
    typedef Partitioned< S, Worker > This;

    Store &_store;

    int owner( Node n, hash_t hint = 0 ) const {
        return graph.owner( _store.hasher(), worker, n, hint );
    }

    inline void queue( Node from, Node to, Label label, hash_t hint = 0 ) {
        if ( owner( to, hint ) != worker.id() )
            return;
        queueAny( from, to, label, hint );
    }

    inline void queueAny( Node from, Node to, Label label, hash_t hint = 0 ) {
        int _to = owner( to, hint ), _from = worker.id();
        Statistics::global().sent( _from, _to, sizeof(from) + memSize(to) );
        worker.submit( _from, _to, std::make_tuple( unblob< Node >( from ),
                                                    unblob< Node >( to ), label ) );
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
                        auto p = worker.comms().take( from, to );
                        Statistics::global().received(
                            from, to, sizeof( Node ) + memSize( std::get< 1 >( p ) ) );
                        bfv.edge( unblob< Node >( std::get< 0 >( p ) ),
                                  unblob< Node >( std::get< 1 >( p ) ),
                                  std::get< 2 >( p ) );
                    }
                }

                bfv.processQueue();

            } else {
                if ( worker.idle() )
                    return;
            }
        }
    }

    typedef Partitioned< S, Worker > P;
    struct Ours : SetupOverride< S, This >
    {
        typedef typename SetupOverride< S, This >::Listener Listener;
        static inline TransitionAction transitionHint(
            Listener l, Node f, Node t, Label label, hash_t hint )
        {
            This &n = *l.second;
            if ( n.owner( t, hint ) != n.worker.id() ) {
                assert_eq( n.owner( f ), n.worker.id() );
                n.queueAny( f, t, label, hint );
                return visitor::IgnoreTransition;
            }
            return visitor::FollowTransition;
        }
    };

    template< typename T >
    void setIds( T &bfv ) {
        bfv.store.id = &worker;
        bfv._queue.id = worker.id();
    }

    void exploreFrom( Node initial ) {
        BFV< Ours > bfv( *this, graph, _store );
        setIds( bfv );
        if ( owner( initial ) == worker.id() ) {
            bfv.exploreFrom( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void processQueue() {
        auto l = std::make_pair( &notify, this );
        BFV< Ours > bfv( l, graph, _store );
        setIds( bfv );
        run( bfv );
    }

    Partitioned( typename S::Listener &n, Worker &w, Graph &g, Store &s )
        : worker( w ), notify( n ), graph( g ), _store( s )
    {}
};

/*
 * NB! This is work in progress. It is very unlikely it works. Try it only if
 * you are ready for some serious debugging (throw in some gcc errors for fun
 * and profit).
 */

template< typename S, typename Worker >
struct Shared {
    typedef Shared< S, Worker > This;
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Store Store;
    typedef divine::SharedQueue< typename S::Graph, typename S::Statistics > Chunker;
    typedef SharedHashSet< Blob > Table;
    typedef ApproximateCounter Termination;

    Chunker open;
    Store closed;
    Termination::Shared termination_sh;
    Termination termination;

    Worker &worker;
    typename S::Listener &notify;
    typename S::Graph &graph;
    typedef typename S::Statistics Statistics;

    inline void queue( Node from, Node to, hash_t hint = 0 ) {
        assert_die();
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

    template< typename Slave >
    void run( Slave &slave ) {
        worker.restart();
	while ( !termination.isZero() ) {
	    /* Take a whole chunk of work. */
	    auto chunk = open.pop();
	    if ( open.empty() ) {
		/* Whenever queue is empty, we push the current chunk to queue
		 * and synchronize the termination balance, then try again. */
		open.flush();
		termination.sync();
		continue;
	    }

            slave.processQueue();
        }
    }

    template< typename T >
    void setIds( T &bfv ) {
        bfv.store.id = &worker;
        bfv._queue.id = worker.id();
    }

    void exploreFrom( Node initial ) {
        BFV< S > bfv( notify, graph, closed );
        setIds( bfv );
        if ( worker.id() == 0 ) {
            bfv.exploreFrom( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void processQueue() {
        BFV< S > bfv( notify, graph, closed );
        setIds( bfv );
        run( bfv );
    }

    Shared( typename S::Listener &l, Worker &w, Graph &g, Store &s )
        : worker( w ), notify( l ), graph( g ), closed( s ), termination( termination_sh )
    {}
};

}
}

#endif
