// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm.h>
#include <divine/metrics.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_REACHABILITY_H
#define DIVINE_REACHABILITY_H

namespace divine {
namespace algorithm {

template< typename > struct Reachability;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G >
struct _MpiId< Reachability< G > >
{
    static int to_id( void (Reachability< G >::*f)() ) {
        assert_eq( f, &Reachability< G >::_visit );
        return 0;
    }

    static void (Reachability< G >::*from_id( int n ))()
    {
        assert_eq( n, 0 );
        return &Reachability< G >::_visit;
    }

    template< typename O >
    static void writeShared( typename Reachability< G >::Shared s, O o ) {
        o = s.stats.write( o );
    }

    template< typename I >
    static I readShared( typename Reachability< G >::Shared &s, I i ) {
        i = s.stats.read( i );
        return i;
    }
};
// END MPI drudgery

template< typename G >
struct Reachability : DomainWorker< Reachability< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        Node goal;
        Statistics< G > stats;
        G g;
    } shared;
    Domain< Reachability< G > > domain;

    struct Extension {
        Blob parent;
    };

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;
    Hasher hasher;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        shared.stats.addNode( shared.g, st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !extension( t ).parent.valid() )
            extension( t ).parent = f;
        shared.stats.addEdge();

        if ( shared.g.isGoal( t ) ) {
            shared.goal = t;
            return visitor::TerminateOnTransition;
        }

        return visitor::FollowTransition;
    }

    void _visit() { // parallel
        typedef visitor::Setup< G, Reachability< G >, Table > VisitorSetup;

        hasher.setSlack( sizeof( Extension ) );
        visitor::Parallel< VisitorSetup, Reachability< G >, Hasher >
            vis( shared.g, *this, *this, hasher,
                 new Table( hasher, divine::valid< Node >(),
                            Equal( hasher.slack ) ) );
        BlobAllocator *alloc = new BlobAllocator();
        alloc->setSlack( sizeof( Extension ) );
        shared.g.setAllocator( alloc );
        vis.visit( shared.g.initial() );
    }

    Reachability( Config *c = 0 )
        : domain( &shared, workerCount( c ) )
    {
        if ( c )
            shared.g.read( c->input() );
    }

    void counterexample( Node n ) {
        // XXX ... alloc is used by shared.g to determine slack size... this is
        // needed for showNode to work (it needs to know the slack)... Btw.,
        // OWCTY has the same problem...
        BlobAllocator *alloc = new BlobAllocator();
        alloc->setSlack( sizeof( Extension ) );
        shared.g.setAllocator( alloc );

        std::cerr << "GOAL: " << std::endl
                  << shared.g.showNode( n ) << std::endl;
        Node x = extension( n ).parent;

        if ( x.valid() ) {
            std::cerr << "reached from:" << std::endl;
            do {
                std::cerr << shared.g.showNode( x ) << std::endl;
                x = extension( x ).parent;
            } while ( extension( x ).parent.valid() );
        }
    }

    Result run() {
        domain.parallel().run( shared, &Reachability< G >::_visit );

        for ( int i = 0; i < domain.peers(); ++i ) {
            Shared &s = domain.shared( i );
            if ( s.goal.valid() )
                counterexample( s.goal );
            shared.stats.merge( s.stats );
        }

        shared.stats.print( std::cerr );

        Result res;
        res.fullyExplored = Result::Yes;
        shared.stats.updateResult( res );
        return res;
    }
};

}
}

#endif
