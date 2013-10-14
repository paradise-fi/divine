// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/instances/select-impl.h>

#include <divine/instances/create.h>
#include <divine/instances/auto/extern.h>

namespace divine {
namespace instantiate {

struct MetaSelector {
    Meta &meta;

    using ReturnType = AlgoR;

    MetaSelector( Meta &meta ) : meta( meta ) { }

    template< typename T >
    auto postSelect( Meta &meta, T, wibble::Preferred )
        -> decltype( T::postSelect( meta ) )
    {
        T::postSelect( meta );
    }

    template< typename T >
    auto postSelect( Meta &, T, wibble::NotPreferred ) -> void { }

    template< typename Selected >
    auto runInits( Meta &meta, Selected )
        -> typename std::enable_if< ( Selected::length > 0 ) >::type
    {
        runInit( meta, typename Selected::Head(), wibble::Preferred() );
        runInits( meta, typename Selected::Tail() );
    }

    template< typename Selected >
    auto runInits( Meta &, Selected )
        -> typename std::enable_if< Selected::length == 0 >::type
    { }

    template< typename T >
    auto runInit( Meta &meta, T, wibble::Preferred )
        -> decltype( T::init( meta ) )
    {
        T::init( meta );
    }

    template< typename T >
    auto runInit( Meta &, T, wibble::NotPreferred ) -> void { }

    template< typename T, typename TrueFn, typename FalseFn >
    ReturnType ifSelect( T, TrueFn trueFn, FalseFn falseFn ) {
        if ( T::select( meta ) ) {
            if ( T::trait( Traits::available() ) ) {
                postSelect( meta, T(), wibble::Preferred() );
                return trueFn();
            } else {
                std::cerr << "WARNING: " << Atom( T() ).friendlyName
                          << " should be selected, but it is not available" << std::endl
                          << "(when selecting " << Atom( T() ).component << ")." << std::endl
                          << "Will try other instances..." << std::endl;
                return falseFn();
            }
        }
        else
            return falseFn();
    }

    template< typename Error >
    ReturnType instantiationError( Error ) {
        std::cerr << "FATAL: " << Error::instantiationError( meta ) << std::endl
                  << "FATAL: Internal instantiation error." << std::endl;
        return nullptr;
    }

    ReturnType instantiationError( std::string component ) {
        std::cerr << "FATAL: Failed to select " << component << " please check your input." << std::endl
                  << "FATAL: Internal instantiation error." << std::endl;
        return nullptr;
    }

    template< typename Array >
    void warnOtherAvailable( Atom selected, Array others ) {
        std::string component = selected.component;
        std::vector< std::string > otherNames;
        std::for_each( others.begin(), others.end(), [ &otherNames ]( const Atom &at ) {
                otherNames.push_back( at.friendlyName );
            } );
        std::cerr << "WARNING: Following components of type " << component
                  << " were not selected" << std::endl
                  << "because " << selected.friendlyName
                  << " has higher priority:" << std::endl
                  << wibble::str::fmt_container( otherNames, '[', ']' ) << std::endl;
    }

    template< typename Selected >
    ReturnType create() {
        runInits( meta, Selected() );
        return createInstance< Selected >( meta );
    }
};

}

instantiate::AlgoR select( Meta &meta ) {
    instantiate::MetaSelector ms( meta );
    return runSelector( ms, instantiate::Instantiate(), TypeList<>() );
}

}

