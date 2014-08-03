// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <cmath> // for pow
#include <tuple>
#include <memory>

#include <divine/toolkit/blob.h>
#include <divine/toolkit/parallel.h>
#include <divine/toolkit/pool.h>
#include <divine/graph/visitor.h>
#include <divine/graph/store.h>
#include <divine/utility/statistics.h>

#include <brick-hashset.h>

using namespace divine;
using namespace wibble;
using namespace visitor;

namespace brick_test { namespace hashset {

template<>
struct test_hasher< Lake::Pointer >
{
    typedef Lake::Pointer T;
    Pool& _pool;
    Pool &pool() { return _pool; }
    test_hasher( Pool& p ) : _pool( p ) { }
    test_hasher( const test_hasher &o ) : _pool( o._pool ) {}
    hash128_t hash( T t ) const { return _pool.hash( t ); }
    bool valid( T t ) const { return _pool.valid( t ); }
    bool equal( T a, T b ) const { return _pool.equal( a, b ); }
};

} }

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
int node( Blob n, Pool &p ) { return p.valid( n ) ? p.get< int >( n ) : 0; }
int node( const Int &n, Pool& ) { return n.i; }

template< typename N > N makeNode( int, Pool& );
template<> Int makeNode< Int >( int n, Pool& ) { return n; }
template<> Blob makeNode< Blob >( int n, Pool& p ) {
    Blob b = p.allocate( sizeof( int ) );
    p.get< int >( b ) = n;
    return b;
}

struct TestVisitor {
    typedef void Test_;

    template< typename _Node >
    struct NMTree {
        typedef _Node Node;
        typedef wibble::Unit Label;
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

        template< typename V, typename Yield >
        void successors( V from, Yield yield ) {
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
    using StoreFor = visitor::DefaultStore< Provider, G, TestHasher< typename G::Node >, NoStatistics >;

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
                assert( c.seen.count( f ) );
                c.edges() ++;
                assert( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        template< typename V >
        static ExpansionAction expansion( This &c, V tV ) {
            Node t = tV.node();
            assert( !c.seen.count( t ) );
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
            assert( self->seen.count( f ) );

        if ( node( f, self->_graph.pool() ) ) {
            self->edges() ++;
            assert( !self->t_seen.count( std::make_pair( f, t ) ) );
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
            assert_eq( expected % this->peers(), 0 );
            assert_leq( 0, this->id() );
            assert_leq( 0, this->rank() );
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
                assert_eq( node( std::get< 1 >( next ), this->_graph.pool() ) % this->peers(), this->id() );
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
                assert( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        void _visit() { // parallel
            assert_eq( expected % this->peers(), 0 );
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
                assert( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return TransitionAction::Follow;
        }

        void _visit() { // parallel
            store.setId( *this );
            assert_eq( expected % this->peers(), 0 );
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
                    assert_eq( owner( std::get< 1 >( next ) ), this->id() );
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
        int transitions = (n - 1) + lastfull + remaining - remaining / m;

        assert_eq( n, _nodes );
        assert_eq( transitions, _transitions );
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
        assert_eq( c.edges(), 0 );
        assert_eq( c.nodes(), 0 );

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

    Test bfv_int() {
        // examples( _sequential< Int, BFV > );
    }

    Test dfv_int() {
        // examples( _sequential< Int, DFV > );
    }

    Test bfv_blob() {
        examples( _sequential< Blob, BFV > );
    }

    Test dfv_blob() {
        examples( _sequential< Blob, DFV > );
    }

    Test parallel_int() {
        // examples( _parallel< ParallelCheck, Int > );
    }

    Test parallel_blob() {
        examples( _parallel< ParallelCheck, Blob > );
    }

    Test terminable_int() {
        // examples( _parallel< TerminableCheck, Int > );
    }

    Test terminable_blob() {
        examples( _parallel< TerminableCheck, Blob > );
    }

    Test partition_int() {
        // examples( _parallel< PartitionCheck, Int > ) ;
    }

    Test partition_blob() {
        examples( _parallel< PartitionCheck, Blob > );
    }

    Test shared_int() {
        // examples( _parallel< SharedCheck, Int > );
    }

    Test shared_blob() {
        examples( _parallel< SharedCheck, Blob > );
    }

#if 0
    template< typename G >
    struct SimpleParReach : DomainWorker< SimpleParReach< G > >
    {
        typedef typename G::Node Node;
        struct Shared {
            Node initial;
            int seen, trans;
            G g;
        } shared;
        Domain< SimpleParReach< G > > domain;

        std::set< int > seenset;

        TransitionAction transition( Node f, Node t, Label ) {
            shared.trans ++;
            return TransitionAction::Follow;
        }

        ExpansionAction expansion( Node n ) {
            seenset.insert( unblob< int >( n ) );
            ++ shared.seen;
            return ExpansionAction::Expand;
        }

        void _visit() { // parallel
            typedef Setup< G, SimpleParReach< G > > VisitorSetup;
            Partitioned::Data< This > data;
            Partitioned::Implementation< VisitorSetup, SimpleParReach< G > >
                vis( shared.g, *this, *this, data );
            vis.exploreFrom( shared.initial );
        }

        void visit( Node initial ) {
            shared.initial = initial;
            shared.seen = 0;
            shared.trans = 0;
            domain.parallel( Meta() ).run( shared, &SimpleParReach< G >::_visit );
            for ( int i = 0; i < domain.parallel( Meta() ).n; ++i ) {
                shared.seen += domain.parallel( Meta() ).shared( i ).seen;
                shared.trans += domain.parallel( Meta() ).shared( i ).trans;
            }
        }

        SimpleParReach( G g = G() ) { shared.g = g; }
        SimpleParReach( Meta ) {}
    };

    void _simpleParReach( int n, int m ) {
        SimpleParReach< BlobNMTree > pv( BlobNMTree( n, m ) );
        Blob init( sizeof( int ) );
        init.get< int >() = 0;
        pv.visit( init );
        checkNMTreeMetric( n, m, pv.shared.seen, pv.shared.trans );
    }
#endif

};
