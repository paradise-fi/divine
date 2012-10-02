// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <cmath> // for pow

#include <divine/toolkit/blob.h>
#include <divine/toolkit/parallel.h>
#include <divine/graph/visitor.h>

using namespace divine;
using namespace wibble;
using namespace visitor;

template< typename N >
inline Blob blob( const N &n ) {
    Blob b( sizeof( N ) );
    b.template get< N >() = n;
    return b;
}

template<>
inline Blob blob( const Blob &b ) {
    return b;
}

template< typename N > int node( N );
template< typename N > N makeNode( int );

template<> int node< Blob >( Blob b ) {
    if ( b.valid() )
        return b.get< int >();
    else return 0;
}
template<> int node< int >( int n ) { return n; }

template<> int makeNode< int >( int n ) { return n; }
template<> Blob makeNode< Blob >( int n ) {
    Blob b( sizeof( int ) );
    b.get< int >() = n;
    return b;
}

struct TestVisitor {
    typedef void Test_;

    template< typename _Node >
    struct NMTree {
        typedef _Node Node;
        int n, m;

        Node clone( Node n ) { return n; }
        void release( Blob n ) { n.free(); }
        void release( int ) {}
        Node initial() {
            static Node n = makeNode< Node >( 1 );
            setPermanent( n );
            return n;
        }

        template< typename Yield >
        void successors( Node from, Yield yield ) {
            if ( n < 0 )
                return;

            for ( int i = 0; i < m; ++i ) {
                int x = m * ( node( from ) - 1 ) + i + 1;
                int y = (x >= n ? 0 : x) + 1;
                yield( makeNode< Node >( y ) );
                if ( x >= n )
                    break;
            }
        }

        NMTree( int _n, int _m ) : n( _n ), m( _m ) {}
        NMTree() : n( 0 ), m( 0 ) {}

        template< typename Hash, typename Worker >
        int owner( Hash &hash, Worker &worker, Node n, hash_t = 0 ) {
            return hash( n ) % worker.peers();
        }
    };

    template< typename G >
    struct Check : SetupBase {
        typedef typename G::Node Node;
        typedef G Graph;
        typedef Check< G > This;
        typedef This Listener;
        typedef NoStatistics Statistics;
        typedef PartitionedStore< G > Store;
        std::set< Node > seen;
        std::set< std::pair< Node, Node > > t_seen;
        std::pair< int, int > counts;

        int &edges() { return counts.first; }
        int &nodes() { return counts.second; }

        int _edges() { return this->edges(); }
        int _nodes() { return this->nodes(); }

        template< typename Self >
        static TransitionAction transition( Self &c, Node f, Node t ) {
            if ( node( f ) ) {
                assert( c.seen.count( f ) );
                c.edges() ++;
                assert( !c.t_seen.count( std::make_pair( f, t ) ) );
                c.t_seen.insert( std::make_pair( f, t ) );
            }
            return FollowTransition;
        }

        static ExpansionAction expansion( This &c, Node t ) {
            assert( !c.seen.count( t ) );
            c.seen.insert( t );
            c.nodes() ++;
            return ExpandState;
        }

        Check() : counts( std::make_pair( 0, 0 ) ) {}
    };

    template< typename T, typename N >
    static TransitionAction parallel_transition( T *self, N f, N t ) {
        if ( node( t ) % self->peers() != self->id() ) {
            self->submit( self->id(), node( t ) % self->peers(),
                          std::make_pair( f, t ) );
                return IgnoreTransition;
            }

        if ( node( f ) % self->peers() == self->id() )
            assert( self->seen.count( f ) );

        if ( node( f ) ) {
            self->edges() ++;
            assert( !self->t_seen.count( std::make_pair( f, t ) ) );
            self->t_seen.insert( std::make_pair( f, t ) );
        }

        return FollowTransition;
    }

