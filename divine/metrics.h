// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_METRICS_H
#define DIVINE_METRICS_H

namespace divine {
namespace algorithm {

template< typename > struct Metrics;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G >
struct _MpiId< Metrics< G > >
{
    static int to_id( void (Metrics< G >::*f)() ) {
        assert_eq( f, &Metrics< G >::_visit );
        return 0;
    }

    static void (Metrics< G >::*from_id( int n ))()
    {
        assert_eq( n, 0 );
        return &Metrics< G >::_visit;
    }

    template< typename O >
    static void writeShared( typename Metrics< G >::Shared s, O o ) {
        *o++ = s.states;
        *o++ = s.transitions;
        *o++ = s.accepting;
        *o++ = s.goals;
        *o++ = s.deadlocks;
    }

    template< typename I >
    static I readShared( typename Metrics< G >::Shared &s, I i ) {
        s.states = *i++;
        s.transitions = *i++;
        s.accepting = *i++;
        s.goals = *i++;
        s.deadlocks = *i++;
        return i;
    }
};
// END MPI drudgery

template< typename G >
struct Metrics : DomainWorker< Metrics< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        size_t states, transitions, accepting;
        size_t goals, deadlocks;
        G g;
    } shared;
    Domain< Metrics< G > > domain;

    typedef HashMap< Node, Unit, Hasher > Table;
    Hasher hasher;

    // TODO error & deadlock states
    visitor::ExpansionAction expansion( Node st )
    {
        ++shared.states;
        if ( shared.g.isAccepting( st ) ) {
            ++ shared.accepting;
        }
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        ++ shared.transitions;
        return visitor::FollowTransition;
    }

    void _visit() { // parallel
        typedef visitor::Setup< G, Metrics< G >, Table > VisitorSetup;
        visitor::Parallel< VisitorSetup, Metrics< G >, Hasher >
            vis( shared.g, *this, *this, hasher, new Table( hasher ) );
        vis.visit( shared.g.initial() );
    }

    Metrics( Config *c = 0 )
        : domain( &shared, workerCount( c ) )
    {
        shared.states = shared.transitions = shared.accepting = 0;
        shared.goals = shared.deadlocks = 0;
        if ( c )
            shared.g.read( c->input() );
    }

    Result run() {
        domain.parallel().run( shared, &Metrics< G >::_visit );

        for ( int i = 0; i < domain.peers(); ++i ) {
            Shared &s = domain.shared( i );
            shared.states += s.states;
            std::cerr << "peer " << i << " has seen "
                      << s.states << " states" << std::endl;
            shared.transitions += s.transitions;
            shared.accepting += s.accepting;
            shared.goals += s.goals;
            shared.deadlocks += s.deadlocks;
        }

        std::cerr << "seen total of: " << shared.states
                  << " states (" << shared.accepting
                  << " accepting) and "<< shared.transitions
                  << " transitions" << std::endl;

        std::cerr << "encountered total of " << shared.goals
                  << " goal and " << shared.deadlocks
                  << " deadlock states" << std::endl;

        Result res;
        res.visited = res.expanded = shared.states;
        res.deadlocks = shared.deadlocks;
        res.goals = shared.goals;
        res.fullyExplored = Result::Yes;
        return res;
    }
};

}
}

#endif
