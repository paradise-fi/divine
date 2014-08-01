// -*- C++ -*- (c) 2007-2013 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/fifo.h>
#include <divine/toolkit/shmem.h>
#include <divine/graph/datastruct.h>

#ifndef DIVINE_VISITOR_H
#define DIVINE_VISITOR_H

namespace divine {
namespace visitor {

enum class TransitionAction {
    Terminate,
    Expand, // force expansion on the target state
    Follow, // expand the target state if it's
            // not been expanded yet
    Forget  // do not act upon this transition;
            // target state is freed by visitor
};

enum class TransitionFilter { Take, Ignore };

enum class ExpansionAction { Terminate, Expand, Ignore };
enum class DeadlockAction { Terminate, Ignore };

enum class ReadOnly { No, Yes };

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
    static TransitionFilter transitionFilter( Listener &, Vertex, Node, Label, hash64_t ) {
        return TransitionFilter::Take;
    }
};

template< typename _ASetup, typename _BListener >
struct SetupOverride : _ASetup {
    using A = typename _ASetup::Listener;
    using B = _BListener;
    using S = _ASetup;

    typedef typename _ASetup::Node Node;
    typedef typename _ASetup::Label Label;
    typedef typename _ASetup::Store::Vertex Vertex;
    typedef std::pair< A *, B * > Listener;

    static TransitionAction transition( Listener &l, Vertex a, Vertex b, Label label ) {
        return S::transition( *l.first, a, b, label );
    }

    static ExpansionAction expansion( Listener &l, Vertex x ) {
        return S::expansion( *l.first, x );
    }

    static void finished( Listener &l, Vertex n ) {
        S::finished( *l.first, n );
    }

    static DeadlockAction deadlocked( Listener &l, Vertex x ) {
        return S::deadlocked( *l.first, x );
    }

    template< typename Listener, typename Vertex, typename Node >
    static TransitionFilter transitionFilter( Listener &l, Vertex a, Node b, Label label, hash64_t h ) {
        return S::transitionFilter( *l.first, a, b, label, h );
    }
};


template<
    template< typename > class _Queue, typename S, ReadOnly readOnly = ReadOnly::No >
struct Common {
    typedef typename S::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename S::Label Label;
    typedef typename S::Listener Listener;
    typedef typename S::Statistics Statistics;
    typedef typename S::Store Store;

    typedef typename Store::Vertex Vertex;
    typedef typename Store::Handle Handle;

    typedef _Queue< S > Queue;

    Graph &graph;
    Listener &notify;
    Store &_store;
    Queue _queue;

    Pool &pool() { return graph.pool(); }
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

        hash64_t hint;
        hash64_t ownership;
        std::tie( hint, ownership ) = store().hash128( _to );

        if ( S::transitionFilter( notify, from, _to, label, ownership ) == TransitionFilter::Ignore )
            return;

        auto to = readOnly == ReadOnly::Yes
                    ? store().fetch( _to, hint )
                    : store().store( _to, hint );
        assert( readOnly == ReadOnly::No || !to.isnew() );
        assert( store().valid( *to ) );

        tact = S::transition( notify, from, *to, label );

        /**
         * If this thread attempted to store the node and the node has been already stored before,
         * this node CANNOT be pushed into the working queue to be processed.
         */
        if ( tact == TransitionAction::Expand ||
             ( tact == TransitionAction::Follow && to.isnew() ) ) {
            eact = S::expansion( notify, *to );
            if ( eact == ExpansionAction::Expand )
                _queue.push( to->handle() );
        }

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
        : Super( n, g, s, typename Super::Queue( g, s ) ) {}
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
        : Super( l, g, s, typename Super::Queue( g, s, ch, t ) ) {}

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
        : Super( n, g, s, typename Super::Queue( g, s ) ) {}
};

template< typename S >
struct DFVReadOnly : Common< Stack, S, ReadOnly::Yes > {
    typedef Common< Stack, S, ReadOnly::Yes > Super;
    typedef typename S::Store Store;
    DFVReadOnly( typename S::Listener &n,
         typename S::Graph &g,
         typename S::Store &s )
        : Super( n, g, s, typename Super::Queue( g, s ) ) {}
};

template< typename Override >
struct Interruptible : Override
{
    using Listener = typename Override::Listener;
    using Label = typename Override::S::Label;
    using Vertex = typename Override::S::Store::Vertex;

    static visitor::TransitionAction transition( Listener &l, Vertex f, Vertex t, Label label )
    {
        auto tact = Override::transition( l, f, t, label );
        if ( tact == TransitionAction::Terminate )
            l.second->worker.interrupt();
        return tact;
    }

    static visitor::ExpansionAction expansion( Listener &l, Vertex n )
    {
        auto eact = Override::expansion( l, n );
        if ( eact == ExpansionAction::Terminate )
            l.second->worker.interrupt();
        return eact;
    }
};

struct Partitioned {
    template< typename S >
    struct Data {
        void create() {}
    };

