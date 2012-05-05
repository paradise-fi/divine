// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h> // for stats
#include <divine/visitor.h>
#include <divine/parallel.h>

#ifndef DIVINE_ALGORITHM_SIMPLE_H
#define DIVINE_ALGORITHM_SIMPLE_H

namespace divine {
namespace algorithm {

template< typename G, template< typename > class Topology, typename Stats >
struct Simple : Algorithm, AlgorithmUtils< G >, Parallel< Topology, Simple< G, Topology, Stats > >
{
    typedef Metrics< G, Topology, Stats > This;
    ALGORITHM_CLASS( G, algorithm::Statistics );
    typedef typename G::Successors Successors;

    std::deque< Successors > localqueue;

    int owner( Node n ) {
        return hasher( n ) % this->peers();
    }

    void edge( Node from, Node to ) {
        hash_t hint = this->table().hash( to );
        int owner = hint % this->peers();

        if ( owner != this->globalId() ) { // send to remote
            this->comms().submit( this->globalId(), owner, BlobPair( from, to ) );
        } else { // we own this node, so let's process it
            Node in_table = this->table().getHinted( to, hint );

            shared.addEdge();
            if ( !this->table().valid( in_table ) ) {
                this->table().insertHinted( to, hint );
                to.header().permanent = 1; // don't ever release this
                localqueue.push_back( m_graph.successors( to ) );
                shared.addNode( m_graph, to );
            } else {
                m_graph.release( to );
                to = in_table;
            }
            // from now on "to" always refers to the node in the hash table
            // although in this case we don't do anything with it...
        }
    }

    void _visit() { // parallel
        this->comms().notify( this->globalId(), &m_graph.pool() );
        Node initial = m_graph.initial();
        if ( owner( initial ) == this->globalId() ) {
            this->table().insert( initial );
            initial.header().permanent = 1;
            shared.addNode( m_graph, initial );
            localqueue.push_back( m_graph.successors( initial ) );
        } else
            m_graph.release( initial );

        do {
            // process incoming stuff from other workers
            for ( int from = 0; from < this->peers(); ++from ) {
                while ( this->comms().pending( from, this->globalId() ) ) {
                    std::pair< Node, Node > p;
                    p = this->comms().take( from, this->globalId() );
                    edge( p.first, p.second );
                }
            }

            // process local queue
            while ( !localqueue.empty() ) {
                Successors &succ = localqueue.front();
                while ( !succ.empty() ) {
                    edge( succ.from(), succ.head() );
                    succ = succ.tail();
                }
                localqueue.pop_front();
            }
        } while ( !this->idle() ) ; // until termination detection succeeds
    }

    Simple( Meta m, bool master = false )
        : Algorithm( m, 0 )
    {
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        this->init( this );
    }

    void run() {
        progress() << "  exploring... \t\t\t\t" << std::flush;
        this->parallel( &This::_visit );
        progress() << "   done" << std::endl;

        for ( int i = 0; i < shareds.size(); ++i ) {
            shared.merge( shareds[ i ] );
        }

        shared.print( progress() );

        result().fullyExplored = meta::Result::Yes;
        shared.update( meta().statistics );
    }
};

}
}

#endif
