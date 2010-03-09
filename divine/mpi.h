// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/test.h>
#include <divine/pool.h>

#ifndef DIVINE_MPI_H
#define DIVINE_MPI_H

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace divine {

#define TAG_ALL_DONE    0
#define TAG_RUN         1
#define TAG_GET_COUNTS  2
#define TAG_GIVE_COUNTS 4
#define TAG_DONE        5
#define TAG_SHARED      6
#define TAG_INTERRUPT   7
#define TAG_RING_RUN    8
#define TAG_RING_DONE   9
#define TAG_SOLICIT_SHARED 10

#define TAG_ID           100

namespace algorithm {

template< typename T >
struct _MpiId
{
    static int to_id( void (T::*f)() ) {
        // assert( 0 );
        return -1;
    }

    static void (T::*from_id( int ))() {
        // assert( 0 );
        return 0;
    }

    template< typename O >
    static void writeShared( typename T::Shared, O ) {
    }

    template< typename I >
    static I readShared( typename T::Shared &, I i ) {
        return i;
    }
};

}

#ifdef HAVE_MPI

/**
 * A global per-mpi-node structure that manages the low-level aspects of MPI
 * communication. Each node should call start() when it is ready to do so
 * (i.e. the local data structures are set up and the algorithm is about to be
 * started). On the master node, start() returns control to its caller after
 * setting up MPI. However, on slave (non-master) nodes, it seizes the control
 * and never returns. The slave nodes then wait for commands from the master.
 *
 * The MpiThread (see below) is designed to plug into the Domain framework (see
 * parallel.h) and to relay the parallel-start and the termination detection
 * over MPI to the slave nodes.
 */
template< typename Algorithm, typename D >
struct Mpi {

    bool m_started;
    bool is_master; // the master token
    int m_rank, m_size;
    D *m_domain;
    typedef typename Algorithm::Shared Shared;
    Shared *master_shared;
    std::vector< Shared > m_shared;

    D &domain() {
        assert( m_domain );
        return *m_domain;
    }

    Mpi( Shared *sh, D *d ) : master_shared( sh )
    {
        m_domain = d;
        is_master = false;
        m_started = false;
    }

    Shared &shared( int i ) {
        return m_shared[ i ];
    }

    void start() {
        m_started = true;
        MPI::Init();
        m_size = MPI::COMM_WORLD.Get_size();
        m_rank = MPI::COMM_WORLD.Get_rank();

        m_shared.resize( domain().peers() );

        if ( m_rank == 0 )
            is_master = true;

        if ( !master() )
            while ( true )
                slaveLoop();
    }

    void notifyOne( int i, int tag, int id ) {
        MPI::COMM_WORLD.Send( &id, 1, MPI::INT, i, tag );
    }

    void notifySlaves( int tag, int id ) {
        if ( !master() )
            return;
        notify( tag, id );
    }

    void notify( int tag, int id ) {
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                notifyOne( i, tag, id );
        }
    }

    void returnSharedBits( int to ) {
        std::vector< int32_t > shbits;
        for ( int i = 0; i < domain().n; ++i ) {
            algorithm::_MpiId< Algorithm >::writeShared(
                domain().parallel().shared( i ),
                std::back_inserter( shbits ) );
        }
        MPI::COMM_WORLD.Send( &shbits.front(),
                              shbits.size() * 4,
                              MPI::BYTE, to, TAG_SHARED );
    }

    void collectSharedBits() {
        if (!master())
            return;

        MPI::Status status;
        std::vector< int32_t > shbits;
        notifySlaves( TAG_SOLICIT_SHARED, 0 );
        for ( int i = 0; i < size(); ++ i ) {
            if ( i == rank() )
                continue;
            MPI::COMM_WORLD.Probe( i, TAG_SHARED, status );
            shbits.resize( status.Get_count( MPI::BYTE ) / 4 );
            MPI::COMM_WORLD.Recv( &shbits.front(), shbits.size() * 4,
                                  MPI::BYTE, i, TAG_SHARED, status );
            std::vector< int32_t >::const_iterator it = shbits.begin();
            for ( int k = 0; k < domain().n; ++k ) {
                it = algorithm::_MpiId< Algorithm >::readShared(
                    m_shared[ i * domain().n + k ], it );
            }
            assert( it == shbits.end() ); // sanity check
        }
    }

    Shared obtainSharedBits() {
        MPI::Status status;
        std::vector< int32_t > shbits;
        Shared sh = *master_shared;

        MPI::COMM_WORLD.Probe( MPI::ANY_SOURCE, TAG_SHARED, status );
        shbits.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &shbits.front(), shbits.size() * 4,
                              MPI::BYTE, MPI::ANY_SOURCE, TAG_SHARED, status );
        algorithm::_MpiId< Algorithm >::readShared( sh, shbits.begin() );
        return sh;
    }

    void sendSharedBits( int to, const Shared &sh ) {
        std::vector< int32_t > shbits;
        algorithm::_MpiId< Algorithm >::writeShared(
            sh, std::back_inserter( shbits ) );
        MPI::COMM_WORLD.Send( &shbits.front(),
                              shbits.size() * 4,
                              MPI::BYTE, to, TAG_SHARED );
    }

    template< typename F >
    void runInRing( F f ) {
        if ( rank() != 0 ) return;
        if ( size() <= 1 ) return;

        is_master = false;
        notifyOne( 1, TAG_RING_RUN,
                   algorithm::_MpiId< Algorithm >::to_id( f ) );
        sendSharedBits( 1, *master_shared );

        while( slaveLoop() ); // wait (and serve) till the ring is done
        is_master = true;
    }

    template< typename Shared, typename F >
    void runOnSlaves( Shared sh, F f ) {
        if ( !master() ) return;
        if ( size() <= 1 ) return; // master_shared can be NULL otherwise
        notifySlaves( TAG_RUN,
                      algorithm::_MpiId< Algorithm >::to_id( f ) );
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                sendSharedBits( i, sh );
        }
    }

    ~Mpi() {
        if ( m_started && master() ) {
            notifySlaves( TAG_ALL_DONE, 0 );
            MPI::Finalize();
        }
    }

    int rank() { if ( m_started ) return m_rank; else return 0; }
    int size() { if ( m_started ) return m_size; else return 1; }

    bool master() { return is_master; }

    // Default copy and assignment is fine for us.

    void runSerial( int id ) {
        is_master = true;
        *master_shared = obtainSharedBits();
        domain().parallel().runInRing(
            *master_shared, algorithm::_MpiId< Algorithm >::from_id( id ) );
        is_master = false;
        if ( rank() < size() - 1 )
            notifyOne( rank() + 1, TAG_RING_RUN, id );
        else
            notifyOne( 0, TAG_RING_DONE, 0 );
        sendSharedBits( ( rank() + 1 ) % size(), *master_shared );
    }

    void run( int id ) {
        Shared sh = obtainSharedBits();
        domain().parallel().run(
            sh, algorithm::_MpiId< Algorithm >::from_id( id ) );
    }

    bool slaveLoop() {
        MPI::Status status;
        int id;
        MPI::COMM_WORLD.Recv( &id, 1, /* one integer per message */
                              MPI::INT,
                              MPI::ANY_SOURCE, /* anyone can become a master these days */
                              MPI::ANY_TAG, status );
        switch ( status.Get_tag() ) {
            case TAG_ALL_DONE:
                MPI::Finalize();
                exit( 0 ); // after a fashion...
            case TAG_RING_RUN:
                runSerial( id );
                break;
            case TAG_RING_DONE:
                assert( master_shared );
                *master_shared = obtainSharedBits();
                return false;
            case TAG_RUN:
                run( id );
                break;
            case TAG_SOLICIT_SHARED:
                returnSharedBits( status.Get_source() );
                break;
            default:
                assert( 0 );
        }
        return true;
    }

};

