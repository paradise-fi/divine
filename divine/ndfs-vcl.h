// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <divine/ndfs.h>
#include <divine/legacy/common/process_decomposition.hh>

#ifndef DIVINE_NDFS_VCL_H
#define DIVINE_NDFS_VCL_H
#if 0

namespace divine {
namespace ndfsVcl {

enum ComponentType {
    NonAccepting = 0,
    PartiallyAccepting = 1,
    FullyAccepting = 2,
    UndefinedType
};

struct Master;

template< typename T, typename Self >
struct VclTemplate : controller::PartitionTemplate< T, Self >
{
    typedef typename T::State State;
    Master *m_master;
    ComponentType m_componentType;

    void setMaster( Master *m ) { m_master = m; }

    void deallocationRequest( std::pair< State, State > );
    State transition( State from, State to );
    int owner( State st );

    VclTemplate( Config &c ) : controller::PartitionTemplate< T, Self >( c ) {
        m_componentType = UndefinedType;
    }
};

template< typename Info >
struct Vcl : controller::MetaTemplate< Info, VclTemplate >
{};

typedef divine::State< Void > State;
struct Master;

typedef controller::StateInfo< Void, char > SI;

typedef controller::CompInfo< visitor::DFS, ndfs::OuterDFS > COuterDFS;
typedef controller::CompInfo< visitor::BFS, observer::Reachability > CBFS;

typedef Vcl< controller::Info< SI, COuterDFS > >::Controller NDFS;
typedef Vcl< controller::Info< SI, CBFS > >::Controller BFS;

struct Master
{
    Pack< NDFS > ndfs;
    Pack< BFS > bfs;

    Pool m_pool;
    process_decomposition_t decomposition;
    dve_explicit_system_t system;

    int ndfsQ, bfsQ;

    std::vector< int > m_ndfsId;

    hash_t componentId( State st )
    {
        // for some reason, process_decomposition_t::get_scc_id takes
        // non-const reference, so we have to provide it... lame
        state_t s = st.state();
        return decomposition.get_scc_id( s );
    }

    ComponentType componentType( State st )
    {
        assert( st.valid() );
        state_t s = st.state();
        return static_cast< ComponentType >( decomposition.get_scc_type( s ) );
    }

    int ndfsId( State st )
    {
        assert( componentType( st ) != NonAccepting );
        return m_ndfsId[ componentId( st ) ];
    }

    void queue( std::pair< State, State > t )
    {
        if ( componentType( t.second ) == NonAccepting ) {
            ++bfsQ;
            bfs.queue( t );
        } else {
            ++ ndfsQ;
            ndfs.queue( t );
        }
    }

    void deallocationRequest( std::pair< State, State > t )
    {
        if ( componentType( t.first ) == NonAccepting ) {
            bfs.worker( bfs.owner( t.first ) ).m_releaseFifo.push( t.second );
        } else {
            ndfs.worker( ndfs.owner( t.first ) ).m_releaseFifo.push( t.second );
        }
    }

    void start()
    {
        TerminationConsole term;
        ndfs.initialize();
        bfs.initialize();

        for ( size_t i = 0; i < bfs.workerCount(); ++i ) {
            bfs.worker( i ).setMaster( this );
            bfs.worker( i ).plug( term );
            bfs.worker( i ).m_componentType = NonAccepting;
        }

        for ( size_t i = 0; i < ndfs.workerCount(); ++i ) {
            ndfs.worker( i ).setMaster( this );
            ndfs.worker( i ).plug( term );
            ndfs.worker( i ).observer().setThreadCount( 2 ); // FIXME
        }

        for ( int i = 0; i < decomposition.get_scc_count(); ++i ) {
            ComponentType t = static_cast< ComponentType >(
                decomposition.get_scc_type( i ) );
            if ( t != NonAccepting )
                ndfs.worker( m_ndfsId[ i ] ).m_componentType = t;
        }

        queue( std::make_pair( State(), State( system.get_initial_state() ) ) );

        ndfs.run();
        bfs.run();

        term.start();

        while ( !term.m_terminated )
        {
            sleep(1);
            int ns = 0, bs = 0, bq = 0;

            for ( size_t i = 0; i < bfs.workerCount(); ++i ) {
                bs += bfs.worker( i ).observer().m_stateCount;
                bq += bfs.worker( i ).visitor().m_queue.size();
            }

            for ( size_t i = 0; i < ndfs.workerCount(); ++i ) {
                ns += ndfs.worker( i ).observer().m_seenCount;
            }

            std::cerr << "ndfs seen: " << ns << ", bfs seen: " << bs
                      << ", total: " << ns + bs << std::endl;
        }

        // term.wait();

        ndfs.termination();
        bfs.termination();
    }

    Master( Config &c )
        : ndfs( c ), bfs( c )
    {
        std::ifstream in( c.input().c_str() );
        system.setAllocator( new PooledAllocator< State >( m_pool ) );
        system.read( in );
        decomposition.set_system( &system );
        decomposition.parse_process( system.get_property_gid() );
        ndfsQ = bfsQ = 0;
        int count = decomposition.get_scc_count();
        int j = 0;
        for ( int i = 0; i < count; ++i ) {
            int t = decomposition.get_scc_type( i );
            if ( t != NonAccepting ) {
                m_ndfsId.push_back( j );
                ++ j;
            } else {
                m_ndfsId.push_back( -1 );
            }
        }
        assert( c.maxThreadCount() > j );
        ndfs.setThreadCount( j );
        bfs.setThreadCount( c.maxThreadCount() - j );
    }
};

void ndfs( Config &c )
{
    Master m( c );
    m.start();
}

template< typename T, typename S >
typename T::State VclTemplate< T, S >::transition( State from, State to )
{
    if ( m_componentType == m_master->componentType( to )
         && this->self().owner( to ) == this->m_id ) {
        return this->self().localTransition( from, to );
    } else {
        to.clearDeleteFlag();
        m_master->queue( std::make_pair( from, to ) );
        return State();
    }
}

template< typename T, typename S >
int VclTemplate< T, S >::owner( State st )
{
    if ( m_master->componentType( st ) == NonAccepting )
        return (st.hash( 12 ) % 13001) % this->pack().workerCount();
    else
        return m_master->ndfsId( st );
}

template< typename T, typename S >
void VclTemplate< T, S >::deallocationRequest( std::pair< State, State > t )
{
    assert( t.second.valid() );
    m_master->deallocationRequest( t );
}

}
}

#endif
#endif
