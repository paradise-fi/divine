// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/observer.h>
#include <divine/threading.h>

#ifndef DIVINE_MAP_H
#define DIVINE_MAP_H

namespace divine {
namespace algorithm {

template< typename _Setup >
struct Map : Algorithm
{
    template< typename Bundle, typename Self >
    struct ObserverImpl :
        observer::Common< Bundle, ObserverImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Controller &m_controller;
        size_t m_expanded, m_elim, m_accepting;

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        void expanding( State st )
        {
            ++ m_expanded;
            if ( system().is_accepting( st ) ) {
                if ( !st.extension().id ) { // first encounter
                    st.extension().id = reinterpret_cast< intptr_t >( st.ptr );
                    ++ m_accepting;
                }
                if ( st.extension().id == st.extension().map )
                    std::cout << "found accepting cycle" << std::endl;
            } else {
                st.extension().id = 0;
            }
        }

        std::pair< int, bool > map( State f, State st )
        {
            int map;
            bool elim = false;
            if ( f.extension().oldmap != 0 && st.extension().oldmap != 0 )
                if ( f.extension().oldmap != st.extension().oldmap )
                    return std::make_pair( -1, false );

            if ( f.extension().map < f.extension().id ) {
                if ( f.extension().elim == 2 )
                    map = f.extension().map;
                else {
                    map = f.extension().id;
                    if ( f.extension().elim != 1 )
                        elim = true;
                }
            } else
                map = f.extension().map;

            return std::make_pair( map, elim );
        }

        State transition( State f, State t )
        {
            if ( !f.valid() ) {
                assert( t == system().initial() );
                return follow( t );
            }

            if ( f.extension().id == 0 )
                assert( !system().is_accepting( f ) );
            if ( system().is_accepting( f ) )
                assert( f.extension().id > 0 );

            std::pair< int, bool > a = map( f, t );
            int map = a.first;
            bool elim = a.second;

            if ( map == -1 )
                return follow( t );

            if ( t.extension().map < map ) {
                if ( elim ) {
                    ++ m_elim;
                    f.extension().elim = 1;
                }
                t.extension().map = map;
                // re-expand this state
                t.setFlags( 0 );
                m_controller.visitor().push( t );
            }

            return follow( t );
        }

        ObserverImpl( typename Bundle::Controller &controller )
            : m_controller( controller ), m_expanded( 0 ), m_elim( 0 ),
            m_accepting( 0 )
            {
            }

        void reset() {
            std::cerr << "resetting, expanded states = " << m_expanded
                      << ", eliminated " << m_elim << std::endl;
            for ( size_t i = 0; i < visitor().storage().table().size(); ++i ) {
                State st = visitor().storage().table()[ i ].key;
                if ( st.valid() )
                    st.extension().oldmap = st.extension().map;
            }
            m_expanded = 0;
            m_elim = 0;
        }
    };

    struct Extension {
        unsigned id:30;
        unsigned elim:2;
        unsigned map:30;
        unsigned oldmap:30;
    };

    template< typename Storage >
    void zeroFlags( Storage &stor )
    {
        for ( size_t j = 0; j < stor.table().size(); ++j ) {
            typename Storage::State st = stor.table()[ j ].key;
            if ( st.valid() ) {
                st.setFlags( 0 );
                st.extension().map = 0;
                if ( st.extension().elim == 1 )
                    st.extension().elim = 2;
            }
        }
    }

    Map( Config &c ) : Algorithm( c ) {}

    typedef Finalize< ObserverImpl > Observer;
    typedef typename BundleFromSetup<
        _Setup, Observer, Extension >::T Bundle;

    Result run()
    {
        threads::Pack< typename Bundle::Controller > map( config() );

        map.initialize();
        int threads = map.workerCount();
        int acceptingCount = 0, eliminated = 0, d_eliminated = 0;
        do {
            map.blockingVisit();
            acceptingCount = 0;
            d_eliminated = 0;
            for ( int i = 0; i < threads; ++ i ) {
                acceptingCount += map.worker( i ).observer().m_accepting;
                d_eliminated += map.worker( i ).observer().m_elim;
                // map.worker( i ).storage().pool().printStatistics( std::cerr );
                map.worker( i ).observer().reset();
                zeroFlags( map.worker( i ).visitor().storage() );
            }
            eliminated += d_eliminated;
            assert( eliminated <= acceptingCount );
            std::cerr << "so far eliminated " << eliminated
                      << " out of " << acceptingCount
                      << " total accepting states" << std::endl;
        } while ( d_eliminated > 0 );
        std::cout << "no accepting cycles found" << std::endl;
        return Result();
    }

    /* template< template< typename > class Controller,
              template< typename > class Visitor >
    void shared( Config &c )
    {
        typedef controller::StateInfo< Extension, Void > SI;
        typedef controller::CompInfo< Visitor, MapObserver, storage::ConstShared >
            CMap;
        typedef typename Controller< controller::Info< SI, CMap > >::Controller Map;

        Pack< Map > map( c );
        map.initialize();
        int threads = map.workerCount();
        int acceptingCount = 0, eliminated = 0, d_eliminated = 0;
        do {
            map.blockingVisit();
            acceptingCount = 0;
            d_eliminated = 0;
            for ( int i = 0; i < threads; ++ i ) {
                acceptingCount += map.worker( i ).observer().m_accepting;
                d_eliminated += map.worker( i ).observer().m_elim;
                map.worker( i ).observer().reset();
            }
            zeroFlags( map.worker( 0 ).visitor().storage() );
            eliminated += d_eliminated;
            assert( eliminated <= acceptingCount );
            std::cerr << "so far eliminated " << eliminated
                      << " out of " << acceptingCount
                      << " total accepting states" << std::endl;
        } while ( d_eliminated > 0 );
        std::cout << "no accepting cycles found" << std::endl;
        } */
};

}
}

#endif
