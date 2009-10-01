// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <cmath> // for pow
#include <divine/blob.h>

using namespace divine;
using namespace wibble;

struct TestVisitor {
    struct NMTree {
        typedef int Node;
        int n, m;

        struct Successors {
            int n, m;
            int i, _from;
            Node from() { return _from; }
            Node head() {
                int x = m * (_from - 1) + i + 1; return (x >= n ? 0 : x)+1;
            }
            bool empty() {
                if ( n < 0 )
                    return true;
                // no multi-edges to 0 please
                if ( i > 0 && m * (_from - 1) + i >= n )
                    return true;
                return i >= m;
            }

            Successors tail() {
                Successors next = *this;;
                next.i ++;
                return next;
            }

            Successors() : n( -1 ) {}
        };

        void release( Node ) {}

        Successors successors( Node from ) {
            Successors s;
            s.n = n;
            s.m = m;
            s._from = from;
            s.i = 0;
            return s;
        }

        NMTree( int _n, int _m ) : n( _n ), m( _m ) {}
        NMTree() : n( 0 ), m( 0 ) {}
    };

    template< typename G >
    struct Checker {
        typedef typename G::Node Node;
        std::set< Node > seen;
        std::set< std::pair< Node, Node > > t_seen;
        int nodes, transitions;

        visitor::TransitionAction transition( Node f, Node t ) {
            assert( seen.count( f ) );
            assert( !t_seen.count( std::make_pair( f, t ) ) );
            t_seen.insert( std::make_pair( f, t ) );
            transitions ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node t ) {
            assert( !seen.count( t ) );
            seen.insert( t );
            nodes ++;
            return visitor::ExpandState;
        }

        Checker() : nodes( 0 ), transitions( 0 ) {}
    };

    void checkNMTreeMetric( int n, int m, int _nodes, int _transitions )
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

    void _nmtree( int n, int m ) {
        NMTree g( n, m );
        typedef Checker< NMTree > C;
        C c1, c2;

        // sanity check 
        assert_eq( c1.transitions, 0 );
        assert_eq( c1.nodes, 0 );

        typedef visitor::Setup< NMTree, C > CheckSetup;

        visitor::BFV< CheckSetup > bfv( g, c1 );
        bfv.exploreFrom( 1 );
        checkNMTreeMetric( n, m, c1.nodes, c1.transitions );

        visitor::DFV< CheckSetup > dfv( g, c2 );
        dfv.exploreFrom( 1 );
        checkNMTreeMetric( n, m, c2.nodes, c2.transitions );
    }

    // requires that n % peers() == 0
    template< typename G >
    struct ParVisitor : DomainWorker< ParVisitor< G > > {
        typedef typename G::Node Node;

        struct Shared {
            Node initial;
            int seen, trans;
            G g;
            int n;
        } shared;
        Domain< ParVisitor< G > > domain;

        int seen, trans;
        std::set< int > seenset;

        visitor::TransitionAction transition( Node f, Node t ) {
            if ( t % this->peers() != this->globalId() ) {
                push( this->queue( this->globalId(), t % this->peers() ), t );
                return visitor::IgnoreTransition;
            }
            shared.trans ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node n ) {
            ++ shared.seen;
            assert( !seenset.count( unblob< int >( n ) ) );
            seenset.insert( unblob< int >( n ) );
            return visitor::ExpandState;
        }

        void _visit() { // parallel
            assert( !(shared.n % this->peers()) );
            typedef visitor::Setup< G, ParVisitor< G > > VisitorSetup;
            visitor::BFV< VisitorSetup > bfv( shared.g, *this );
            if ( shared.initial % this->peers() == this->globalId() ) {
                bfv.exploreFrom( shared.initial );
            }
            while ( shared.seen != shared.n / this->peers() ) {
                if ( this->fifo.empty() )
                    continue;
                assert_eq( this->fifo.next().template get< int >()
                             % this->peers(),
                           this->globalId() );
                shared.trans ++;
                bfv.expand( unblob< Node >( this->fifo.next() ) );
                this->fifo.remove();
            }
        }

        void _finish() { // parallel
            while ( !this->fifo.empty() ) {
                shared.trans ++;
                this->fifo.remove();
            }
        }