/**
 * A high-level MPI bridge. This structure is designed to integrate into the
 * Domain framework (see parallel.h). The addExtra mechanism of Parallel can be
 * used to plug in the MpiThread into a Parallel setup. The MpiThread then
 * takes care to relay
 */
template< typename D >
struct MpiThread : wibble::sys::Thread, Terminable {
    D &m_domain;
    int recv, sent;
    typedef typename D::Fifo Fifo;

    Pool pool;
    std::vector< Fifo > fifo;
    std::vector< std::vector< int32_t > > buffers;
    std::vector< std::pair< bool, MPI::Request > > requests;
    std::vector< int32_t > in_buffer;

    MpiThread( D &d ) : m_domain( d ) {
        fifo.resize( d.peers() * d.peers() );
        buffers.resize( d.mpi.size() );
        requests.resize( d.mpi.size() );
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
        for ( int i = 0; i < mpi.size(); ++ i ) {

            if ( i == mpi.rank() )
                continue;

            int cnt[3];
            MPI::COMM_WORLD.Recv( cnt, 3, MPI::INT,
                                  MPI::ANY_SOURCE, TAG_GIVE_COUNTS,
                                  status );
            if ( !cnt[0] )
                valid = false;
            s += cnt[1];
            r += cnt[2];
        }
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

    void interrupt() {
        m_domain.mpi.notify( TAG_INTERRUPT, 0 );
        sent += m_domain.mpi.size() - 1;
    }

    void receiveDataMessage( MPI::Status &status )
    {
        Blob b;
        int target;

        in_buffer.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &in_buffer.front(), in_buffer.size() * 4,
                              MPI::BYTE,
                              status.Get_source(),
                              status.Get_tag(), status );
        std::vector< int32_t >::const_iterator i = in_buffer.begin();

        // FIXME. This here hardcoding of pairwise blob relationship is rather
        // evil, but necessary for correctness with regards to parallel visitor
        // implementation. It is however a modularity violation and this whole
        // pairing mess needs to be addressed somehow.
        while ( i != in_buffer.end() ) {
            target = *i++;
            assert_pred( m_domain.isLocalId, target );
            i = b.read32( &pool, i );
            m_domain.queue( -1, target ).push( b );
            i = b.read32( &pool, i );
            m_domain.queue( -1, target ).push( b );
        }
        ++ recv;
    }

