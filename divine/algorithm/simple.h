// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h> // for stats
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_SIMPLE_H
#define DIVINE_SIMPLE_H

namespace divine {
namespace algorithm {

template< typename > struct Simple;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G >
struct _MpiId< Simple< G > >
{
    typedef Simple< G > A;

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

template< typename G >
struct Simple : Algorithm, DomainWorker< Simple< G > >
{
    typedef Simple< G > This;
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;

    struct Shared {
        algorithm::Statistics< G > stats;
        G g;
        int initialTable;
    } shared;

    std::deque< Successors > localqueue;

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

    int owner( Node n ) {
        return hasher( n ) % this->peers();
    }

    void edge( Node from, Node to ) {
        hash_t hint = table().hash( to );
        int owner = hint % this->peers();

        if ( owner != this->globalId() ) { // send to remote
            divine::Fifo< Node > &fifo = this->queue( this->globalId(), owner );
            fifo.push( from );
            fifo.push( to );
        } else { // we own this node, so let's process it
            Node in_table = table().get( to, hint ).key;

            shared.stats.addEdge();
            if ( !table().valid( in_table ) ) {
                table().insert( to, hint );
                to.header().permanent = 1; // don't ever release this
                localqueue.push_back( shared.g.successors( to ) );
                shared.stats.addNode( shared.g, to );
            } else {
                shared.g.release( to );
                to = in_table;
            }
            // from now on "to" always refers to the node in the hash table
            // although in this case we don't do anything with it...
        }
    }

    void _visit() { // parallel
        m_initialTable = &shared.initialTable; // XXX find better place for this
        Node initial = shared.g.initial();
        if ( owner( initial ) == this->globalId() ) {
            shared.stats.addNode( shared.g, initial );
            localqueue.push_back( shared.g.successors( initial ) );
        } else
            shared.g.release( initial );

        do {
            // process incoming stuff from other workers
            while ( !this->fifo.empty() ) {
                Node f, t;
                f = this->fifo.next();
                this->fifo.remove();
                t = this->fifo.next( true );
                this->fifo.remove();
                edge( f, t );
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
        } while ( !this->idle() ); // until termination detection succeeds
    }

    Simple( Config *c = 0 )
        : Algorithm( c, 0 )
    {
        initGraph( shared.g );
        if ( c ) {
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }

    Result run() {
        progress() << "  exploring... \t\t\t\t" << std::flush;
        domain().parallel().run( shared, &This::_visit );
        progress() << "   done" << std::endl;

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
