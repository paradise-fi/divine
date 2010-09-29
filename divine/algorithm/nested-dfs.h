// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/porcp.h>
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
    bool parallel, finished;

    std::deque< Node > ce_stack;
    std::deque< Node > ce_lasso;
    std::deque< Node > toexpand;

    algorithm::Statistics< G > stats;

    struct Extension {
        bool nested:1;
        bool on_stack:1;
    };

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    void runInner( G &graph, Node n ) {
        seed = n;
        visitor::DFV< InnerVisit > visitor( graph, *this, &table() );
        visitor.exploreFrom( n );
    }

    struct : wibble::sys::Thread {
        Fifo< Node > process;
        NestedDFS< G, Statistics > *outer;
        G graph;

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
        typedef LtlCE< G, Unit, Unit > CE;
        CE::generateLinear( *this, g, ce_stack );
        CE::generateLasso( *this, g, ce_lasso );
        progress() << "done" << std::endl;
    }

    // this is the entrypoint for full expansion... I know the name isn't best,
    // but that's what PORGraph uses
    void queue( Node from, Node to ) {
        visitor::DFV< OuterVisit > visitor( g, *this, &table() );
        visitor.exploreFrom( to );
    }

    Result run() {
        progress() << " searching...\t\t\t" << std::flush;

        if ( parallel )
            inner.start();

        visitor::DFV< OuterVisit > visitor( g, *this, &table() );
        visitor.exploreFrom( g.initial() );

        while ( valid && !toexpand.empty() ) {
            if ( !g.full( toexpand.front() ) )
                g.fullexpand( *this, toexpand.front() );
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
        extension( st ).on_stack = true;
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
        if ( from.valid() && !g.full( from ) && !g.full( to ) && extension( to ).on_stack )
            toexpand.push_back( from );
        return visitor::FollowTransition;
    }

    visitor::TransitionAction innerTransition( Node from, Node to )
    {
        if ( to.pointer() == seed.pointer() ) {
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

            if ( dfs.g.isAccepting( n ) ) { // run the nested search
                if ( dfs.parallel )
                    dfs.inner.process.push( n );
                else
                    dfs.runInner( dfs.g, n );
            }

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
        parallel = c->workers() > 1;
        *m_initialTable = c->initialTableSize();
        if ( parallel ) {
            progress() << "WARNING: Parallel Nested DFS uses a fixed-size hash table." << std::endl;
            progress() << "Using table size " << c->initialTableSize()
                       << ", please use -i to override." << std::endl;
            table().m_maxsize = c->initialTableSize();
        }
        finished = false;
        inner.outer = this;
        inner.graph = g;
    }

};

}
}

#endif
