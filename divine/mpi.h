// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_MPI_H
#define DIVINE_MPI_H

#include <wibble/test.h>
#include <divine/algorithm.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace divine {

#define TAG_ALL_DONE    0
#define TAG_RUN         1
#define TAG_GET_COUNTS  2
#define TAG_GIVE_COUNTS 4
#define TAG_DONE        5
#define TAG_ID          6

#ifdef HAVE_MPI

template< typename Algorithm, typename D >
struct Mpi {

    bool m_started;
    int m_rank, m_size;
    D *m_domain;

    Mpi( D *d ) {
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
        if ( !master() )
            return;
        for ( int i = 1; i < size(); ++i ) {
            /*  std::cerr << "MPI: Notify: tag = " << tag
                      << ", id = " << id << ", target = " << i
                      << std::endl; */
            MPI::COMM_WORLD.Send( &id, 1, MPI::INT, i, tag );
        }
    }

    ~Mpi() {
        if ( m_started && master() ) {
            std::cerr << "MPI: Teardown start." << std::endl;
            notifySlaves( TAG_ALL_DONE, 0 );
            MPI::Finalize();
            std::cerr << "MPI: Teardown end." << std::endl;
        }
    }

    int rank() { if ( m_started ) return m_rank; else return 0; }
    int size() { if ( m_started ) return m_size; else return 1; }

    bool master() {
        return rank() == 0;
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

        // Btw, we want to use buffering sends. We need to use nonblocking
        // receive, since blocking receive is busy-waiting under openmpi.
        typename Algorithm::Shared shared;
        m_domain->parallel().run(
            shared,
            algorithm::_MpiId< Algorithm >::from_id( id ) );

        // TBD. we need to collect the shared data now; maybe a call to
        // MPI::COMM_WORLD.Gather would help.
    }

