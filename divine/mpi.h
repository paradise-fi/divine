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
    typedef typename Algorithm::Shared Shared;
    Shared *shared;

    Mpi( Shared *sh, D *d ) : shared( sh )
    {
        m_domain = d;
        m_started = false;
    }

    void start() {
        m_started = true;
        MPI::Init();
        m_size = MPI::COMM_WORLD.Get_size();
        m_rank = MPI::COMM_WORLD.Get_rank();

        char *mpibuf = new char[ 1024 * 1024 ];
        MPI::Attach_buffer( mpibuf, 1024 * 1024 );

        if ( !master() ) {
            while ( true )
                slaveLoop();
        }
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

    void run( int id ) {
        // TDB. First distribute the shared bits ... Probably a
        // MPI::COMM_WORLD.Scatter is in place.

        // Btw, we want to use buffering sends. We need to use nonblocking
        // receive, since blocking receive is busy-waiting under openmpi.
        assert( shared );
        m_domain->parallel().run(
            *shared,
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
    }

};

template< typename D >
struct MpiThread : wibble::sys::Thread, Terminable {
    D &m_domain;
    int recv, sent;
    typedef typename D::Fifo Fifo;

    std::vector< Fifo > fifo;
    std::vector< std::vector< int32_t > > buffers;
    std::vector< int32_t > in_buffer;

    bool busy;

    MpiThread( D &d ) : m_domain( d ) {
        fifo.resize( d.peers() * d.peers() );
        buffers.resize( d.mpi.size() );
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
        in_buffer.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &in_buffer.front(), in_buffer.size() * 4,
                              MPI::BYTE,
                              MPI::ANY_SOURCE, MPI::ANY_TAG, status );
        std::vector< int32_t >::const_iterator i = in_buffer.begin();
        int count = 0;
        while ( i != in_buffer.end() ) {
            Blob b;
            int target = *i;
            /* std::cerr << "reading blob (off = "
                      << i - in_buffer.begin()
                      << ", total = "
                      << in_buffer.end() - in_buffer.begin()
                      << ", ct = "
                      << count
                      << "), for " << target << std::endl; */
            ++count;
            ++i;
            i = b.read32( &alloc, i );
            assert_pred( m_domain.isLocalId, target );
            m_domain.queue( -1, target ).push( b );
        }
        /* std::cerr << "MPI: " << m_domain.mpi.rank()
                  << " (" << target << ") <-- "
                  << status.Get_source()
                  << " [size = " << status.Get_count( MPI::CHAR )
                  << ", word0 = " << b.get< uint32_t >()
                  << "]" << std::endl; */
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

        // Fill outgoing buffers from the incoming FIFO queues...
        for ( int i = 0; i < fifo.size(); ++i ) {
            int to = i / m_domain.peers(),
              rank = to / m_domain.n;
            if ( m_domain.isLocalId( to ) ) {
                assert( fifo[ i ].empty() );
                continue;
            }

            // The size limit for these buffers is about .8M -- the hard limit
            // in MPI is set above to 1M, so if state size is on the order of
            // 200k, we might have a problem (trying to Bsend a message bigger
            // than MPI buffer size kills the application, sadly).
            while ( !fifo[ i ].empty() && buffers[ rank ].size() <= 200 * 1024 )
            {
                Blob &b = fifo[ i ].front();
                fifo[ i ].pop();
                buffers[ rank ].push_back( to );
                b.write32( std::back_inserter( buffers[ rank ] ) );
            }
        }

        // ... and flush the buffers.
        for ( int to = 0; to < buffers.size(); ++ to ) {
            if ( buffers[ to ].empty() )
                continue;
            MPI::COMM_WORLD.Bsend( &buffers[ to ].front(),
                                   buffers[ to ].size() * 4,
                                   MPI::BYTE, to, TAG_ID );
            buffers[ to ].clear();
            ++ sent;
        }

        // And process incoming MPI traffic.
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
        // The allocator needs to be instantiated here, since this depends on
        // current thread's ID. Yes, icky.
        Allocator< char > alloc;

        std::cerr << "MPI domain started: " << m_domain.mpi.rank() << std::endl;
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
    Mpi( typename M::Shared *, D * ) {}
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
