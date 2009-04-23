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
        s.stats.write( o );
    }

    template< typename I >
    static I readShared( typename Metrics< G >::Shared &s, I i ) {
        return s.stats.read( i );
    }
};
// END MPI drudgery

template< typename G >
struct Statistics {
    int states, transitions, accepting, goals, deadlocks;

    Statistics() : states( 0 ), transitions( 0 ), accepting( 0 ), goals( 0 ), deadlocks( 0 ) {}

    void addNode( G &g, typename G::Node n ) {
        ++states;
        if ( g.isAccepting( n ) )
            ++ accepting;
        if ( g.isDeadlock( n ) )
            ++ deadlocks;
        if ( g.isGoal( n ) )
            ++ goals;
    }

    void addEdge() {
        ++ transitions;
    }

    void updateResult( Result &res ) {
        res.visited = res.expanded = states;
        res.deadlocks = deadlocks;
        res.goals = goals;
    }

    void print( std::ostream &o ) {
        o << "seen total of: " << states
          << " states (" << accepting
          << " accepting) and "<< transitions
          << " transitions" << std::endl;

        o << "encountered total of " << goals
          << " goal and " << deadlocks
          << " deadlock states" << std::endl;
    }

    template< typename O >
    O write( O o ) {
        *o++ = states;
        *o++ = transitions;
        *o++ = accepting;
        *o++ = goals;
        *o++ = deadlocks;
        return o;
    }

    template< typename I >
    I read( I i ) {
        states = *i++;
        transitions = *i++;
        accepting = *i++;
        goals = *i++;
        deadlocks = *i++;
        return i;
    }

    template< typename G1 >
    void merge( Statistics< G1 > other ) {
        states += other.states;
        transitions += other.transitions;
        accepting += other.accepting;
        goals += other.goals;
        deadlocks += other.deadlocks;
    }

};

template< typename G >
struct Metrics : DomainWorker< Metrics< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        Statistics< G > stats;
        G g;
    } shared;
    Domain< Metrics< G > > domain;

    typedef HashMap< Node, Unit, Hasher > Table;
    Hasher hasher;

    // TODO error & deadlock states
    visitor::ExpansionAction expansion( Node st )
    {
        shared.stats.addNode( shared.g, st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        shared.stats.addEdge();
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
        if ( c )
            shared.g.read( c->input() );
    }

    Result run() {
        domain.parallel().run( shared, &Metrics< G >::_visit );

        for ( int i = 0; i < domain.peers(); ++i ) {
            Shared &s = domain.shared( i );
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
