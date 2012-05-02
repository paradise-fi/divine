// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/visitor.h>
#include <divine/parallel.h>

#include <iomanip>

#ifndef DIVINE_ALGORITHM_METRICS_H
#define DIVINE_ALGORITHM_METRICS_H

namespace divine {
namespace algorithm {

/**
 * A generalized statistics tracker for parallel algorithms. Put an instance in
 * your Shared structure and use the addNode/addEdge traps in your algorithm to
 * collect data. Use merge() at the end of your computation to combine the data
 * from multiple parallel workers. Call updateResult to popuplate your Result
 * structure with collected data (for the report).
 */
struct Statistics {
    int states, transitions, accepting, deadlocks, expansions;

    Statistics() : states( 0 ), transitions( 0 ), accepting( 0 ), deadlocks( 0 ), expansions( 0 ) {}

    template< typename G >
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

    void update( meta::Statistics &s ) {
        s.visited += states;
        s.deadlocks += deadlocks;
        s.expanded += expansions;
        s.transitions += transitions;
        s.accepting += accepting;
    }

    void print( std::ostream &o ) {
        o << " ===================================== " << std::endl
          << std::setw( 12 ) << states << " states" << std::endl
          << std::setw( 12 ) << transitions << " transitions" << std::endl
          << std::setw( 12 ) << accepting << " accepting" << std::endl
          << std::setw( 12 ) << deadlocks << " deadlocks " << std::endl
          << " ===================================== " << std::endl;
    }

    void merge( Statistics other ) {
        states += other.states;
        transitions += other.transitions;
        accepting += other.accepting;
        expansions += other.expansions;
        deadlocks += other.deadlocks;
    }

};

static inline bitstream &operator<<( bitstream &bs, Statistics st )
{
    return bs << st.states << st.transitions << st.expansions << st.accepting << st.deadlocks;
}

static inline bitstream &operator>>( bitstream &bs, Statistics &st )
{
    return bs >> st.states >> st.transitions >> st.expansions >> st.accepting >> st.deadlocks;
}

/**
 * A very simple state space measurement algorithm. Explores the full state
 * space, keeping simple numeric statistics (see the Statistics template
 * above).
 */
template< typename G, template < typename > class Topology, typename Statistics >
struct Metrics : Algorithm, AlgorithmUtils< G >,
                 Parallel< Topology, Metrics< G, Topology, Statistics > >
{
    typedef Metrics< G, Topology, Statistics > This;
    ALGORITHM_CLASS( G, algorithm::Statistics );

    visitor::ExpansionAction expansion( Node st )
    {
        shared.addNode( m_graph, st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node, Node )
    {
        shared.addEdge();
        return visitor::FollowTransition;
    }

    struct VisitorSetup : visitor::Setup< G, This, typename AlgorithmUtils< G >::Table, Statistics > {
        static visitor::DeadlockAction deadlocked( This &r, Node ) {
            r.shared.addDeadlock();
            return visitor::IgnoreDeadlock;
        }
    };

    void _visit() { // parallel
        this->comms().notify( this->id(), &m_graph.pool() );
        visitor::Partitioned< VisitorSetup, This, Hasher >
            vis( m_graph, *this, *this, hasher, &this->table() );
        vis.exploreFrom( m_graph.initial() );
    }

    Metrics( Meta m, bool master = false )
        : Algorithm( m, 0 )
    {
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        this->init( this );
    }

    void run() {
        progress() << "  exploring... \t\t\t " << std::flush;
        parallel( &This::_visit );
        progress() << "done" << std::endl;

        for ( int i = 0; i < shareds.size(); ++i ) {
            shared.merge( shareds[ i ] );
        }

        shared.print( progress() );

        result().fullyExplored = meta::Result::Yes;
        shared.update( meta().statistics );
    }
};

ALGORITHM_RPC( Metrics );
ALGORITHM_RPC_ID( Metrics, 1, _visit );

}
}

#endif