    template< typename G >
    struct ParallelCheck : Check< G >, Parallel<
        Topology< std::pair< typename G::Node, typename G::Node > >::template Local,
        ParallelCheck< G > >
    {
        typedef ParallelCheck< G > This;
        typedef This Listener;
        typedef typename G::Node Node;
        typedef G Graph;
        typedef std::pair< Node, Node > Message;
        Node make( int n ) { return makeNode< Node >( n ); }
        int expected;

        G m_graph;

        static TransitionAction transition( This &c, Node f, Node t ) {
            return parallel_transition( &c, f, t );
        }

        void _visit() { // parallel
            assert_eq( expected % this->peers(), 0 );
            PartitionedStore< G > store( m_graph );
            store.id = this;
            BFV< ParallelCheck< G > > bfv( *this, m_graph, store );
            Node initial = m_graph.initial();
            if ( node( initial ) % this->peers() == this->id() )
                bfv.exploreFrom( initial );

            while ( this->nodes() != expected / this->peers() ) {
                if ( !this->comms().pending( this->id() ) )
                    continue;

                Message next = this->comms().take( this->id() );
                assert_eq( node( next.second ) % this->peers(), this->id() );
                bfv.queue( next.first, next.second );
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

        ParallelCheck( std::pair< G, int > init, bool master = false )
        {
            m_graph = init.first;
            expected = init.second;
            if ( master ) {
                int i = 32;
                while ( expected % i ) i--;
                this->becomeMaster( i, init );
            }
        }
    };

    template< typename G >
    struct TerminableCheck : Parallel<
        Topology< std::pair< typename G::Node, typename G::Node > >::template Local,
        TerminableCheck< G > >, Check< G >
    {
        typedef TerminableCheck< G > This;
        typedef This Listener;
        typedef typename G::Node Node;
        typedef std::pair< Node, Node > Message;
        G m_graph;

        static TransitionAction transition( This &c, Node f, Node t ) {
            return parallel_transition( &c, f, t );
        }

        int owner( Node n ) {
            return node( n ) % this->peers();
        }

        void _visit() { // parallel
            PartitionedStore< G > store( m_graph );
            store.id = this;
            BFV< This > bfv( *this, m_graph, store );

            Node initial = m_graph.initial();
            if ( owner( initial ) == this->id() )
                bfv.exploreFrom( initial );

            while ( true ) {
                if ( this->comms().pending( this->id() ) ) {
                    Message next = this->comms().take( this->id() );
                    assert_eq( owner( next.second ), this->id() );
                    bfv.queue( next.first, next.second );
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

        TerminableCheck( std::pair< G, int > init, bool master = false )
        {
            m_graph = init.first;
            if ( master )
                this->becomeMaster( 10, init );
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
        NMTree< N > g( n, m );
        typedef Check< NMTree< N > > C;
        C c;

        // sanity check
        assert_eq( c.edges(), 0 );
        assert_eq( c.nodes(), 0 );

        struct CheckSetup : Check< NMTree< N > >, Sequential {};

        typename CheckSetup::Store s( g );
        WithID wid;
        wid.m_id = 0;
        s.id = &wid;

        Visitor< CheckSetup > bfv( c, g, s );
        bfv.exploreFrom( makeNode< N >( 1 ) );
        checkNMTreeMetric( n, m, c.nodes(), c.edges() );
    }

    template< template< typename > class T, typename N >
    static void _parallel( int n, int m ) {
        T< NMTree< N > > pv( std::make_pair( NMTree< N >( n, m ), n ), true );
        pv.visit();
        checkNMTreeMetric( n, m, pv.nodes(), pv.edges() );
    }

    Test bfv_int() {
        examples( _sequential< int, BFV > );
    }

    Test dfv_int() {
        examples( _sequential< int, DFV > );
    }

    Test bfv_blob() {
        examples( _sequential< Blob, BFV > );
    }

    Test dfv_blob() {
        examples( _sequential< Blob, DFV > );
    }

    Test parallel_int() {
        examples( _parallel< ParallelCheck, int > );
    }

    Test parallel_blob() {
        examples( _parallel< ParallelCheck, Blob > );
    }

    Test terminable_int() {
        examples( _parallel< TerminableCheck, int > );
    }

    Test terminable_blob() {
        examples( _parallel< TerminableCheck, Blob > );
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

        TransitionAction transition( Node f, Node t ) {
            shared.trans ++;
            return FollowTransition;
        }

        ExpansionAction expansion( Node n ) {
            seenset.insert( unblob< int >( n ) );
            ++ shared.seen;
            return ExpandState;
        }

        void _visit() { // parallel
            typedef Setup< G, SimpleParReach< G > > VisitorSetup;
            Partitioned< VisitorSetup, SimpleParReach< G > >
                vis( shared.g, *this, *this );
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
