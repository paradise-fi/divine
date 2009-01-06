// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_MPI_H
#define DIVINE_MPI_H

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace divine {

#ifdef HAVE_MPI

#define TAG_ALL_DONE 0
#define TAG_RUN 1

template< typename M, typename D >
struct Mpi {

    bool m_started;
    int m_rank, m_size;
    M *m_master;
    D *m_domain;

    Mpi( M *m, D *d ) {
        m_master = m;
        m_domain = d;
        m_started = false;
    }

    void start() {
        m_started = true;
        MPI::Init();
        m_size = MPI::COMM_WORLD.Get_size();
        m_rank = MPI::COMM_WORLD.Get_rank();
        startupSync();

        if ( !master() ) slaveLoop(); // never returns
    }

    void notifySlaves( int tag, int id ) {
        for ( int i = 1; i < m_size; ++i ) {
            std::cerr << "MPI: notify slave: tag = " << tag
                      << ", id = " << id << ", target = " << i
                      << std::endl;
            MPI::COMM_WORLD.Send( &id, 1, MPI::INT, i, tag );
        }
    }

    ~Mpi() {
        if ( m_started && master() ) {
            std::cerr << "MPI: Teardown..." << std::endl;
            notifySlaves( TAG_ALL_DONE, 0 );
            MPI::Finalize();
            std::cerr << "MPI: Done." << std::endl;
        }
    }

    int rank() { return m_rank; }
    int size() { return m_size; }

    bool master() {
        return m_rank == 0;
    }

    // Default copy and assignment is fine for us.

    void startupSync() {
        // TBD. all-to-all sync
    }
    
    void run( int id ) {
        // TBD. We want to add a "mpi-thread" to the domain, that handles
        // inter-node messaging and termination detection... Ie all mpi-thread
        // instances in a system become idle at once, after distributed
        // termination has finished. Ie: if ( dom.master().m_barrier.lastMan(
        // &dom ) ) { start distributed termination detection; if ( success &&
        // dom.master().m_barrier.idle( &dom ) ) { return; }

        // TDB. First distribute the shared bits ... Probably a
        // MPI::COMM_WORLD.Scatter is in place.

        // Btw, we want to use buffering sends.
        m_domain->parallel().run( m_master->_mpi_from_id( id ) );

        // TBD. we need to collect the shared data now; maybe a call to
        // MPI::COMM_WORLD.Gather would help.
    }

    void slaveLoop() {
        MPI::Status status;
        int id;
        std::cerr << "MPI-Slave: waiting..." << std::endl;
        MPI::COMM_WORLD.Recv( &id, 1 /* one integer per message */,
                              MPI::INT, 0 /* from master */,
                              MPI::ANY_TAG, status );
        std::cerr << "MPI-Slave: received tag " << status.Get_tag()
                  << ", id = " << id << std::endl;
        switch ( status.Get_tag() ) {
            case TAG_ALL_DONE:
                MPI::Finalize();
                exit( 0 ); // after a fashion...
            case TAG_RUN:
                run( id );
                break;
            default:
                assert( 0 );
        }
        slaveLoop(); // oops, tail recursion? : - )
    }

};

#else

template< typename M, typename D >
struct Mpi {
    int rank() { return 0; }
    int size() { return 1; }
    void start() {}
    Mpi( M *, D * ) {}
};

#endif

}

#endif
