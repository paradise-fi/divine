// -*- C++ -*-

#include <divine/bundle.h>
#include <divine/controller.h>
#include <divine/state.h>
#include <divine/legacy/system/path.hh>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {

struct Algorithm {

    Config &m_config;
    Config &config() { return m_config; }
    Algorithm( Config &c ) : m_config( c ) {}

    template< typename C, typename S, typename O, template< typename > class G >
    struct Setup {
        typedef C Controller;
        typedef S Storage;
        typedef O Order;
        template< typename T >
        struct Generator : G< T > {};
    };

    template< typename _Setup, typename _Controller >
    struct OverrideController : _Setup {
        typedef _Controller Controller;
    };

    template< typename Setup, typename Observer, typename StateExt >
    struct BundleFromSetup {
        typedef bundle::Setup<
            typename Setup::Controller,
            typename Setup::Storage,
            typename Setup::Order,
            Observer,
            State< StateExt >,
            Setup::template Generator > BundleSetup;
        typedef Bundle< BundleSetup > T;
    };

    void resultBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    template< typename State, typename Pack >
    void printCounterexample( Pack &p, State st ) {
        State s = st;
        path_t ce;
        explicit_system_t * sys =
            p.anyWorker().observer().system().legacy_system();
        if (!sys) {
            std::cerr << "ERROR: Unable to produce counterexample" << std::endl;
            exit( 1 ); // FIXME
        }

        ce.set_system( sys );
        ce.setAllocator( p.anyWorker().storage().newAllocator() );
        do {
            s = s.extension().cycle;
            ce.push_front( s.state() );
            assert( s.valid() );
        } while ( s != st );
        ce.mark_cycle_start_front();
        while ( s != State( p.anyWorker().visitor().sys.initial() ) ) {
            s = s.extension().parent;
            ce.push_front( s.state() );
        }
        ce.write_trans(this->config().trailStream());
    }

};

}

#endif
