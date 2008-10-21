// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/visitor.h>
#include <divine/parallel.h>
#include <cmath> // for pow

using namespace divine;

struct TestVisitor {
    typedef int Node;

    template< int n, int m >
    struct NMTree {
        typedef int Node;
        struct Successors {
            int i, _from;
            Node from() { return _from; }
            Node head() { int x = m * _from + i + 1; return x >= n ? 0 : x; }
            bool empty() {
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
        };
        Successors successors( Node n ) {
            Successors s;
            s._from = n;
            s.i = 0;
            return s;
        }
    };

    struct Checker {
        std::set< Node > seen;
        std::set< std::pair< Node, Node > > t_seen;
        int nodes, transitions;

        visitor::TransitionAction transition( Node f, Node t ) {
            // std::cerr << "transition: " << f << " - > " << t << std::endl;
            assert( seen.count( f ) );
            assert( !t_seen.count( std::make_pair( f, t ) ) );
            t_seen.insert( std::make_pair( f, t ) );
            transitions ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node t ) {
            // std::cerr << "expansion: " << t << std::endl;
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
        int transitions = (n - 1) + lastfull + remaining - remaining / m;

        /* std::cerr << "nodes = " << n
                  << ", fulltree height = " << fullheight
                  << ", fulltree nodes = " << fulltree
                  << ", last full = " << lastfull 
                  << ", remaining = " << remaining << std::endl; */
        assert_eq( n, _nodes );
        assert_eq( transitions, _transitions );
    }

    template< int n, int m >
    void _nmtree() {
        // bintree metrics
        // remaining - remaining/m is not same as remaining/m (due to flooring)

        NMTree< n, m > g;
        Checker c1, c2;

        // sanity check 
        assert_eq( c1.transitions, 0 );
        assert_eq( c1.nodes, 0 );

        visitor::BFV< NMTree< n, m >, Checker,
            &Checker::transition, &Checker::expansion > bfv( g, c1 );
        bfv.visit( 0 );
        checkNMTreeMetric( n, m, c1.nodes, c1.transitions );

        visitor::DFV< NMTree< n, m >, Checker,
            &Checker::transition, &Checker::expansion > dfv( g, c2 );
        dfv.visit( 0 );
        checkNMTreeMetric( n, m, c2.nodes, c2.transitions );
    }

    // requires that n % peers() == 0
    template< typename G, int n >
    struct ParVisitor : Domain< ParVisitor< G, n > > {
        struct Shared {
            Node initial;
            int seen, trans;
        } shared;

        int seen, trans;
        G g;

        visitor::TransitionAction transition( Node f, Node t ) {
            if ( t % this->peers() != this->id() ) {
                this->queue( t % this->peers() ).push( t );
                return visitor::IgnoreTransition;
            }
            shared.trans ++;
            return visitor::FollowTransition;
        }

        visitor::ExpansionAction expansion( Node n ) {
            ++ shared.seen;
            return visitor::ExpandState;
        }

        void _visit() {
            assert( !(n % this->peers()) );
            visitor::BFV< G, ParVisitor< G, n >,
                &ParVisitor< G, n >::transition,
                &ParVisitor< G, n >::expansion > bfv( g, *this );
            if ( shared.initial % this->peers() == this->id() ) {
                bfv.visit( shared.initial );
            }
            while ( shared.seen != n / this->peers() ) {
                if ( this->fifo.empty() )
                    continue;
                assert_eq( this->fifo.front() % this->peers(), this->id() );
                shared.trans ++;
                bfv.visit( this->fifo.front() );
                this->fifo.pop();
            }
        }

        void _finish() {
            while ( !this->fifo.empty() ) {
                this->shared.trans ++;
                this->fifo.pop();
            }
        }

        void visit( Node initial ) {
            shared.initial = initial;
            seen = shared.seen = 0;
            trans = shared.trans = 0;
            this->parallel().run( &ParVisitor< G, n >::_visit );
            for ( int i = 0; i < this->parallel().n; ++i ) {
                seen += this->parallel().shared( i ).seen;
                trans += this->parallel().shared( i ).trans;
            }
            shared.seen = 0;
            shared.trans = 0;
            this->parallel().run( &ParVisitor< G, n >::_finish );
            for ( int i = 0; i < this->parallel().n; ++i )
                trans += this->parallel().shared( i ).trans;
        }

        ParVisitor() {}
    };

    template< int n, int m >
    void _parVisitor() {
        ParVisitor< NMTree< n, m >, n > pv;
        pv.visit( 0 );
        checkNMTreeMetric( n, m, pv.seen, pv.trans );
    }

    Test nmtree() {
        _nmtree< 7, 2 >();
        _nmtree< 8, 2 >();
        _nmtree< 31, 2 >();
        _nmtree< 4, 3 >();
        _nmtree< 8, 3 >();
        _nmtree< 242, 3 >();
        _nmtree< 245, 3 >();

        // check that the stuff we use in parVisitor later actually works
        _nmtree< 20, 2 >();
        _nmtree< 50, 3 >();
        _nmtree< 120, 8 >();
        _nmtree< 120, 2 >();
    }

    Test parVisitor() {
        // note we need first number to be 10-divisible for now.
        _parVisitor< 20, 2 >();
        _parVisitor< 50, 3 >();
        _parVisitor< 120, 8 >();
        _parVisitor< 120, 2 >();
    }
};
