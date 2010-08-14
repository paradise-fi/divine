// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/visitor.h>
#include <divine/report.h>

#ifndef DIVINE_NDFS_H
#define DIVINE_NDFS_H

namespace divine {
namespace algorithm {

template< typename G, typename Statistics >
struct NestedDFS : Algorithm
{
    typedef NestedDFS< G, Statistics > This;
    G g;
    typedef typename G::Node Node;
    Node seed;
    bool valid;

    std::deque< Node > ce_stack;
    std::deque< Node > ce_lasso;

    algorithm::Statistics< G > stats;

    struct Extension {
        bool nested:1;
    };

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    void inner( Node n ) {
        if ( !g.isAccepting( n ) )
            return;
        seed = n;
        visitor::DFV< InnerVisit > inner( g, *this, &table() );
        inner.exploreFrom( n );
    }

    void counterexample() {
        progress() << "generating counterexample... " << std::flush;
        typedef LtlCE< G, Unit, Unit > CE;
        CE::generateLinear( *this, g, ce_stack );
        CE::generateLasso( *this, g, ce_lasso );
        progress() << "done" << std::endl;
    }

    Result run() {
        std::cerr << " searching...\t\t\t" << std::flush;
        visitor::DFV< OuterVisit > visitor( g, *this, &table() );
        visitor.exploreFrom( g.initial() );

        std::cerr << "done" << std::endl;
        livenessBanner( valid );

        if ( !valid )
            counterexample();

        stats.updateResult( result() );
        result().ltlPropertyHolds = valid ? Result::Yes : Result::No;
        result().fullyExplored = valid ? Result::Yes : Result::No;
        return result();
    }

    visitor::ExpansionAction expansion( Node st ) {
        if ( !valid )
            return visitor::TerminateOnState;
        stats.addNode( g, st );
        ce_stack.push_front( st );
        return visitor::ExpandState;
    }

    visitor::ExpansionAction innerExpansion( Node st ) {
        if ( !valid )
            return visitor::TerminateOnState;
        stats.addExpansion();
        ce_lasso.push_front( st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node from, Node to ) {
        stats.addEdge();
        return visitor::FollowTransition;
    }

    visitor::TransitionAction innerTransition( Node from, Node to )
    {
        if ( to == seed ) {
            valid = false;
            return visitor::TerminateOnTransition;
        }

        if ( !extension( to ).nested ) {
            extension( to ).nested = true;
            return visitor::ExpandTransition;
        }

        return visitor::FollowTransition;
    }

    struct OuterVisit : visitor::Setup< G, This, Table, Statistics > {
        static void finished( This &dfs, Node n ) {
            dfs.inner( n );
            if ( !dfs.ce_stack.empty() ) {
                assert_eq( n.pointer(), dfs.ce_stack.front().pointer() );
                dfs.ce_stack.pop_front();
            }
        }
    };

    struct InnerVisit : visitor::Setup< G, This, Table, Statistics,
                                        &This::innerTransition,
                                        &This::innerExpansion >
    {
        static void finished( This &dfs, Node n ) {
            if ( !dfs.ce_lasso.empty() ) {
                assert_eq( n.pointer(), dfs.ce_lasso.front().pointer() );
                dfs.ce_lasso.pop_front();
            }
        }
    };

    NestedDFS( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        valid = true;
        initGraph( g );
    }

};

}
}

#endif
