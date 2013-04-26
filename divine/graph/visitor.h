// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

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

enum class TransitionAction { Terminate,
                        Expand, // force expansion on the target state
                        Follow, // expand the target state if it's
                                // not been expanded yet
                        Forget  // do not act upon this transition;
                                // target state is freed by visitor
};

enum class TransitionFilter { Take, Ignore };

enum class ExpansionAction { Terminate, Expand, Ignore };
enum class DeadlockAction { Terminate, Ignore };

struct SetupBase {
    template< typename Listener, typename Vertex, typename Label >
    static TransitionAction transition( Listener &, Vertex, Vertex, Label ) {
        return TransitionAction::Follow;
    }

    template< typename Listener, typename Vertex >
    static ExpansionAction expansion( Listener &, Vertex ) {
        return ExpansionAction::Expand;
    }

    template< typename Listener, typename Node >
    static void finished( Listener &, Node ) {}

    template< typename Listener, typename Node >
    static DeadlockAction deadlocked( Listener &, Node ) {
        return DeadlockAction::Ignore;
    }

    template< typename Listener, typename Vertex, typename Node, typename Label >
    static TransitionFilter transitionFilter( Listener &, Vertex, Node, Label, hash_t ) {
        return TransitionFilter::Take;
    }
};

template< typename A, typename BListener >
struct SetupOverride : A {
    typedef typename A::Node Node;
    typedef typename A::Label Label;
    typedef std::pair< typename A::Listener *, BListener * > Listener;
    typedef typename A::Vertex Vertex;
    typedef typename A::VertexId VertexId;

    static TransitionAction transition( Listener &l, Vertex a, Vertex b, Label label ) {
        return A::transition( *l.first, a, b, label );
    }

    static ExpansionAction expansion( Listener &l, Vertex x ) {
        return A::expansion( *l.first, x );
    }

    static void finished( Listener &l, Vertex n ) {
        A::finished( *l.first, n );
    }

    static DeadlockAction deadlocked( Listener &l, Vertex x ) {
        return A::deadlocked( *l.first, x );
    }

    template< typename Listener, typename Vertex, typename Node >
    static TransitionFilter transitionFilter( Listener &l, Vertex a, Node b, Label label, hash_t h ) {
        return A::transitionFilter( *l.first, a, b, label, h );
    }
};


template<
    template< typename > class _Queue, typename S >
struct Common {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Label Label;
    typedef typename S::Listener Listener;
    typedef typename S::Store Store;
    typedef typename S::Statistics Statistics;
    typedef typename S::Vertex Vertex;
    typedef typename S::VertexId VertexId;
    typedef _Queue< S > Queue;

    Graph &graph;
    Listener &notify;
    Store &_store;
    Queue _queue;

    Pool &pool() {
        return graph.pool();
    }

    Store &store() { return _store; }

    void expand( Node n ) {
        if ( store().has( n ) )
            return;
        exploreFrom( n );
    }

    void exploreFrom( Node _initial ) {
        queue( Vertex(), _initial, Label() );
        processQueue();
    }

    void queue( Vertex from, Node to, Label l ) {
        edge( from, to, l );
    }

    void processQueue( int max = 0 ) {
        int i = 0;
        while ( ! _queue.empty() && (!max || i < max) ) {
            _queue.processOpen( [&]( Vertex f, Node t, Label l ) {
                    this->edge( f, t, l );
                } );
            _queue.processDead( [&]( Vertex n ) {
                    if ( S::deadlocked( notify, n ) == DeadlockAction::Terminate )
                        this->terminate();
                } );
            _queue.processClosed( [&]( Vertex n ) {
                    S::finished( notify, n );
                } );
            ++ i;
        }
    }

    // process an edge and free both nodes
    void edge( Vertex from, Node _to, Label label ) {
        TransitionAction tact;
        ExpansionAction eact = ExpansionAction::Expand;

        hash_t hint = store().hash( _to );

        if ( S::transitionFilter( notify, from, _to, label, hint ) == TransitionFilter::Ignore )
            return;

        /**
         * There is an important part of correct behaviour of shared visitor.
         * If you fetch node from store and the node still does not exists
         * you have to know whether any other thread saved this node into the shared hash table
         * before this thread.
         *
         * Note: Only shared store changes `had` value.
         */
        Vertex to;
        bool isnew;
        std::tie( to, isnew ) = store().store( _to, hint ); // we have to update Vertex so it
            // contains permanent information about stored node
        tact = S::transition( notify, from, to, label );

        /**
         * If this thread attempted to store the node and the node has been already stored before,
         * this node CANNOT be pushed into the working queue to be processed.
         */
        if ( tact == TransitionAction::Expand ||
             (tact == TransitionAction::Follow && isnew) ) {
            eact = S::expansion( notify, to );
            if ( eact == ExpansionAction::Expand )
                _queue.push( to.toQueue( pool() ) );
        }

        if ( !store().alias( to.getNode(), _to ) )
            graph.release( _to );

        if ( tact == TransitionAction::Terminate ||
                eact == ExpansionAction::Terminate )
            this->terminate();
    }

