// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_REACHABILITY_H
#define DIVINE_REACHABILITY_H

namespace divine {
namespace algorithm {

template< typename G >
struct Reachability : Domain< Reachability< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        size_t states, transitions, accepting;
        size_t errors, deadlocks;
        G g;
    } shared;

    // TODO error & deadlock states
    visitor::ExpansionAction expansion( Node st )
    {
        ++shared.states;
        if ( shared.g.is_accepting( st ) ) {
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
        typedef visitor::Setup< G, Reachability< G > > VisitorSetup;
        visitor::Parallel< VisitorSetup, Reachability< G > >
            vis( shared.g, *this, *this );
        vis.visit( shared.g.initial() );
    }

    void printStatistics( std::ostream &str, std::string prefix )
    {
        str << prefix << "states: " << shared.states << std::endl;
        str << prefix << "accepting states: " << shared.accepting
            << std::endl;
        str << prefix << "transitions: " << shared.transitions << std::endl;
    }

    Reachability( Config *c = 0 )
    {
        shared.states = shared.transitions = shared.accepting = 0;
        shared.errors = shared.deadlocks = 0;
        if ( c )
            shared.g.read( c->input() );
    }

    Result run() {
        typedef visitor::Setup< G, Reachability< G > > VisitorSetup;
        visitor::Parallel< VisitorSetup, Reachability< G > >
            vis( shared.g, *this, *this );

        this->parallel().run( &Reachability< G >::_visit );

        for ( int i = 0; i < this->parallel().n; ++i ) {
            Shared &s = this->parallel().shared( i );
            shared.states += s.states;
            shared.transitions += s.transitions;
            shared.accepting += s.accepting;
            shared.errors += s.errors;
            shared.deadlocks += s.deadlocks;
        }

        std::cerr << "seen total of: " << shared.states
                  << " states (" << shared.accepting
                  << " accepting) and "<< shared.transitions
                  << " transitions" << std::endl;

        std::cerr << "encountered total of " << shared.errors
                  << " errors and " << shared.deadlocks
                  << " deadlocks" << std::endl;
                  
        Result res;
        res.visited = res.expanded = shared.states;
        res.deadlocks = shared.deadlocks;
        res.errors = shared.errors;
        res.fullyExplored = Result::Yes;
        return res;
    }
};

}

}

#endif
