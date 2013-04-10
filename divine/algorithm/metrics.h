// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>

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
    int64_t states, transitions, accepting, deadlocks, expansions;

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

    template< typename G >
    void addEdge( G &g, typename G::Node n, typename G::Node ) {
        if ( n.valid() )
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

    void merge( Statistics other ) {
        states += other.states;
        transitions += other.transitions;
        accepting += other.accepting;
        expansions += other.expansions;
        deadlocks += other.deadlocks;
    }

};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, Statistics st )
{
    return bs << st.states << st.transitions << st.expansions << st.accepting << st.deadlocks;
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, Statistics &st )
{
    return bs >> st.states >> st.transitions >> st.expansions >> st.accepting >> st.deadlocks;
}

/**
 * A very simple state space measurement algorithm. Explores the full state
 * space, keeping simple numeric statistics (see the Statistics template
 * above).
 */
template< typename Setup >
struct Metrics : Algorithm, AlgorithmUtils< Setup >,
                 Parallel< Setup::template Topology, Metrics< Setup > >
{
    typedef Metrics< Setup > This;
    typedef typename Setup::Vertex Vertex;
    typedef typename Setup::VertexId VertexId;
    struct Shared : algorithm::Statistics {
        bool need_expand;
        Shared() : need_expand( false ) {}
    };

    ALGORITHM_CLASS( Setup, Shared );

    struct Main : Visit< This, Setup > {
        static visitor::ExpansionAction expansion( This &t, Vertex st )
        {
            t.shared.addNode( t.graph(), st );
            t.graph().porExpansion( st );
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &t, Vertex from, Vertex to, Label )
        {
            t.shared.addEdge( t.graph(), from.getNode(), to.getNode() );
            t.graph().template porTransition< Vertex >( from, to, 0 );
            return visitor::FollowTransition;
        }

        static visitor::DeadlockAction deadlocked( This &t, Vertex ) {
            t.shared.addDeadlock();
            return visitor::IgnoreDeadlock;
        }
    };

    void _visit() { // parallel
        this->visit( this, Main(), shared.need_expand );
    }

    void _por_worker() {
        this->graph()._porEliminate( *this );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( this->graph().porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    Metrics( Meta m ) : Algorithm( m, 0 )
    {
        this->init( this );
        this->becomeMaster( m.execution.threads, this );
    }

    Metrics( Metrics *master ) : Algorithm( master->meta(), 0 )
    {
        this->init( this, master );
    }


    void banner( std::ostream &o ) {
        auto &s = meta().statistics;
        o << " ============================================= " << std::endl
          << std::setw( 12 ) << s.visited << " states" << std::endl
          << std::setw( 12 ) << s.transitions << " transitions" << std::endl
          << std::setw( 12 ) << s.accepting << " accepting" << std::endl
          << std::setw( 12 ) << s.deadlocks << " deadlocks " << std::endl
          << " ============================================= " << std::endl;
    }

    void collect() {
        for ( auto s : shareds )
            s.update( meta().statistics );
    }

    void run() {
        progress() << "  exploring... \t\t\t " << std::flush;
        parallel( &This::_visit );
        collect();

        do {
            shared.need_expand = false;
            ring( &This::_por );
            if ( shared.need_expand ) {
                parallel( &This::_visit );
                collect();
            }
        } while ( shared.need_expand );

        progress() << "done" << std::endl;
        banner( progress() );

        result().fullyExplored = meta::Result::Yes;
        shared.update( meta().statistics );
    }
};

ALGORITHM_RPC( Metrics );
ALGORITHM_RPC_ID( Metrics, 1, _visit );
ALGORITHM_RPC_ID( Metrics, 2, _por );
ALGORITHM_RPC_ID( Metrics, 3, _por_worker );

}
}

#endif
