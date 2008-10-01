// -*- C++ -*-

#include <divine/bundle.h>
#include <divine/controller.h>
#include <divine/state.h>

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

};

}

#endif
