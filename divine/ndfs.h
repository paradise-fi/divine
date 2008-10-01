// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/observer.h>
#include <divine/threading.h>
#include <cmath>

#ifndef DIVINE_NDFS_H
#define DIVINE_NDFS_H
#if 0

namespace divine {
namespace ndfs {

typedef divine::State< Void > State;
struct Master;

template< typename Bundle >
struct Inner : observer::Common< Bundle, Inner< Bundle > >
{
    typedef typename Bundle::State State;
    typename Bundle::Controller &m_controller;

    State m_first;

    explicit_system_t &system() { return m_controller.visitor().sys; }
    typename Bundle::Visitor &visitor() { return m_controller.visitor(); }

    void started() {
        m_first.ptr = 0;
    }

    void expanding( State st ) {
        if ( !m_first.valid() ) {
            m_first = st;
            assert( system().is_accepting( st.state() ) );
        }
    }

    State transition( State from, State to )
    {
        if ( !m_first.valid() )
            return follow( to );
        if ( m_first == to ) {
            std::cout << "ACCEPTING cycle found" << std::endl;
        }
        return follow( to );
    }

    Inner( typename Bundle::Controller &c )
        : m_controller( c )
    {
        visitor().setSeenFlag( SEEN << 1 );
    }
};

template< typename T, typename Self >
struct SlaveTemplate : controller::Template< T, Self >, wibble::sys::Thread
{
    wibble::sys::Condition cond;
    bool m_terminate;

    Fifo< State > m_workFifo;
    void queue( State s ) {
        m_workFifo.push( s );
    }

    void *main() {
        while ( !m_terminate || !m_workFifo.empty() ) {
            if ( m_workFifo.empty() ) {
                Mutex foo;
                MutexLock foobar( foo );
                cond.wait( foobar );
            }
            while ( !m_workFifo.empty() ) {
                this->visitor().visit( m_workFifo.front() );
                m_workFifo.pop();
            }
        }
        m_terminate = false;
        return 0;
    }

    void terminate() {
        m_terminate = true;
        cond.signal();
    }

    SlaveTemplate( Config &c ) : controller::Template< T, Self >( c ) {
        m_terminate = false;
    }
};

template< typename Info >
struct Slave : controller::MetaTemplate< Info, SlaveTemplate >
{};

typedef controller::StateInfo< Void, char > SI;
typedef controller::CompInfo< visitor::DFS, Inner > CInner;

typedef Slave< controller::Info< SI, CInner > >::Controller In;

template< typename Bundle >
struct OuterDFS : observer::Common< Bundle, OuterDFS< Bundle > >
{
    typedef typename Bundle::State State;
    typename Bundle::Controller &m_controller;

    typedef std::deque< State > Queue;

    In m_inner;

    int threads;

    int m_nestedCount, m_acceptingCount;
    int m_seenCount, m_lastSeenCount;

    explicit_system_t &system() { return m_controller.visitor().sys; }
    typename Bundle::Visitor &visitor() { return m_controller.visitor(); }

    void expanding( State st )
    {
        assert( !visitor().seen( st ) );
        ++ m_seenCount;
        if ( system().is_accepting( st.state() ) )
            ++ m_acceptingCount;
    }

    void finished( State st )
    {
        if ( system().is_accepting( st.state() ) )
            nested( st );
    }

    void nested( State state )
    {
        ++ m_nestedCount;
        if ( threads > 1 ) {
            m_inner.queue( state );
            if ( m_nestedCount % 50 == 0 )
                m_inner.cond.signal(); // try to wake them up
        } else
            m_inner.visitor().visit( state );
    }

    void finished()
    {
        if ( threads > 1 ) {
            std::cerr << "NDFS: waiting for nested thread..." << std::flush;
            m_inner.terminate();
            m_inner.join();
            std::cerr << "done" << std::endl;
        }
        assert( m_nestedCount == m_acceptingCount );
    }

    void setThreadCount( int c )
    {
        threads = c;
        if ( c >= 2 )
            m_inner.start();
    }

    OuterDFS( typename Bundle::Controller &c )
        : m_controller( c ), m_inner( c.config() ),
          m_nestedCount( 0 ), m_acceptingCount( 0 ),
          m_seenCount( 0 ), m_lastSeenCount( 0 )
    {
        threads = 1;
    }
};

typedef controller::CompInfo< visitor::DFS, OuterDFS > COuter;
typedef controller::Simple< controller::Info< SI, COuter > >::Controller Outer;

void ndfs( Config &c )
{
    Outer m( c );
    m.observer().setThreadCount( c.canSpawnThread() ? 2 : 1 );
    m.visitor().visitFromInitial();
    m.observer().finished(); // FIXME
}

}
}

#endif
#endif