    virtual void terminate() { _queue.clear(); }

    Common( Listener &n, Graph &g, Store &s, Queue q ) :
        graph( g ), notify( n ), _store( s ), _queue( q )
    {
    }

    virtual ~Common() {}
};

template< typename S >
struct BFV : Common< Queue, S > {
    typedef Common< Queue, S > Super;
    typedef typename S::Store Store;
    BFV( typename S::Listener &n,
         typename S::Graph &g,
         typename S::Store &s )
        : Super( n, g, s, typename Super::Queue( g ) ) {}
};

template< typename S >
struct BFVShared : Common< SharedQueue, S > {
    typedef Common< SharedQueue, S > Super;
    typedef typename S::Store Store;
    BFVShared( typename S::Listener &l,
               typename S::Graph &g,
               typename S::Store &s,
               typename Super::Queue::ChunkQPtr ch,
               typename Super::Queue::TerminatorPtr t )
              : Super( l, g, s, typename Super::Queue( ch, g, t ) ) {}

    inline typename Super::Queue& open() { return Super::_queue; }
    virtual void terminate() { Super::terminate(); open().termination.reset(); }
};

template< typename S >
struct DFV : Common< Stack, S > {
    typedef Common< Stack, S > Super;
    typedef typename S::Store Store;
    DFV( typename S::Listener &n,
         typename S::Graph &g,
         typename S::Store &s )
        : Super( n, g, s, typename Super::Queue( g ) ) {}
};

struct Partitioned {
    template< typename S >
    struct Data {
    };

    template< typename S, typename Worker >
    struct Implementation {
        typedef typename S::Node Node;
        typedef typename S::Label Label;
        typedef typename S::Graph Graph;
        typedef typename S::Vertex Vertex;
        typedef typename S::VertexId VertexId;

        Worker &worker;
        typename S::Listener &notify;
        typename S::Graph &graph;
        typedef typename S::Store Store;
        typedef typename Store::Hasher Hasher;
        typedef typename S::Statistics Statistics;
        typedef Implementation< S, Worker > This;

        Store &_store;
        Store &store() { return _store; }

        Pool &pool() {
            return graph.pool();
        }

        int owner( Node n, hash_t hint = 0 ) const {
            return _store.owner( worker, n, hint );
        }

        int ownerV( Vertex n ) const {
            return _store.owner( worker, n );
        }

        inline void queue( Vertex from, Node to, Label label, hash_t hint = 0 ) {
            if ( owner( to, hint ) != worker.id() )
                return;
            queueAny( from, to, label, hint );
        }

        inline void queueAny( Vertex from, Node to, Label label, hash_t hint = 0 ) {
            int _to = owner( to, hint ), _from = worker.id();
            Statistics::global().sent( _from, _to, sizeof(from) + memSize( to, pool() ) );
            worker.submit( _from, _to, std::make_tuple( from.toQueue( pool() ),
                                                        to, label ) );
        }

        visitor::TransitionAction transition( Vertex f, Node t ) {
            visitor::TransitionAction tact = S::transition( notify, f, t );
            if ( tact == TransitionAction::Terminate )
                worker.interrupt();
            return tact;
        }

