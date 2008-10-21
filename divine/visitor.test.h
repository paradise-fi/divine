// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/visitor.h>
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

    template< int n, int m >
    void _nmtree() {
        // bintree metrics
        int fullheight = 1;
        int fulltree = 1;
        while ( fulltree + pow(m, fullheight) <= n ) {
            fulltree += pow(m, fullheight);
            fullheight ++;
        }
        int lastfull = pow(m, fullheight-1);
        int remaining = n - fulltree;
        int transitions = (n - 1) + lastfull + remaining - remaining / m;
        // remaining - remaining/m is not same as remaining/m (due to flooring)
        /* std::cerr << "nodes = " << n
                  << ", fulltree height = " << fullheight
                  << ", fulltree nodes = " << fulltree
                  << ", last full = " << lastfull 
                  << ", remaining = " << remaining << std::endl; */

        NMTree< n, m > g;
        Checker c1, c2;

        // sanity check 
        assert_eq( c1.transitions, 0 );
        assert_eq( c1.nodes, 0 );

        visitor::BFV< NMTree< n, m >, Checker,
            &Checker::transition, &Checker::expansion > bfv( g, c1 );
        bfv.visit( 0 );
        assert_eq( c1.nodes, n );
        assert_eq( c1.transitions, transitions );

        visitor::DFV< NMTree< n, m >, Checker,
            &Checker::transition, &Checker::expansion > dfv( g, c2 );
        dfv.visit( 0 );
        assert_eq( c2.nodes, n );
        assert_eq( c2.transitions, transitions );
    }

    Test nmtree() {
        _nmtree< 7, 2 >();
        _nmtree< 8, 2 >();
        _nmtree< 31, 2 >();
        _nmtree< 4, 3 >();
        _nmtree< 8, 3 >();
        _nmtree< 242, 3 >();
        _nmtree< 245, 3 >();
    }
};
