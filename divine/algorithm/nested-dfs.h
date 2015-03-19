// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>
//             (c) 2013, 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/algorithm/por-c3.h>
#include <divine/graph/ltlce.h>
#include <atomic>

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
    typedef typename Graph::Label Label;
    typedef typename Setup::Store Store;
    typedef typename Store::Vertex Vertex;
    typedef typename Store::Handle Handle;

    Vertex seed;
    bool parallel;
    std::atomic< bool > valid, finished;

    std::deque< Handle > ce_stack;
    std::deque< Handle > ce_lasso;
    std::deque< Handle > toexpand;

    algorithm::Statistics stats;

    struct Extension {
        bool nested() { return _data & _nestedMask; }
        bool onStack() { return _data & _onStackMask; }

        void setNested() { _data |= _nestedMask; }
        void setOnStack() { _data |= _onStackMask; }
      private:
        typename Store::template DataWrapper< uint8_t > _data;
        static constexpr uint8_t _nestedMask = 0x1;
        static constexpr uint8_t _onStackMask = 0x2;
    };

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
    }

    int id() { return 0; } // expected by AlgorithmUtils

    inline Pool& pool() {
        return this->graph().pool();
    }

    void runInner( Graph &graph, Vertex n, Store *store = nullptr ) {
        seed = n;
        visitor::DFVReadOnly< Inner > visitor( *this, graph, store ? *store : this->store() );
        visitor.exploreFrom( n.node() );
        n.disown();
    }

    struct : brick::shmem::Thread {
        brick::shmem::Fifo< Handle > process;
        This *outer;
        std::unique_ptr< Graph > graph;
        std::unique_ptr< Store > store;

        void main() {
            while ( outer->valid.load( std::memory_order_relaxed ) ) {
                if ( !process.empty() ) {
                    auto n = process.front();
                    process.pop();
                    // run the inner loop
                    outer->runInner( *graph, store->vertex( n ), store.get() );
                } else {
                    if ( outer->finished && process.empty() )
                        return;
                    std::this_thread::yield();
                }
            }
        }
    } inner;

    void counterexample() {
        progress() << "generating counterexample... " << std::flush;
        typedef LtlCE< Setup, Unit, Unit, Unit > CE;
        CE ce( this );
        auto ceStack = ce.succTraceLocal( *this, typename CE::Linear(), Node(),
                                          ce_stack.rbegin(), ce_stack.rend() );
        ce.generateLinear( *this, this->graph(), ceStack );
        auto ceLasso = ce.succTraceLocal( *this, typename CE::Lasso(),
                                          ce.traceBack( ceStack ),
                                          ce_lasso.rbegin(), ce_lasso.rend() );
        ce.generateLasso( *this, this->graph(), ceLasso );
        progress() << "done" << std::endl;
        result().ceType = meta::Result::CET::Cycle;
    }

    void run() {
        progress() << " searching...\t\t\t" << std::flush;

        if ( parallel ) {
            inner.graph->setSlack( this->m_slack );
            inner.start();
        }

        visitor::DFV< Outer > visitor( *this, this->graph(), this->store() );
        this->graph().initials( this->store().alloc, [&]( Node f, Node t, Label l ) {
                ASSERT( !this->store().valid( f ) );
                visitor.queue( Vertex(), t, l );
        } );
        visitor.processQueue();

        while ( valid.load( std::memory_order_relaxed ) && !toexpand.empty() ) {
            auto from = this->store().vertex( toexpand.front() );
            if ( !this->graph().full( from ) )
                this->graph().fullexpand( this->store().alloc, [&]( Vertex, Node t, Label ) {
                        visitor::DFV< Outer > visitor( *this, this->graph(), this->store() );
                        visitor.exploreFrom( t );
                    }, from );
            toexpand.pop_front();
        }

        finished = true;
        if ( parallel ) {
            inner.join();
            ASSERT( !valid || inner.process.empty() );
        }

        progress() << "done" << std::endl;

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
        result().propertyHolds = valid ? meta::Result::R::Yes : meta::Result::R::No;
        result().fullyExplored = valid ? meta::Result::R::Yes : meta::Result::R::No;

        resultBanner( valid );
    }

    struct Outer : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &dfs, Vertex st ) {
            if ( !dfs.valid.load( std::memory_order_relaxed ) )
                return visitor::ExpansionAction::Terminate;
            dfs.stats.addNode( dfs.graph(), st );
            dfs.ce_stack.push_front( st.handle() );
            dfs.extension( st ).setOnStack();
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &dfs, Vertex from, Vertex to, Label ) {
            dfs.stats.addEdge( dfs.store(), from, to );
            if ( dfs.store().valid( from ) && !dfs.graph().full( from ) &&
                 !dfs.graph().full( to ) && dfs.extension( to ).onStack() )
                dfs.toexpand.push_back( from.handle() );
            return visitor::TransitionAction::Follow;
        }

        static void finished( This &dfs, Vertex n ) {

            if ( dfs.graph().stateFlags( n.node(), graph::flags::isAccepting ) ) { // run the nested search
                if ( dfs.parallel )
                    dfs.inner.process.push( n.handle() );
                else
                    dfs.runInner( dfs.graph(), n );
            }

            if ( dfs.valid.load( std::memory_order_relaxed ) && !dfs.ce_stack.empty() ) {
                ASSERT_EQ( n.handle().asNumber(), dfs.ce_stack.front().asNumber() );
                dfs.ce_stack.pop_front();
            }
        }
    };

    struct Inner : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &dfs, Vertex st ) {

            if ( !dfs.valid.load( std::memory_order_relaxed ) )
                return visitor::ExpansionAction::Terminate;
            dfs.stats.addExpansion();
            dfs.ce_lasso.push_front( st.handle() );
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &dfs, Vertex from, Vertex to, Label )
        {
            // The search always starts with a transition from "nowhere" into the
            // initial state. Ignore this transition here.
            if ( dfs.pool().valid( from.node() ) &&
                 to.handle().asNumber() == dfs.seed.handle().asNumber() )
            {
                dfs.valid = false;
                return visitor::TransitionAction::Terminate;
            }

            if ( !dfs.extension( to ).nested() ) {
                dfs.extension( to ).setNested();
                return visitor::TransitionAction::Expand;
            }

            return visitor::TransitionAction::Follow;
        }

        static void finished( This &dfs, Vertex n ) {

            if ( !dfs.ce_lasso.empty() ) {
                ASSERT_EQ( n.handle().asNumber(), dfs.ce_lasso.front().asNumber() );
                dfs.ce_lasso.pop_front();
            }
        }
    };

    NestedDFS( Meta m ) : Algorithm( m, sizeof( Extension ) )
    {
        valid = true;
        this->init( *this );
        parallel = m.execution.threads > 1;
        if (m.execution.threads > 2)
            progress() << "WARNING: Nested DFS uses only 2 threads." << std::endl;
        if ( parallel ) {
            this->store().setSize( m.execution.initialTable ); // XXX
            inner.graph = std::unique_ptr< Graph >( this->initGraph( *this ) );
            inner.graph->setPool( this->pool() ); // copy the pool (rerefence to lake)
            inner.store = std::unique_ptr< Store >(
                    new Store( *inner.graph, inner.graph->base().slack(), &this->store() ) );
        }
        finished = false;
        inner.outer = this;
    }

};

}
}

#endif