    void slaveLoop() {
        MPI::Status status;
        int id;
        std::cerr << "MPI-Slave: Waiting." << std::endl;
        MPI::COMM_WORLD.Recv( &id, 1 /* one integer per message */,
                              MPI::INT, 0 /* from master */,
                              MPI::ANY_TAG, status );
        std::cerr << "MPI-Slave: Notified. Tag = " << status.Get_tag()
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

template< typename D >
struct MpiThread : wibble::sys::Thread, Terminable {
    D &m_domain;
    int recv, sent;
    typedef typename D::Fifo Fifo;

    std::vector< Fifo > fifo;
    bool busy;

    MpiThread( D &d ) : m_domain( d ) {
        fifo.resize( d.peers() * d.peers() );
        busy = true;
        sent = recv = 0;
    }

    bool outgoingEmpty() {
        for ( int i = 0; i < fifo.size(); ++i )
            if ( !fifo[ i ].empty() )
                return false;
        return true;
    }

    bool workWaiting() {
        return !outgoingEmpty();
    }

    template< typename Mpi >
    std::pair< int, int > accumCounts( Mpi &mpi, int id ) {
        mpi.notifySlaves( TAG_GET_COUNTS, id );
        MPI::Status status;
        int r = recv, s = sent;
        bool valid = true;
        for ( int i = 1; i < mpi.size(); ++ i ) {
            int cnt[3];
            MPI::COMM_WORLD.Recv( cnt, 3, MPI::INT,
                                  MPI::ANY_SOURCE, TAG_GIVE_COUNTS,
                                  status );
            if ( !cnt[0] )
                valid = false;
            s += cnt[1];
            r += cnt[2];
        }
        std::cerr << "termination phase "
                  << id << " (" << (valid ? "" : "in") << "valid) counts: "
                  << r << ", " << s << std::endl;
        return valid ? std::make_pair( r, s ) : std::make_pair( 0, 1 );
    }

    bool termination() {
        std::pair< int, int > one, two;

        one = accumCounts( m_domain.mpi, 0 );

        if ( one.first != one.second )
            return false;

        two = accumCounts( m_domain.mpi, 1 );

        if ( one.first == two.first && two.first == two.second ) {
            m_domain.mpi.notifySlaves( TAG_DONE, 0 );

            if ( m_domain.barrier().idle( this ) )
                return true;
        }

        return false;
    }

    void receiveDataMessage( Allocator< char > &alloc, MPI::Status &status ) {
        Blob b( &alloc, status.Get_count( MPI::CHAR ) );
        MPI::COMM_WORLD.Recv( b.data(), status.Get_count( MPI::CHAR ),
                              MPI::CHAR,
                              MPI::ANY_SOURCE, MPI::ANY_TAG, status );
        int target = status.Get_tag() - TAG_ID;
        assert( m_domain.isLocalId( target ) );
        /* std::cerr << "MPI: " << m_domain.mpi.rank()
                  << " (" << target << ") <-- "
                  << status.Get_source()
                  << " [size = " << status.Get_count( MPI::CHAR )
                  << ", word0 = " << b.get< uint32_t >()
                  << "]" << std::endl; */
        m_domain.queue( -1, target ).push( b );
        ++ recv;
    }

    bool receiveControlMessage( MPI::Status &status ) {
        int id;
        MPI::COMM_WORLD.Recv( &id, 1, MPI::INT,
                              MPI::ANY_SOURCE, MPI::ANY_TAG, status );
        /* std::cerr << "MPI CONTROL: " << m_domain.mpi.rank()
                  << " <-- "
                  << status.Get_source()
                  << " [tag = " << status.Get_tag() << ", id = " << id << "]" << std::endl; */
        switch ( status.Get_tag() ) {
            case TAG_GET_COUNTS:
                int cnt[3];
                cnt[0] = outgoingEmpty() && lastMan();
                cnt[1] = sent;
                cnt[2] = recv;
                MPI::COMM_WORLD.Send( cnt, 3, MPI::INT, 0, TAG_GIVE_COUNTS );
                return false;
            case TAG_DONE:
                return m_domain.barrier().idle( this );
            default:
                assert_die();
        }
    }

    bool lastMan() {
        bool r = m_domain.barrier().lastMan( this );
        return r;
    }

    bool loop( Allocator< char > &alloc ) {
        MPI::Status status;

        for ( int i = 0; i < fifo.size(); ++i ) {
            if ( m_domain.isLocalId( i ) ) {
                assert( fifo[ i ].empty() );
                continue;
            }
            while ( !fifo[ i ].empty() ) {
                Blob &b = fifo[ i ].front();
                fifo[ i ].pop();
                // might make sense to use ISend and waitall after the loop
                /* std::cerr << "MPI: " << m_domain.mpi.rank()
                          << " --> " << i / m_domain.n << " ("
                          << i << ")" << " [size = " << b.size()
                          << ", word0 = " << b.get< uint32_t >()
                          << "]" << std::endl; */

                MPI::COMM_WORLD.Send( b.data(), b.size(),
                                      MPI::CHAR, i / m_domain.n,
                                      TAG_ID + i );
                // b.free( &alloc );
                ++ sent;
            }
        }

        while ( MPI::COMM_WORLD.Iprobe(
                    MPI::ANY_SOURCE, MPI::ANY_TAG, status ) )
        {
            if ( status.Get_tag() < TAG_ID ) {
                if ( receiveControlMessage( status ) )
                    return false;
            } else
                receiveDataMessage( alloc, status );
        }

        // NB. The call to lastMan() here is first, since it needs to be done
        // by non-master nodes, to wake up sleeping workers that have received
        // messages from network. Ugly, yes.
        if ( lastMan() && m_domain.mpi.master() && outgoingEmpty() ) {
            if ( termination() ) {
                std::cerr << "MPI: Master terminated." << std::endl;
                return false;
            }
        }

        return true;
    }

    void *main() {
        Allocator< char > alloc; // needs to be here, since this depends on current thread's ID.

        std::cerr << "domain started: " << m_domain.mpi.rank() << std::endl;
        m_domain.barrier().started( this );
        while ( loop( alloc ) );
        return 0;
    }
};

#else

template< typename M, typename D >
struct Mpi {
    int rank() { return 0; }
    int size() { return 1; }
    void notifySlaves( int, int ) {}
    void start() {}
    Mpi( D * ) {}
};

template< typename D >
struct MpiThread : wibble::sys::Thread {
    struct Fifos {
        Fifo< Blob, NoopMutex > &operator[]( int ) __attribute__((noreturn)) {
            assert_die();
        }
    };

    Fifos fifo;

    void *main() {
        assert_die();
    }

    MpiThread( D& ) {}
};

#endif

}

#endif