    bool receiveControlMessage( MPI::Status &status ) {
        int id;
        MPI::COMM_WORLD.Recv( &id, 1, MPI::INT,
                              status.Get_source(),
                              status.Get_tag(), status );
        switch ( status.Get_tag() ) {
            case TAG_GET_COUNTS:
                int cnt[3];
                cnt[0] = outgoingEmpty() && lastMan();
                cnt[1] = sent;
                cnt[2] = recv;
                MPI::COMM_WORLD.Send( cnt, 3, MPI::INT,
                                      status.Get_source(), TAG_GIVE_COUNTS );
                return false;
            case TAG_DONE:
                return m_domain.barrier().idle( this );
            case TAG_INTERRUPT:
                ++ recv;
                m_domain.interrupt( true );
                return false;
            default:
                assert_die();
        }
    }

    bool lastMan() {
        bool r = m_domain.barrier().lastMan( this );
        return r;
    }

    bool loop() {
        MPI::Status status;

        // Fill outgoing buffers from the incoming FIFO queues...
        for ( int i = 0; i < fifo.size(); ++i ) {
            int to = i / m_domain.peers(),
              rank = to / m_domain.n;
            if ( m_domain.isLocalId( to ) ) {
                assert( fifo[ i ].empty() );
                continue;
            }

            // Build up linear buffers for MPI send. We only do that on
            // buffers that are not currently in-flight (request not busy).
            while ( !fifo[ i ].empty() && !requests[ rank ].first )
            {
                buffers[ rank ].push_back( to );

                Blob b = fifo[ i ].front();
                fifo[ i ].pop();
                b.write32( std::back_inserter( buffers[ rank ] ) );
                b.free( pool );

                b = fifo[ i ].front( true );
                fifo[ i ].pop();
                b.write32( std::back_inserter( buffers[ rank ] ) );
                b.free( pool );
            }
        }

        // ... and flush the buffers.
        for ( int to = 0; to < buffers.size(); ++ to ) {
            if ( requests[ to ].first ) {
                if ( requests[ to ].second.Test() ) {
                    buffers[ to ].clear();
                    requests[ to ].first = false;
                }
                continue;
            }

            if ( buffers[ to ].empty() )
                continue;

            requests[ to ].first = true;
            requests[ to ].second = MPI::COMM_WORLD.Isend(
                    &buffers[ to ].front(),
                    buffers[ to ].size() * 4,
                    MPI::BYTE, to, TAG_ID );
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
                receiveDataMessage( status );
        }

        // NB. The call to lastMan() here is first, since it needs to be done
        // by non-master nodes, to wake up sleeping workers that have received
        // messages from network. Ugly, yes.
        if ( lastMan() && m_domain.mpi.master() && outgoingEmpty() ) {
            if ( termination() ) {
                return false;
            }
        }

        return true;
    }

    void *main() {
        m_domain.barrier().started( this );
        while ( loop() );
        return 0;
    }
};

#else

template< typename M, typename D >
struct Mpi {
    int rank() { return 0; }
    int size() { return 1; }
    void notifySlaves( int, int ) {}
    void collectSharedBits() {}

    template< typename Shared, typename F >
    void runOnSlaves( Shared, F ) {}

    template< typename F >
    void runInRing( F f ) {}

    void start() {}
    typename M::Shared &shared( int i ) {
        assert_die();
    }
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

    void interrupt() {}

    void *main() {
        assert_die();
    }

    MpiThread( D& ) {}
};

#endif

}

#endif
