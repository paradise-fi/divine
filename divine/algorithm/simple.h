// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h> // for stats

#ifndef DIVINE_ALGORITHM_SIMPLE_H
#define DIVINE_ALGORITHM_SIMPLE_H

namespace divine {
namespace algorithm {

template< typename Setup >
struct Simple : Algorithm, AlgorithmUtils< Setup >, Parallel< Setup::template Topology, Simple< Setup > >
{
    typedef Simple< Setup > This;
    ALGORITHM_CLASS( Setup, algorithm::Statistics );

    std::deque< Node > localqueue;

    int owner( Node n ) {
        return this->store().hash( n ) % this->peers();
    }

    void edge( Node from, Node to ) {
        hash_t hint = this->store().hash( to );
        int owner = hint % this->peers();

        if ( owner != this->id() ) { // send to remote
            this->comms().submit( this->id(), owner, std::make_pair( from, to ) );
        } else { // we own this node, so let's process it
            Node in_table = std::get< 0 >( this->store().fetch( to, hint ) );

            shared.addEdge( this->graph(), from, to );
            if ( !this->store().valid( in_table ) ) {
                this->store().store( to, hint );
                to.header().permanent = 1; // don't ever release this
                localqueue.push_back( to );
                shared.addNode( this->graph(), to );
            } else {
                this->graph().release( to );
                to = in_table;
            }
            // from now on "to" always refers to the node in the hash table
            // although in this case we don't do anything with it...
        }
    }

    void _visit() { // parallel
        this->comms().notify( this->id(), &this->graph().pool() );
        Node initial = this->graph().initial();
        if ( owner( initial ) == this->id() ) {
            this->store().store( initial, this->store().hash( initial ) );
            initial.header().permanent = 1;
            shared.addNode( this->graph(), initial );
            localqueue.push_back( initial );
        } else
            this->graph().release( initial );

        do {
            // process incoming stuff from other workers
            for ( int from = 0; from < this->peers(); ++from ) {
                while ( this->comms().pending( from, this->id() ) ) {
                    std::pair< Node, Node > p;
                    p = this->comms().take( from, this->id() );
                    edge( p.first, p.second );
                }
            }

            // process local queue
            while ( !localqueue.empty() ) {
                this->graph().successors( localqueue.front(), [&]( Node n, Label ) {
                        this->edge( localqueue.front(), n );
                    } );
                localqueue.pop_front();
            }
        } while ( !this->idle() ) ; // until termination detection succeeds
    }

    Simple( Meta m ) : Algorithm( m, 0 )
    {
        this->init( this );
        this->becomeMaster( m.execution.threads, this );
    }

    Simple( Simple *master ) : Algorithm( master->meta(), 0 )
    {
        this->init( this, master );
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

ALGORITHM_RPC( Simple );
ALGORITHM_RPC_ID( Simple, 1, _visit );

}
}

#endif
