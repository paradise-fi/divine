// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#include <iomanip>

#ifndef DIVINE_METRICS_H
#define DIVINE_METRICS_H

namespace divine {
namespace algorithm {

template< typename, typename > struct Metrics;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G, typename S >
struct _MpiId< Metrics< G, S > >
{
    typedef Metrics< G, S > A;

    static int to_id( void (A::*f)() ) {
        assert_eq( f, &A::_visit );
        return 0;
    }

    static void (A::*from_id( int n ))()
    {
        assert_eq( n, 0 );
        return &A::_visit;
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        *o++ = s.initialTable;
        s.stats.write( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        s.initialTable = *i++;
        return s.stats.read( i );
    }
};
// END MPI drudgery

/**
 * A generalized statistics tracker for parallel algorithms. Put an instance in
 * your Shared structure and use the addNode/addEdge traps in your algorithm to
 * collect data. Use merge() at the end of your computation to combine the data
 * from multiple parallel workers. Call updateResult to popuplate your Result
 * structure with collected data (for the report).
 */
template< typename G >
struct Statistics {
    int states, transitions, accepting, deadlocks, expansions;

    Statistics() : states( 0 ), transitions( 0 ), accepting( 0 ), deadlocks( 0 ), expansions( 0 ) {}

    void addNode( G &g, typename G::Node n ) {
        ++states;
        if ( g.isAccepting( n ) )
            ++ accepting;
        addExpansion();
    }

    void addExpansion() {
        ++expansions;
    }

    void addEdge() {
        ++ transitions;
    }

    void addDeadlock() {
        ++ deadlocks;
    }

    void updateResult( Result &res ) {
        res.visited += states;
        res.deadlocks += deadlocks;
        res.expanded += expansions;
        res.transitions += transitions;
    }

    void print( std::ostream &o ) {
        o << " ===================================== " << std::endl
          << std::setw( 12 ) << states << " states" << std::endl
          << std::setw( 12 ) << transitions << " transitions" << std::endl
          << std::setw( 12 ) << accepting << " accepting" << std::endl
          << std::setw( 12 ) << deadlocks << " deadlocks " << std::endl
          << " ===================================== " << std::endl;
    }

    template< typename O >
    O write( O o ) {
        *o++ = states;
        *o++ = transitions;
        *o++ = accepting;
        *o++ = expansions;
        *o++ = deadlocks;
        return o;
    }

    template< typename I >
    I read( I i ) {
        states = *i++;
        transitions = *i++;
        accepting = *i++;
        expansions = *i++;
        deadlocks = *i++;
        return i;
    }

    template< typename G1 >
    void merge( Statistics< G1 > other ) {
        states += other.states;
        transitions += other.transitions;
        accepting += other.accepting;
        expansions += other.expansions;
        deadlocks += other.deadlocks;
    }

};

/**
 * A very simple state space measurement algorithm. Explores the full state
 * space, keeping simple numeric statistics (see the Statistics template
 * above).
 */
template< typename G, typename Statistics >
struct Metrics : Algorithm, DomainWorker< Metrics< G, Statistics > >
{
    typedef Metrics< G, Statistics > This;
    typedef typename G::Node Node;

    struct Shared {
        algorithm::Statistics< G > stats;
        G g;
        int initialTable;
    } shared;

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

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

    struct VisitorSetup : visitor::Setup< G, This, Table, Statistics > {
        static visitor::DeadlockAction deadlocked( This &r, Node n ) {
            r.shared.stats.addDeadlock();
            return visitor::IgnoreDeadlock;
        }
    };

    void _visit() { // parallel
        m_initialTable = &shared.initialTable; // XXX find better place for this
        visitor::Parallel< VisitorSetup, This, Hasher >
            vis( shared.g, *this, *this, hasher, &table() );
        vis.exploreFrom( shared.g.initial() );
    }

    Metrics( Config *c = 0 )
        : Algorithm( c, 0 )
    {
        initGraph( shared.g );
        if ( c ) {
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }

    Result run() {
        progress() << "  exploring... \t\t\t " << std::flush;
        domain().parallel().run( shared, &This::_visit );
        progress() << "done" << std::endl;

        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            shared.stats.merge( s.stats );
        }

        shared.stats.print( progress() );

        result().fullyExplored = Result::Yes;
        shared.stats.updateResult( result() );
        return result();
    }
};

}
}

#endif