        void visit( Node initial ) {
            shared.initial = initial;
            seen = shared.seen = 0;
            trans = shared.trans = 0;
            domain.parallel().run( shared, &ParVisitor< G >::_visit );
            for ( int i = 0; i < domain.parallel().n; ++i ) {
                seen += domain.parallel().shared( i ).seen;
                trans += domain.parallel().shared( i ).trans;
            }
            shared.seen = 0;
            shared.trans = 0;
            domain.parallel().run( shared, &ParVisitor< G >::_finish );
            for ( int i = 0; i < domain.parallel().n; ++i )
                trans += domain.parallel().shared( i ).trans;
        }

        ParVisitor( G g = G(), int _n = 0 ) 
            : domain( 0, 10 )
        {
            shared.g = g;
            shared.n = _n;
        }
    };

    void _parVisitor( int n, int m ) {
        ParVisitor< NMTree > pv( NMTree( n, m ), n );
        pv.visit( 1 );
        checkNMTreeMetric( n, m, pv.seen, pv.trans );
    }

    // requires that n % peers() == 0
    template< typename G >
    struct TermParVisitor : DomainWorker< TermParVisitor< G > >
    {
        typedef typename G::Node Node;
        struct Shared {
            Node initial;
            int seen, trans;
            G g;
        } shared;
        Domain< TermParVisitor< G > > domain;

        std::set< int > seenset;

        visitor::TransitionAction transition( Node f, Node t ) {
            if ( owner( t ) != this->globalId() ) {
                push( this->queue( this->globalId(), owner( t ) ), t );
                return visitor::IgnoreTransition;
            }
            assert_eq( owner( t ), this->globalId() );
            shared.trans ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node n ) {
            assert( !seenset.count( unblob< int >( n ) ) );
            assert_eq( owner( n ), this->globalId() );
            seenset.insert( unblob< int >( n ) );
            ++ shared.seen;
            return visitor::ExpandState;
        }

        int owner( Node n ) {
            return unblob< int >( n ) % this->peers();
        }

        void _visit() { // parallel
            typedef visitor::Setup< G, TermParVisitor< G > > VisitorSetup;
            visitor::BFV<  VisitorSetup > bfv( shared.g, *this );
            if ( owner( shared.initial ) == this->globalId() ) {
                bfv.exploreFrom( unblob< Node >( shared.initial ) );
            }
            while ( true ) {
                if ( this->fifo.empty() ) {
                    if ( this->idle() )
                        return;
                } else {
                    assert_eq( this->fifo.next().template get< int >()
                            % this->peers(), this->globalId() );
                    shared.trans ++;
                    bfv.expand( unblob< Node >( this->fifo.next() ) );
                    this->fifo.remove();
                }
            }
        }

        void visit( Node initial ) {
            shared.initial = initial;
            shared.seen = 0;
            shared.trans = 0;
            domain.parallel().run( shared, &TermParVisitor< G >::_visit );
            for ( int i = 0; i < domain.parallel().n; ++i ) {
                shared.seen += domain.parallel().shared( i ).seen;
                shared.trans += domain.parallel().shared( i ).trans;
            }
        }

        TermParVisitor( G g = G() ) { shared.g = g; }
    };

    void _termParVisitor( int n, int m ) {
        TermParVisitor< NMTree > pv( NMTree( n, m ) );
        pv.visit( 1 );
        checkNMTreeMetric( n, m, pv.shared.seen, pv.shared.trans );
    }

    Test nmtree() {
        _nmtree( 7, 2 );
        _nmtree( 8, 2 );
        _nmtree( 31, 2 );
        _nmtree( 4, 3 );
        _nmtree( 8, 3 );
        _nmtree( 242, 3 );
        _nmtree( 245, 3 );

        // check that the stuff we use in parVisitor later actually works
        _nmtree( 20, 2 );
        _nmtree( 50, 3 );
        _nmtree( 120, 8 );
        _nmtree( 120, 2 );
    }

    Test parVisitor() {
        // note we need first number to be 10-divisible for now.
        _parVisitor( 20, 2 );
        _parVisitor( 50, 3 );
        _parVisitor( 120, 8 );
        _parVisitor( 120, 2 );
    }

