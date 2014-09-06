// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2013 Petr Rockai <me@mornfall.net>

#include <divine/instances/definitions.h>
#include <divine/instances/select.h>

namespace divine {
namespace instantiate {

bool _select( const Meta &meta, Key k ) {
    return select[ k ]( meta );
}

bool _traits( Key k ) {
    auto it = traits.find( k );
    return it == traits.end() || it->second( Traits::available() );
}

void _postSelect( Meta &meta, Key k ) {
    auto it = postSelect.find( k );
    if ( it != postSelect.end() )
        it->second( meta );
}

void _init( const Meta &meta, FixArray< Key > instance ) {
    for ( auto k : instance ) {
        auto it = init.find( k );
        if ( it != init.end() )
            it->second( meta );
    }
}

void _deactivate( Meta &meta, Key key ) {
    auto it = deactivation.find( key );
    if ( it != deactivation.end() )
        it->second( meta );
}

void warningMissing( Key key ) {
    if ( !options[ key.type ].has( SelectOption::WarnUnavailable ) )
        return;
    std::cerr << "WARNING: " + std::get< 1 >( showGen( key ) ) + " is not available "
              << "while selecting " + std::get< 0 >( showGen( key ) ) + "." << std::endl
              << "    Will try to select other option." << std::endl;
}

std::nullptr_t errorMissing( FixArray< Key > trace ) {
    std::cerr << "ERROR: " << std::get< 1 >( showGen( trace.back() ) ) << " is not available "
              << "while selecting " << std::get< 0 >( showGen( trace.back() ) ) << "." << std::endl
              << "    Selector came to dead end after selecting: "
              << wibble::str::fmt( trace ) << std::endl;
    return nullptr;
}

std::nullptr_t noDefault( FixArray< Key > trace ) {
    std::cerr << "ERROR: Selector came to dead end after selecting: "
              << wibble::str::fmt( trace ) << std::endl
              << "    No default available." << std::endl;
    return nullptr;
}

void warnOtherAvailable( Meta metacopy, Key k ) {
    if ( !options[ k.type ].has( SelectOption::WarnOther ) )
        return;

    for ( auto i : instantiation ) {
        if ( i[ 0 ].type == k.type ) {
            std::vector< std::string > skipped;
            for ( auto l : i ) {
                if ( l.key > k.key && _select( metacopy, l ) )
                    skipped.push_back( std::get< 1 >( showGen( l ) ) );
            }
            if ( options[ k.type ].has( SelectOption::LastDefault ) && skipped.size() )
                skipped.pop_back();

            if ( skipped.empty() )
                return;

            std::cerr << "WARNING: symbols " << wibble::str::fmt_container( skipped, '{', '}' )
                      << " were not selected because " << std::get< 1 >( showGen( k ) )
                      << " has higher priority." << std::endl
                      << "    When instantiating " << std::get< 0 >( showGen( k ) ) << "." << std::endl;
            return;
        }
    }
    assert_unreachable( "Invalid key" );
}

}

using namespace instantiate;

template< typename Trace >
std::string stringTrace( Trace trace ) {
    std::vector< std::string > vec;
    for ( auto i : trace )
        vec.emplace_back( std::get< 1 >( showGen( i ) ) );
    return wibble::str::fmt( vec );
}

template< typename I >
AlgorithmPtr select( Meta &meta, Trace sofar, I component, I end )
{
    if ( component == end ) {
        if ( jumptable.count( sofar ) ) {
            meta.algorithm.instance = stringTrace( sofar );
            return jumptable[ sofar ]( meta );
        }
        return nullptr;
    }

    if ( component->empty() ) {
        std::cerr << "FATAL: component list empty at " << wibble::str::fmt( sofar ) << std::endl;
        throw nullptr;
    }

    bool available = false;

    for ( int retry = 0; retry < 2; ++ retry ) {
        for ( auto c : *component ) {
            if ( retry || _select( meta, c ) ) {
                if ( _traits( c ) ) {
                    available = true;
                    warnOtherAvailable( meta, c );
                    _postSelect( meta, c );
                    if ( auto x = divine::select( meta, appendArray( sofar, c ), component + 1, end ) )
                        return x;
                } else if ( !retry ) {
                    _deactivate( meta, c );
                    warningMissing( c );
                    /* die right away if there is no hope of recovery */
                    if ( options[ c.type ].has( SelectOption::ErrUnavailable ) )
                        throw errorMissing( appendArray( sofar, c ) );
                }
            }
        }
    }

    if ( !available ) {
        std::cerr << "FATAL: no component for " << std::get< 0 >( showGen( *component->begin() ) )
                  << " was built, at " << wibble::str::fmt( sofar ) << std::endl;
        throw nullptr;
    }

    return nullptr;
}

AlgorithmPtr select( Meta &meta )
{
    return divine::select( meta, Trace(), instantiation.begin(), instantiation.end() );
}

}
