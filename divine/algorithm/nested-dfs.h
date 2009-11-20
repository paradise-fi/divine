// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/visitor.h>
#include <divine/report.h>

#ifndef DIVINE_NDFS_H
#define DIVINE_NDFS_H

namespace divine {
namespace algorithm {

template< typename G >
struct NestedDFS : Algorithm
{
    G g;
    typedef typename G::Node Node;
    Node seed;
    bool valid;

    struct Extension {
        bool nested:1;
    };

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    struct OuterVisit : visitor::Setup< G, NestedDFS< G >, Table > {
        static void finished( NestedDFS< G > &dfs, Node n ) {
            dfs.inner( n );
        }
    };

    void inner( Node n ) {
        if ( !g.isAccepting( n ) )
            return;
        seed = n;
        typedef visitor::Setup< G, NestedDFS< G >, Table,
            &NestedDFS< G >::innerTransition > Setup;
        visitor::DFV< Setup > inner( g, *this, &table() );
        inner.exploreFrom( n );
    }

    Result run() {
        std::cerr << " searching...\t\t\t" << std::flush;
        visitor::DFV< OuterVisit > visitor( g, *this, &table() );
        visitor.exploreFrom( g.initial() );

        std::cerr << "done" << std::endl;
        livenessBanner( valid );

        result().ltlPropertyHolds = valid ? Result::Yes : Result::No;
        result().fullyExplored = valid ? Result::Yes : Result::No;
        return result();
    }

    visitor::ExpansionAction expansion( Node ) {
        if ( !valid )
            return visitor::TerminateOnState;
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node from, Node to ) {
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
