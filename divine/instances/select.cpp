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
            postSelect( meta, T(), wibble::Preferred() );
            return trueFn();
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

