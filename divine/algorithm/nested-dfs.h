// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/algorithm/por-c3.h>
#include <divine/graph/ltlce.h>
#include <wibble/sfinae.h> // Unit

#ifndef DIVINE_ALGORITHM_NDFS_H
#define DIVINE_ALGORITHM_NDFS_H

namespace divine {
namespace algorithm {

template< typename Setup >
struct NestedDFS : Algorithm, AlgorithmUtils< Setup >, Sequential
{
    typedef NestedDFS< Setup > This;
    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Setup::Store Store;

    Node seed;
    bool valid;
    bool parallel, finished;

    std::deque< Node > ce_stack;
    std::deque< Node > ce_lasso;
    std::deque< Node > toexpand;

    algorithm::Statistics stats;

    struct Extension {
        bool nested:1;
        bool on_stack:1;
    };

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    int id() { return 0; } // expected by AlgorithmUtils

    void runInner( Graph &graph, Node n ) {
        seed = n;
        visitor::DFV< Inner > visitor( *this, graph, this->store() );
        visitor.exploreFrom( n );
    }

    struct : wibble::sys::Thread {
        Fifo< Node > process;
        This *outer;
        Graph graph;

        void *main() {
            while ( outer->valid ) {
                if ( !process.empty() ) {
                    Node n = process.front();
                    process.pop();
                    outer->runInner( graph, n ); // run the inner loop
                } else {
                    if ( outer->finished )
                        return 0;
#ifdef POSIX // uh oh...
                    sched_yield();
#endif
                }
            }
            return 0;
        }
    } inner;

    void counterexample() {
        progress() << "generating counterexample... " << std::flush;
        LtlCE< Graph, wibble::Unit, wibble::Unit > ce;
        ce.generateLinear( *this, this->graph(), ce_stack );
        ce.generateLasso( *this, this->graph(), ce_lasso );
        progress() << "done" << std::endl;
        result().ceType = meta::Result::Cycle;
    }

    // this is the entrypoint for full expansion... I know the name isn't best,
    // but that's what PORGraph uses
    void queue( Node from, Node to ) {
        visitor::DFV< Outer > visitor( *this, this->graph(), this->store() );
        visitor.exploreFrom( to );
    }

    void run() {
        progress() << " searching...\t\t\t" << std::flush;

        if ( parallel ) {
            inner.graph = this->graph();
            inner.start();
        }

        visitor::DFV< Outer > visitor( *this, this->graph(), this->store() );
        this->graph().queueInitials( visitor );
        visitor.processQueue();

        while ( valid && !toexpand.empty() ) {
            if ( !this->graph().full( toexpand.front() ) )
                this->graph().fullexpand( *this, toexpand.front() );
            toexpand.pop_front();
        }

        finished = true;
        if ( parallel )
            inner.join();

        progress() << "done" << std::endl;
        livenessBanner( valid );

        if ( !valid ) {
            if ( parallel )
                std::cerr << "Sorry, but counterexamples are not implemented in Parallel Nested DFS."
                          << std::endl
                          << "Please re-run with -w 1 to obtain a counterexample." << std::endl;
            else
                counterexample();
        }

        stats.update( meta().statistics );
        meta().statistics.deadlocks = -1; /* did not count */
        result().propertyHolds = valid ? meta::Result::Yes : meta::Result::No;
        result().fullyExplored = valid ? meta::Result::Yes : meta::Result::No;
    }

    struct Outer : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &dfs, Node st ) {
            if ( !dfs.valid )
                return visitor::TerminateOnState;
            dfs.stats.addNode( dfs.graph(), st );
            dfs.ce_stack.push_front( st );
            dfs.extension( st ).on_stack = true;
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &dfs, Node from, Node to ) {
            dfs.stats.addEdge();
            if ( from.valid() && !dfs.graph().full( from ) &&
                 !dfs.graph().full( to ) && dfs.extension( to ).on_stack )
                dfs.toexpand.push_back( from );
            return visitor::FollowTransition;
        }

        static void finished( This &dfs, Node n ) {
            if ( dfs.graph().isAccepting( n ) ) { // run the nested search
                if ( dfs.parallel )
                    dfs.inner.process.push( n );
                else
                    dfs.runInner( dfs.graph(), n );
            }

            if ( !dfs.ce_stack.empty() ) {
                assert_eq( n.pointer(), dfs.ce_stack.front().pointer() );
                dfs.ce_stack.pop_front();
            }
        }
    };

    struct Inner : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &dfs, Node st ) {
            if ( !dfs.valid )
                return visitor::TerminateOnState;
            dfs.stats.addExpansion();
            dfs.ce_lasso.push_front( st );
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &dfs, Node from, Node to )
        {
            // The search always starts with a transition from "nowhere" into the
            // initial state. Ignore this transition here.
            if ( from.valid() && to.pointer() == dfs.seed.pointer() ) {
                dfs.valid = false;
                return visitor::TerminateOnTransition;
            }

            if ( !dfs.extension( to ).nested ) {
                dfs.extension( to ).nested = true;
                return visitor::ExpandTransition;
            }

            return visitor::FollowTransition;
        }

        static void finished( This &dfs, Node n ) {
            if ( !dfs.ce_lasso.empty() ) {
                assert_eq( n.pointer(), dfs.ce_lasso.front().pointer() );
                dfs.ce_lasso.pop_front();
            }
        }
    };

    NestedDFS( Meta m, bool = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        valid = true;
        this->init( this );
        parallel = m.execution.threads > 1;
        if (m.execution.threads > 2)
            progress() << "WARNING: Nested DFS uses only 2 threads." << std::endl;
        if ( parallel ) {
            progress() << "WARNING: Parallel Nested DFS uses a fixed-size hash table." << std::endl;
            progress() << "Using table size " << m.execution.initialTable
                       << ", please use -i to override." << std::endl;
            this->store().table.m_maxsize = m.execution.initialTable; // XXX
        }
        finished = false;
        inner.outer = this;
    }

};

}
}

#endif
