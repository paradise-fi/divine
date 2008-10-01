// -*- C++ -*-

#include <divine/state.h> // for Unit......

#ifndef DIVINE_BUNDLE_H
#define DIVINE_BUNDLE_H

namespace divine {

template< template< typename, typename > class S >
struct Finalize {
    template< typename T > struct Get : S< T, Get< T > > {
        Get( typename T::Controller &c ) : S< T, Get >( c ) {}
    };
};

template< template< typename, typename > class S >
struct FinalizeController {
    template< typename T > struct Get : S< T, Get< T > > {
        Get( Config &c ) : S< T, Get >( c ) {}
    };
};

namespace bundle {

template< typename _Controller,
          typename _Storage,
          typename _Visitor,
          typename _Observer,
          typename _State,
          template< typename > class _Generator >
struct Setup {
    template< typename T >
    struct Get {
        typedef typename _Visitor::template Get< T > Visitor;
        typedef typename _Observer::template Get< T > Observer;
        typedef typename _Storage::template Get< T > Storage;
        typedef typename _Controller::template Get< T > Controller;
        typedef _State State;
        typedef _Generator< State > Generator;
    };
};

}

template< typename Setup >
struct Bundle
{
    typedef typename Setup::template Get< Bundle > T;
    typedef typename T::Visitor Visitor;
    typedef typename T::Observer Observer;
    typedef typename T::Storage Storage;
    typedef typename T::State State;
    typedef typename T::Controller Controller;
    typedef typename T::Generator Generator;
};

struct VoidBundle
{
    typedef Unit Visitor;
    typedef Unit Observer;
    typedef Unit Storage;
    typedef Unit Controller;
    typedef Unit State;
    typedef Unit Generator;

};

}

#endif