    Test termParVisitor() {
        _termParVisitor( 7, 2 );
        _termParVisitor( 8, 2 );
        _termParVisitor( 31, 2 );
        _termParVisitor( 4, 3 );
        _termParVisitor( 8, 3 );
        _termParVisitor( 242, 3 );
        _termParVisitor( 245, 3 );
        _termParVisitor( 20, 2 );
        _termParVisitor( 50, 3 );
        _termParVisitor( 120, 8 );
        _termParVisitor( 120, 2 );
    }

    struct BlobNMTree {
        typedef Blob Node;
        int n, m;

        struct Successors {
            int n, m;
            int i, _from;
            Node from() {
                Node n( sizeof( int ) );
                n.get< int >() = _from;
                return n;
            }
            Node head() {
                Node r = Node( sizeof( int ) );
                int x = m * _from + i + 1;
                r.get< int >() = x >= n ? 0 : x;
                return r;
            }

            bool empty() {
                if ( n < 0 )
                    return true;
                // no multi-edges to 0 please
                if ( i > 0 && m * _from + i >= n )
                    return true;
                return i >= m;
            }

            Successors tail() {
                Successors next = *this;;
                next.i ++;
                return next;
            }

            Successors() : n( -1 ) {}
        };

        void release( Node n ) {
            n.free();
        }

        Successors successors( Node from ) {
            Successors s;
            s.n = n;
            s.m = m;
            s._from = from.get< int >();
            s.i = 0;
            return s;
        }

        BlobNMTree( int _n, int _m ) : n( _n ), m( _m ) {}
        BlobNMTree() : n( 0 ), m( 0 ) {}
    };

    void _bTermParVisitor( int n, int m ) {
        TermParVisitor< BlobNMTree > pv( BlobNMTree( n, m ) );
        Blob init( sizeof( int ) );
        init.get< int >() = 0;
        pv.visit( init );
        checkNMTreeMetric( n, m, pv.shared.seen, pv.shared.trans );
    }

    Test bTermParVisitor() {
        _bTermParVisitor( 7, 2 );
        _bTermParVisitor( 8, 2 );
        _bTermParVisitor( 31, 2 );
        _bTermParVisitor( 4, 3 );
        _bTermParVisitor( 8, 3 );
        _bTermParVisitor( 242, 3 );
        _bTermParVisitor( 245, 3 );
        _bTermParVisitor( 20, 2 );
        _bTermParVisitor( 50, 3 );
        _bTermParVisitor( 120, 8 );
        _bTermParVisitor( 120, 2 );
    }

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

        visitor::TransitionAction transition( Node f, Node t ) {
            shared.trans ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node n ) {
            seenset.insert( unblob< int >( n ) );
            ++ shared.seen;
            return visitor::ExpandState;
        }

        void _visit() { // parallel
            typedef visitor::Setup< G, SimpleParReach< G > > VisitorSetup;
            visitor::Parallel< VisitorSetup, SimpleParReach< G > >
                vis( shared.g, *this, *this );
            vis.exploreFrom( shared.initial );
        }

        void visit( Node initial ) {
            shared.initial = initial;
            shared.seen = 0;
            shared.trans = 0;
            domain.parallel().run( shared, &SimpleParReach< G >::_visit );
            for ( int i = 0; i < domain.parallel().n; ++i ) {
                shared.seen += domain.parallel().shared( i ).seen;
                shared.trans += domain.parallel().shared( i ).trans;
            }
        }

        SimpleParReach( G g = G() ) { shared.g = g; }
    };

    void _simpleParReach( int n, int m ) {
        SimpleParReach< BlobNMTree > pv( BlobNMTree( n, m ) );
        Blob init( sizeof( int ) );
        init.get< int >() = 0;
        pv.visit( init );
        checkNMTreeMetric( n, m, pv.shared.seen, pv.shared.trans );
    }

    Test simpleParReach() {
        _simpleParReach( 7, 2 );
        _simpleParReach( 8, 2 );
        _simpleParReach( 31, 2 );
        _simpleParReach( 4, 3 );
        _simpleParReach( 8, 3 );
        _simpleParReach( 242, 3 );
        _simpleParReach( 245, 3 );
        _simpleParReach( 20, 2 );
        _simpleParReach( 50, 3 );
        _simpleParReach( 120, 8 );
        _simpleParReach( 120, 2 );
    }

};