        visitor::ExpansionAction expansion( Vertex n ) {
            assert_eq( ownerV( n ), worker.id() );
            ExpansionAction eact = S::expansion( notify, n );
            if ( eact == ExpansionAction::Terminate )
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
                                from, to, sizeof( Node ) + memSize( std::get< 1 >( p ),
                                    graph.pool() ) );
                            auto fromData = std::get< 0 >( p ).fromQueue( pool() );
                            bfv.edge( fromData,
                                    std::get< 1 >( p ),
                                    std::get< 2 >( p ) );
                        }
                    }

                } else if ( !bfv._queue.empty() )
                    bfv.processQueue( 64 );
                else if ( worker.idle() )
                    return;
            }
        }

        typedef Implementation< S, Worker > P;
        struct Ours : SetupOverride< S, This >
        {
            typedef typename SetupOverride< S, This >::Listener Listener;
            static inline TransitionFilter transitionFilter(
                 Listener l, Vertex f, Node t, Label label, hash_t hint )
            {
                This &n = *l.second;
                if ( n.owner( t, hint ) != n.worker.id() ) {
                    if (n._store.valid( f ) )
                        assert_eq( n.owner( f.getNode() ), n.worker.id() );
                    n.queueAny( f, t, label, hint );
                    return TransitionFilter::Ignore;
                }
               return TransitionFilter::Take;
            }
        };

        template< typename T >
        void setIds( T &bfv ) {
            bfv._store.setId( worker );
            bfv._queue.id = worker.id();
        }

        void exploreFrom( Node initial ) {
            BFV< Ours > bfv( *this, graph, _store );
            setIds( bfv );
            if ( owner( initial ) == worker.id() ) {
                bfv.exploreFrom( initial );
            }
            run( bfv );
        }

        void processQueue() {
            auto l = std::make_pair( &notify, this );
            BFV< Ours > bfv( l, graph, _store );
            setIds( bfv );
            run( bfv );
        }

        Implementation( typename S::Listener &n, Worker &w, Graph &g, Store &s, Data< typename S::AlgorithmSetup > )
            : worker( w ), notify( n ), graph( g ), _store( s )
        {}
    };
};

struct Shared {
    template< typename S >
    struct Data {
        typedef divine::SharedQueue< S > Chunker;
        typedef SharedHashSet< typename S::Graph::Node > Table;

        std::shared_ptr< typename Chunker::ChunkQ > chunkq;
        std::shared_ptr< typename Chunker::Terminator > terminator;

        Data() :
            chunkq( std::make_shared< typename Chunker::ChunkQ >() ),
            terminator( std::make_shared< typename Chunker::Terminator >() )
            {}
        Data( const Data& ) = default;
    };

    template< typename S, typename Worker >
    struct Implementation {
        typedef Implementation< S, Worker > This;
        typedef typename S::Graph Graph;
        typedef typename S::Node Node;
        typedef typename S::Label Label;
        typedef typename S::Store Store;
        typedef typename S::Statistics Statistics;
        typedef typename S::Vertex Vertex;

        typedef divine::SharedQueue< S > Chunker;

        Store closed;
        BFVShared< S > bfv;

        Store& store() {
            return closed;
        }

        Worker &worker;
        typename S::Listener &notify;

        int owner( Node n, hash_t hint = 0 ) const {
            return worker.id();
        }

        inline void queue( Vertex from, Node to, Label label ) {
            setIds();
            bfv.queue( from, to, label );
        }

        inline void queueAny( Vertex from, Node to, Label label ) {
            queue( from, to, label );
        }

        TransitionAction transition( Vertex f, Node t ) {
            visitor::TransitionAction tact = S::transition( notify, f, t );
            if ( tact == TransitionAction::Terminate )
                worker.interrupt();
            return tact;
        }

        ExpansionAction expansion( Vertex n ) {
            assert_eq( owner( n ), worker.id() );
            ExpansionAction eact = S::expansion( notify, n );
            if ( eact == ExpansionAction::Terminate )
                worker.interrupt();
            return eact;
        }

        void run() {
            worker.restart();
            while ( !bfv.open().termination.isZero() ) {
                /* Take a whole chunk of work. */
                if ( bfv.open().empty() ) {
                    /* Whenever queue is empty, we push the current chunk to queue
                    * and synchronize the termination balance, then try again. */
                    bfv.open().flush();
                    bfv.open().termination.sync();
                    continue;
                }

                bfv.processQueue();
            }
        }

        inline void setIds() {
            bfv.store().setId( worker );
            bfv.open().id = worker.id();
        }

        void exploreFrom( Node initial ) {
            setIds();
            if ( worker.id() == 0 ) {
                bfv.exploreFrom( initial );
            }
            run();
        }

        inline void processQueue() {
            setIds();
            run();
        }

        Implementation( typename S::Listener &l, Worker &w, Graph &g, Store& s,
                        Data< typename S::AlgorithmSetup >& d )
            : closed( s ), bfv( l, g, s, d.chunkq, d.terminator ), worker( w ), notify( l )
        {}
    };
};

}
}
#endif
