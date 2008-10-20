// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/observer.h>
#include <divine/threading.h>
#include <cmath>

#ifndef DIVINE_NDFS_H
#define DIVINE_NDFS_H

namespace divine {
namespace algorithm {

template< typename _Setup >
struct NestedDFS : Algorithm
{
    bool m_terminate;
    Result m_result;

    template< typename Bundle, typename Self >
    struct InnerImpl : observer::Common< Bundle, InnerImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Controller &m_controller;

        State m_first;
        int m_expanded, m_trans;
        NestedDFS *m_owner;

        void setOwner( NestedDFS &own ) {
            m_owner = &own;
            visitor().storage().setTable(
               own.m_outer.visitor().storage().table() );
        }

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        void started() {
            m_first.ptr = 0;
        }

        void expanding( State st ) {
            ++ m_expanded;
            if ( !m_first.valid() ) {
                m_first = st;
                assert( system().is_accepting( st.state() ) );
            }
        }

        State transition( State from, State to )
        {
            ++ m_trans;
            if ( !m_first.valid() )
                return follow( to );
            if ( m_first == to ) {
                std::cout << "ACCEPTING cycle found" << std::endl;
                if ( m_owner ) {
                    m_owner->m_result.ltlPropertyHolds = Result::No;
                    m_owner->m_result.fullyExplored = Result::No;
                    m_owner->m_terminate = true;
                }
            }
            return follow( to );
        }

        InnerImpl( typename Bundle::Controller &c )
            : m_controller( c )
        {
            visitor().setSeenFlag( SEEN2 );
            visitor().setPushedFlag( PUSHED2 );
            m_expanded = m_trans = 0;
            m_owner = 0;
        }
    };

    typedef Finalize< InnerImpl > Inner;
    typedef typename BundleFromSetup<
        OverrideStorage<
            OverrideController< _Setup, controller::Slave >,
            storage::Shadow >,
        Inner, Unit >::T InnerBundle;

    template< typename Bundle, typename Self >
    struct OuterImpl : observer::Common< Bundle, OuterImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Controller &m_controller;
        typename InnerBundle::Controller m_inner;

        typedef std::deque< State > Queue;

        int m_nestedCount, m_acceptingCount;
        int m_seenCount, m_lastSeenCount;

        NestedDFS *m_owner;

        void setOwner( NestedDFS &own ) {
            m_owner = &own;
        }

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        // FIXME. Since we use the shadow storage, dual-threaded NDFS is
        // broken. Bummer.
        bool threaded() { return false; }
        
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
            if ( m_owner && m_owner->m_terminate ) {
                visitor().clear();
                return;
            }
            ++ m_nestedCount;
            if ( threaded() ) {
                m_inner.queue( state );
                if ( m_nestedCount % 50 == 0 )
                    m_inner.cond.signal(); // try to wake them up
            } else
                m_inner.visitor().visit( state );
        }

        void finished()
        {
            if ( threaded() ) {
                std::cerr << "NDFS: waiting for nested thread..." << std::flush;
                m_inner.terminate();
                m_inner.join();
                std::cerr << "done" << std::endl;
            }
            /* std::cerr << "Nested count: " << m_nestedCount << std::endl;
            std::cerr << "Acceepting count: " << m_acceptingCount << std::endl;
            std::cerr << "Seen: " << m_seenCount << ", nested-seen: "
                      << m_inner.observer().m_expanded << std::endl;
            std::cerr << "Nested-trans: " << m_inner.observer().m_trans << std::endl; */

            assert( m_nestedCount <= m_acceptingCount && (m_acceptingCount ? m_nestedCount > 0 : true) );
        }
        
        OuterImpl( typename Bundle::Controller &c )
            : m_controller( c ), m_inner( c.config() ),
              m_nestedCount( 0 ), m_acceptingCount( 0 ),
              m_seenCount( 0 ), m_lastSeenCount( 0 )
        {
            m_owner = 0;
        }
    };
    
    typedef Finalize< OuterImpl > Outer;
    typedef typename BundleFromSetup<
        _Setup, Outer, Unit >::T OuterBundle;

    typename OuterBundle::Controller m_outer;

    Result run()
    {
        /* m_outer.observer().setThreadCount(
           this->config().canSpawnThread() ? 2 : 1 ); */
        m_terminate = false;
        if ( m_outer.observer().threaded() )
            m_outer.observer().m_inner.start();
        m_outer.visitor().visitFromInitial();
        m_outer.observer().finished(); // FIXME
        m_result.visited = m_outer.observer().m_seenCount;
        m_result.expanded = m_result.visited + m_outer.observer().m_inner.observer().m_expanded;

        if ( m_result.ltlPropertyHolds != Result::No )
            m_result.ltlPropertyHolds = Result::Yes;

        if ( m_result.fullyExplored != Result::No )
            m_result.fullyExplored = Result::Yes;

        return m_result;
    }

    NestedDFS( Config &c )
        : Algorithm( c ), m_outer( c )
    {
        m_outer.observer().setOwner( *this );
        m_outer.observer().m_inner.observer().setOwner( *this );
    }

};

}
}

#endif
