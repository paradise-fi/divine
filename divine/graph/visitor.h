// -*- C++ -*- (c) 2007-2013 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <brick-shmem.h>
#include <divine/toolkit/pool.h>
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
            _queue.processOpen( store().alloc, [&]( Vertex f, Node t, Label l ) {
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
        ASSERT( readOnly == ReadOnly::No || !to.isnew() );
        ASSERT( store().valid( *to ) );

        tact = S::transition( notify, from, *to, label );

        /**
         * If this thread attempted to store the node and the node has been already stored before,
         * this node CANNOT be pushed into the working queue to be processed.
         */
        if ( tact == TransitionAction::Expand ||
             ( tact == TransitionAction::Follow && to.isnew() ) ) {
            eact = S::expansion( notify, *to );
            if ( eact == ExpansionAction::Expand )
                _queue.push( store().alloc, to->handle() );
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
                        ASSERT_EQ( n.store().owner( f ), n.worker.id() );
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
    using StartDetector = brick::shmem::StartDetector;

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

#include <divine/graph/store.h>
#include <divine/utility/statistics.h>

namespace brick_test { namespace hashset {

template<>
struct test_hasher< divine::Blob >
{
    typedef divine::Blob T;
    divine::Pool& _pool;
    divine::Pool &pool() { return _pool; }
    test_hasher( divine::Pool& p ) : _pool( p ) { }
    test_hasher( const test_hasher &o ) : _pool( o._pool ) {}
    hash128_t hash( T t ) const { return _pool.hash( t ); }
    bool valid( T t ) const { return _pool.valid( t ); }
    bool equal( T a, T b ) const { return _pool.equal( a, b ); }
};

} }

namespace divine_test {

using namespace divine;
using namespace visitor;

using brick_test::hashset::test_hasher;

struct Int {
    int i;
    Int( int i ) : i( i ) {}
    Int() : i( 0 ) {}
    operator int() { return i; }
};

template < typename T > struct TestHasher;

template <>
struct TestHasher< Int > : test_hasher< int >
{
    TestHasher( Pool&, int ) { }
};

template<>
struct TestHasher< Blob > : test_hasher< Blob > {
    TestHasher( Pool& p, int ) : test_hasher< Blob >( p ) { }
};

template< typename Store >
int node( const _Vertex< Store > &n, Pool &p ) {
    if ( p.valid( n.node() ) )
        return p.template get< int >( n.node() );
    else return 0;
}

inline int node( Blob n, Pool &p ) { return p.valid( n ) ? p.get< int >( n ) : 0; }
inline int node( const Int &n, Pool& ) { return n.i; }

template< typename N > N makeNode( int, Pool& );
template<> inline Int makeNode< Int >( int n, Pool& ) { return n; }
template<> inline Blob makeNode< Blob >( int n, Pool& p ) {
    Blob b = p.allocate( sizeof( int ) );
    p.get< int >( b ) = n;
    return b;
}

template< typename G, typename Provider >
using StoreFor = visitor::DefaultStore< Provider, G, TestHasher< typename G::Node >, NoStatistics >;

struct TestVisitor {
    typedef void Test_;

    template< typename _Node >
    struct NMTree {
        typedef _Node Node;
        typedef generator::NoLabel Label;
        int n, m;
        Pool p;

        Pool &pool() { return p; }
        Node clone( Node n ) { return n; }
        void release( Blob n ) { p.free( n ); }
        void release( int ) {}
        Node initial() {
            Node n = makeNode< Node >( 1, p );
            return n;
        }

        template< typename Alloc, typename V, typename Yield >
        void successors( Alloc, V from, Yield yield ) {
            if ( n < 0 )
                return;

            for ( int i = 0; i < m; ++i ) {
                int x = m * ( node( from, p ) - 1 ) + i + 1;
                int y = (x >= n ? 0 : x) + 1;
                yield( makeNode< Node >( y, p ), Label() );
                if ( x >= n )
                    break;
            }
        }

        NMTree( int _n, int _m ) : n( _n ), m( _m ), p() {}

        template< typename Hasher, typename Worker >
        int owner( Hasher &hasher, Worker &worker, Node n, hash64_t = 0 ) {
            return hasher.hash( n ) % worker.peers();
        }
    };

    template< typename G, typename Provider >
    using Transition = std::tuple<
        typename StoreFor< G, Provider >::Vertex,
        typename G::Node, typename G::Label
    >;

    template< typename G >
    struct Check : SetupBase {
        typedef typename G::Node Node;
        typedef typename G::Label Label;
        typedef G Graph;
        typedef Check< G > This;
        typedef This Listener;
        typedef NoStatistics Statistics;
        using Store = StoreFor< G, PartitionedProvider >;
        typedef typename Store::Vertex Vertex;
        typedef typename Store::Handle Handle;
        G _graph;
        BlobComparerLT bcomp;
        std::set< Node, BlobComparerLT > seen;
        std::set< std::tuple< Node, Node >, BlobComparerLT > t_seen;
        std::tuple< int, int > counts;

        int &edges() { return std::get< 0 >( counts ); }
        int &nodes() { return std::get< 1 >( counts ); }

        int _edges() { return this->edges(); }
        int _nodes() { return this->nodes(); }

        template< typename Self >
        static TransitionAction transition( Self &c, Vertex fV, Vertex tV, Label ) {
            Node f = fV.node();
            Node t = tV.node();
            if ( node( f, c._graph.pool() ) ) {
                ASSERT( c.seen.count( f ) );
                c.edges() ++;
                ASSERT( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        template< typename V >
        static ExpansionAction expansion( This &c, V tV ) {
            Node t = tV.node();
            ASSERT( !c.seen.count( t ) );
            c.seen.insert( t );
            c.nodes() ++;
            return ExpansionAction::Expand;
        }

        Check( Graph &g ) : _graph( g ), bcomp( _graph.pool() ), seen( bcomp ), t_seen( bcomp ),
            counts( std::make_pair( 0, 0 ) )
        { }
        Check( const Check &c )
            : _graph( c._graph ), bcomp( _graph.pool() ), seen( bcomp ), t_seen( bcomp ),
              counts( c.counts )
        {}
    };

    template< typename T, typename Vertex, typename Node, typename Label >
    static TransitionFilter parallel_filter( T *self, Vertex fV, Node t, Label label ) {
        if ( node( t, self->_graph.pool() ) % self->peers() != self->id() ) {
            self->submit( self->id(), node( t, self->_graph.pool() ) % self->peers(),
                          std::make_tuple( fV, t, label ) );
                return TransitionFilter::Ignore;
            }
        return TransitionFilter::Take;
    }

    template< typename T, typename N, typename Label >
    static TransitionAction parallel_transition( T *self, N fV, N tV, Label ) {
        auto f = fV.node();
        auto t = tV.node();

        if ( node( f, self->_graph.pool() ) % self->peers() == self->id() )
            ASSERT( self->seen.count( f ) );

        if ( node( f, self->_graph.pool() ) ) {
            self->edges() ++;
            ASSERT( !self->t_seen.count( std::make_pair( f, t ) ) );
            self->t_seen.insert( std::make_pair( f, t ) );
        }

        return TransitionAction::Follow;
    }

    template< typename G >
    struct ParallelCheck : Parallel< Topology< Transition< G, PartitionedProvider > >::template Local,
                                     ParallelCheck< G > >,
                           Check< G >
    {
        typedef ParallelCheck< G > This;
        typedef This Listener;
        typedef typename G::Node Node;
        typedef typename G::Label Label;
        typedef typename Check< G >::Store Store;
        using Vertex = typename Store::Vertex;
        typedef G Graph;
        Store store;

        Node make( int n ) { return makeNode< Node >( n ); }
        int expected;

        static TransitionAction transition( This &c, Vertex f, Vertex t, Label label ) {
            return parallel_transition( &c, f, t, label );
        }

        static TransitionFilter transitionFilter( This &c, Vertex f, Node t, Label label, hash64_t ) {
            return parallel_filter( &c, f, t, label );
        }

        void _visit() { // parallel
            ASSERT_EQ( expected % this->peers(), 0 );
            ASSERT_LEQ( 0, this->id() );
            ASSERT_LEQ( 0, this->rank() );
            store.setId( *this );
            BFV< ParallelCheck< G > > bfv( *this, this->_graph, store );
            Node initial = this->_graph.initial();
            if ( node( initial, this->_graph.pool() ) % this->peers() == this->id() )
                bfv.exploreFrom( initial );

            while ( this->nodes() != expected / this->peers() ) {
                if ( !this->comms().pending( this->id() ) )
                    continue;

                auto next = this->comms().take( this->id() );
                std::get< 0 >( next ).setPool( this->_graph.pool() );
                ASSERT_EQ( node( std::get< 1 >( next ), this->_graph.pool() ) % this->peers(), this->id() );
                bfv.queue( std::get< 0 >( next ),
                           std::get< 1 >( next ), std::get< 2 >( next ) );
                bfv.processQueue();
            }
        }

        void _finish() { // parallel
            while ( this->comms().pending( this->id() ) ) {
                this->edges() ++;
                this->comms().take( this->id() );
            }
        }

        void visit() {
            std::vector< int > edgevec, nodevec;

            this->topology().parallel( &ParallelCheck< G >::_visit );
            this->topology().parallel( &ParallelCheck< G >::_finish );

            this->topology().collect( edgevec, &ParallelCheck< G >::_edges );
            this->topology().collect( nodevec, &ParallelCheck< G >::_nodes );

            this->edges() = std::accumulate( edgevec.begin(), edgevec.end(), 0 );
            this->nodes() = std::accumulate( nodevec.begin(), nodevec.end(), 0 );
        }

        ParallelCheck( std::pair< G, int > init ) :
            Check< G >( init.first ), store( this->_graph, 0 ), expected( init.second )
        {
            int i = 32;
            while ( expected % i ) i--;
            this->becomeMaster( i );
            this->initSlaves( *this );
        }

        ParallelCheck( ParallelCheck &m, std::pair< int, int > i ) :
            Check< G >( m ), store( this->_graph, 0 ), expected( m.expected )
        {
            this->becomeSlave( m.topology(), i );
        }
    };

    template< typename G >
    struct PartitionCheck : Parallel< Topology< Transition< G, PartitionedProvider > >::template Local,
                                      PartitionCheck< G > >,
                            Check< G >
    {
        typedef PartitionCheck< G > This;
        typedef This Listener;
        typedef This AlgorithmSetup;
        typedef typename G::Node Node;
        typedef typename G::Label Label;
        typedef typename Check< G >::Store Store;
        typedef G Graph;
        using Vertex = typename Store::Vertex;
        Store store;

        Node make( int n ) { return makeNode< Node >( n ); }
        int expected;

        static TransitionAction transition( This &c, Vertex fV, Vertex tV, Label ) {
            Node f = fV.node();
            Node t = tV.node();
            if ( node( f, c._graph.pool() ) ) {
                c.edges() ++;
                ASSERT( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        void _visit() { // parallel
            ASSERT_EQ( expected % this->peers(), 0 );
            store.setId( *this );
            Partitioned::Data< This > data;
            Partitioned::Implementation<This, This> partitioned( *this, *this, this->_graph, store, data );
            partitioned.queue( Vertex(), this->_graph.initial(), Label() );
            partitioned.processQueue();
        }

        void visit() {
            std::vector< int > edgevec, nodevec;

            this->topology().parallel( &PartitionCheck< G >::_visit );

            this->topology().collect( edgevec, &PartitionCheck< G >::_edges );
            this->topology().collect( nodevec, &PartitionCheck< G >::_nodes );

            this->edges() = std::accumulate( edgevec.begin(), edgevec.end(), 0 );
            this->nodes() = std::accumulate( nodevec.begin(), nodevec.end(), 0 );
        }

        PartitionCheck( std::pair< G, int > init ) :
            Check< G >( init.first ), store( this->_graph, 0 ), expected( init.second )
        {
            int i = 32;
            while ( expected % i ) i--;
            this->becomeMaster( i );
            this->initSlaves( *this );
        }

        PartitionCheck( PartitionCheck &m, std::pair< int, int > i ) :
            Check< G >( m ), store( this->_graph, 0 ), expected( m.expected )
        {
            this->becomeSlave( m.topology(), i );
        }
    };

    template< typename G >
    struct SharedCheck : Parallel< Topology< Transition< G, SharedProvider > >::template Local,
                                   SharedCheck< G > >,
           Check< G >
    {
        typedef SharedCheck< G > This;
        typedef This Listener;
        typedef This AlgorithmSetup;
        typedef typename G::Node Node;
        typedef typename G::Label Label;
        typedef NoStatistics Statistics;
        using Store = StoreFor< G, SharedProvider >;
        typedef G Graph;
        typedef typename Store::Vertex Vertex;
        Node make( int n ) { return makeNode< Node >( n ); }
        int expected;

        typename Shared::Data< This > data;
        Store store;

        static TransitionAction transition( This &c, Vertex fV, Vertex tV, Label ) {
            Node f = fV.node();
            Node t = tV.node();
            if ( node( f, c._graph.pool() ) ) {
                c.edges() ++;
                ASSERT( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        void _visit() { // parallel
            store.setId( *this );
            ASSERT_EQ( expected % this->peers(), 0 );
            Shared::Implementation< This, This > shared( *this, *this, this->_graph, store, data );
            if ( !this->id() )
                shared.queue( Vertex(), this->_graph.initial(), Label() );
            shared.processQueue();
        }

        void visit() {
            std::vector< int > edgevec, nodevec;

            this->topology().parallel( &SharedCheck< G >::_visit );

            this->topology().collect( edgevec, &SharedCheck< G >::_edges );
            this->topology().collect( nodevec, &SharedCheck< G >::_nodes );

            this->edges() = std::accumulate( edgevec.begin(), edgevec.end(), 0 );
            this->nodes() = std::accumulate( nodevec.begin(), nodevec.end(), 0 );
        }

        SharedCheck( std::pair< G, int > init ) :
            Check< G >( init.first ),
            expected( init.second ),
            store( this->_graph, 0 )
        {
            int i = 32;
            while ( expected % i ) i--;
            store.setSize( 1024 );
            this->becomeMaster( i );
            data.create();
            this->initSlaves( *this );
        }

        SharedCheck( SharedCheck &m, std::pair< int, int > i ) : SharedCheck( m )
        {
            this->becomeSlave( m.topology(), i );
        }

        SharedCheck( const SharedCheck& ) = default;
    };

    template< typename G >
    struct TerminableCheck : Parallel< Topology< Transition< G, PartitionedProvider > >::template Local,
                                       TerminableCheck< G > >,
                             Check< G >
    {
        typedef TerminableCheck< G > This;
        typedef This Listener;
        typedef typename G::Node Node;
        typedef typename G::Label Label;
        typedef typename Check< G >::Store Store;
        typedef typename Store::Vertex Vertex;
        Store store;

        static TransitionAction transition( This &c, Vertex f, Vertex t, Label label ) {
            return parallel_transition( &c, f, t, label );
        }

        int owner( Node n ) {
            return node( n, this->_graph.pool() ) % this->peers();
        }

        void _visit() { // parallel
            store.setId ( *this );
            BFV< This > bfv( *this, this->_graph, store );

            Node initial = this->_graph.initial();
            if ( owner( initial ) == this->id() )
                bfv.exploreFrom( initial );

            while ( true ) {
                if ( this->comms().pending( this->id() ) ) {
                    auto next = this->comms().take( this->id() );
                    std::get< 0 >( next ).setPool( this->_graph.pool() );
                    ASSERT_EQ( owner( std::get< 1 >( next ) ), this->id() );
                    bfv.queue( std::get< 0 >( next ),
                               std::get< 1 >( next ), std::get< 2 >( next ) );
                    bfv.processQueue();
                } else {
                    if ( this->idle() )
                        return;
                }
            }
        }

        void visit() {
            std::vector< int > edgevec, nodevec;
            this->topology().parallel( &TerminableCheck< G >::_visit );

            this->topology().collect( edgevec, &TerminableCheck< G >::_edges );
            this->topology().collect( nodevec, &TerminableCheck< G >::_nodes );

            this->edges() = std::accumulate( edgevec.begin(), edgevec.end(), 0 );
            this->nodes() = std::accumulate( nodevec.begin(), nodevec.end(), 0 );
        }

        TerminableCheck( std::pair< G, int > init ) :
            Check< G >( init.first ), store( this->_graph, 0 )
        {
            this->becomeMaster( 10 );
            this->initSlaves( *this );
        }

        TerminableCheck( TerminableCheck &m, std::pair< int, int > i ) :
            Check< G >( m ), store( this->_graph, 0 )
        {
            this->becomeSlave( m.topology(), i );
        }
    };

    static void checkNMTreeMetric( int n, int m, int _nodes, int _transitions )
    {
        int fullheight = 1;
        int fulltree = 1;
        while ( fulltree + pow(m, fullheight) <= n ) {
            fulltree += pow(m, fullheight);
            fullheight ++;
        }
        int lastfull = pow(m, fullheight-1);
        int remaining = n - fulltree;
        // remaining - remaining/m is not same as remaining/m (due to flooring)
        int transitions UNUSED = (n - 1) + lastfull + remaining - remaining / m;

        ASSERT_EQ( n, _nodes );
        ASSERT_EQ( transitions, _transitions );
    }

    template< typename F >
    void examples( F f ) {
        f( 7, 2 );
        f( 8, 2 );
        f( 31, 2 );
        f( 4, 3 );
        f( 8, 3 );
        f( 242, 3 );
        f( 245, 3 );
        f( 20, 2 );
        f( 50, 3 );
        f( 120, 8 );
        f( 120, 2 );
    }

    template< typename N, template< typename > class Visitor >
    static void _sequential( int n, int m ) {
        typedef Check< NMTree< N > > C;
        NMTree< N > g( n, m );
        C c( g );

        // sanity check
        ASSERT_EQ( c.edges(), 0 );
        ASSERT_EQ( c.nodes(), 0 );

        struct CheckSetup : Check< NMTree< N > >, Sequential {};

        typename CheckSetup::Store s( g, 0 );
        WithID wid;
        wid.setId( 0, 0, 1, 1, 0 );
        s.setId( wid );

        Visitor< CheckSetup > bfv( c, g, s );
        bfv.exploreFrom( makeNode< N >( 1 , g.p ) );
        checkNMTreeMetric( n, m, c.nodes(), c.edges() );
    }

    template< template< typename > class T, typename N >
    static void _parallel( int n, int m ) {
        T< NMTree< N > > pv( std::make_pair( NMTree< N >( n, m ), n ) );
        pv.visit();
        checkNMTreeMetric( n, m, pv.nodes(), pv.edges() );
    }

    TEST(bfv_int) {
        // examples( _sequential< Int, BFV > );
    }

    TEST(dfv_int) {
        // examples( _sequential< Int, DFV > );
    }

    TEST(bfv_blob) {
        examples( _sequential< Blob, BFV > );
    }

    TEST(dfv_blob) {
        examples( _sequential< Blob, DFV > );
    }

    TEST(parallel_int) {
        // examples( _parallel< ParallelCheck, Int > );
    }

    TEST(parallel_blob) {
        examples( _parallel< ParallelCheck, Blob > );
    }

    TEST(terminable_int) {
        // examples( _parallel< TerminableCheck, Int > );
    }

    TEST(terminable_blob) {
        examples( _parallel< TerminableCheck, Blob > );
    }

    TEST(partition_int) {
        // examples( _parallel< PartitionCheck, Int > ) ;
    }

    TEST(partition_blob) {
        examples( _parallel< PartitionCheck, Blob > );
    }

    TEST(shared_int) {
        // examples( _parallel< SharedCheck, Int > );
    }

    TEST(shared_blob) {
        examples( _parallel< SharedCheck, Blob > );
    }
};

}

#endif
