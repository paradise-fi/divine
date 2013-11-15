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

    bool trySelect( const Selectable &s ) {
        return s.trait( Traits::available() ) && s.select( meta );
    }

    template< typename TrueFn, typename NotSelAvailFn, typename NotAvailFn >
    ReturnType ifSelect( const Selectable &s, bool warn, TrueFn trueFn,
            NotSelAvailFn notSelAvailFn, NotAvailFn notAvailFn )
    {
        if ( s.trait( Traits::available() ) ) {
            if ( s.select( meta ) ) {
                s.postSelect( meta );
                s.init( meta );
                return trueFn();
            } else
                return notSelAvailFn();
        } else {
            Meta metacopy( meta );
            if ( warn && s.select( metacopy ) )
                std::cerr << "WARNING: " << s.atom().friendlyName
                          << " should be selected, but it is not available" << std::endl
                          << "    (when selecting " << s.atom().component << ")." << std::endl
                          << "    Will try other instances..." << std::endl;
            return notAvailFn();
        }
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
                  << "    because " << selected.friendlyName
                  << " has higher priority:" << std::endl
                  << "    " << wibble::str::fmt_container( otherNames, '[', ']' ) << std::endl;
    }

    template< typename Selected >
    ReturnType create() {
        return createInstance< Selected >( meta );
    }
};

}

instantiate::AlgoR select( Meta &meta ) {
    instantiate::MetaSelector ms( meta );
    return runSelector( ms, instantiate::Instantiate(), TypeList<>() );
}

}