    template< typename S, typename Worker >
    struct Implementation
    {
        typedef typename S::Node Node;
        typedef typename S::Label Label;
        typedef typename S::Graph Graph;

        Worker &worker;
        typename S::Listener &notify;
        typename S::Graph &graph;
        typedef typename S::Store Store;
        typedef typename Store::Hasher Hasher;
        typedef typename Store::Vertex Vertex;
        typedef typename Store::Handle Handle;
        typedef typename S::Statistics Statistics;
        typedef Implementation< S, Worker > This;

        Store &_store;
        Store &store() { return _store; }

        Pool &pool() {
            return graph.pool();
        }

        inline void queue( Vertex from, Node to, Label label, hash64_t hint = 0 ) {
            if ( store().owner( to, hint ) != worker.id() )
                return;
            bfv.edge( from, to, label ); //, hint );
        }

        inline void queueAny( Vertex from, Node to, Label label, hash64_t hint = 0 ) {
            int _to = store().owner( to, hint ), _from = worker.id();
            Statistics::global().sent( _from, _to, sizeof(from) + memSize( to, pool() ) );
            worker.submit( _from, _to, std::make_tuple( from, to, label ) );
        }

        template< typename BFV >
        void run( BFV &bfv ) {
            worker.restart();
            while ( true ) {
                Statistics::global().busy( worker.id() );
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
                            auto f = std::get< 0 >( p );
                            f.initForeign( store() );
                            f.setPool( graph.pool() );
                            bfv.edge( f,
                                      std::get< 1 >( p ),
                                      std::get< 2 >( p ) );
                            f.deleteForeign();
                        }
                    }

                } else if ( !bfv._queue.empty() )
                    bfv.processQueue( 64 );
                else {
                    Statistics::global().idle( worker.id() );
                    if ( worker.idle() )
                        return;
                }
            }
        }

        struct Ours : Interruptible< SetupOverride< S, This > >
        {
            typedef typename SetupOverride< S, This >::Listener Listener;

            static inline TransitionFilter transitionFilter(
                 Listener l, Vertex f, Node t, Label label, hash64_t hint )
            {
                This &n = *l.second;
                if ( n._store.owner( t, hint ) != n.worker.id() ) {
                    if (n._store.valid( f ) )
                        assert_eq( n.store().owner( f ), n.worker.id() );
                    n.queueAny( f, t, label, hint );
                    return TransitionFilter::Ignore;
                }
               return TransitionFilter::Take;
            }
        };

        std::pair< typename S::Listener*, This* > bfvListener;
        BFV< Ours > bfv;

        template< typename T >
        void setIds( T &bfv ) {
            bfv._store.setId( worker );
            bfv._queue.id = worker.id();
        }

        void processQueue() {
            setIds( bfv );
            run( bfv );
        }

        Implementation( typename S::Listener &n, Worker &w, Graph &g, Store &s, Data< typename S::AlgorithmSetup > )
            : worker( w ), notify( n ), graph( g ), _store( s ),
              bfvListener( &notify, this ), bfv( bfvListener, graph, _store )
        {}
    };
};

struct Shared {
    template< typename S >
    struct Data {
        typedef divine::SharedQueue< S > Chunker;

        std::shared_ptr< typename Chunker::ChunkQ > chunkq;
        std::shared_ptr< typename Chunker::Terminator > terminator;
        std::shared_ptr< StartDetector::Shared > detector;

        void create() {
            chunkq = std::make_shared< typename Chunker::ChunkQ >();
            terminator = std::make_shared< typename Chunker::Terminator >();
            detector = std::make_shared< StartDetector::Shared >();
        }

        Data() = default;
        Data( const Data & ) = default;
    };

    template< typename S, typename Worker >
    struct Implementation
    {
        typedef Implementation< S, Worker > This;
        typedef typename S::Graph Graph;
        typedef typename S::Node Node;
        typedef typename S::Label Label;
        typedef typename S::Store Store;
        typedef typename S::Statistics Statistics;
        typedef typename S::Vertex Vertex;

        typedef divine::SharedQueue< S > Chunker;

        using S2 = Interruptible< SetupOverride< S, This > >;

        Store &closed;
        std::pair< typename S::Listener*, This* > bfvListener;
        BFVShared< S2 > bfv;
        StartDetector detector;

        Store& store() {
            return closed;
        }

        Worker &worker;
        typename S::Listener &notify;

        inline void queue( Vertex from, Node to, Label label ) {
            setIds();
            bfv.queue( from, to, label );
        }

        inline void queueAny( Vertex from, Node to, Label label ) {
            queue( from, to, label );
        }

        void run() {
            worker.restart();
            detector.waitForAll( worker.peers() );
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
                if ( worker.interrupted() ) {
                    bfv.terminate();
                    break;
                }
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
            : closed( s ), bfvListener( &l, this ), bfv( bfvListener, g, s, d.chunkq, d.terminator ),
              detector( *d.detector ), worker( w ), notify( l )
        {}
    };
};

}
}
#endif
