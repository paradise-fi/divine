// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2013 Petr Rockai <me@mornfall.net>

#include <sstream>

#include <brick-string.h>

#include <divine/instances/definitions.h>
#include <divine/instances/select.h>

namespace divine {
namespace instantiate {

struct ErrorMissing : std::runtime_error {

    ErrorMissing( FixArray< Key > trace ) :
        runtime_error( format( trace ) )
    {}
private:
    static std::string format( FixArray< Key > trace ) {
        std::ostringstream s;
        s << std::get< 1 >( showGen( trace.back() ) ) << " is not available "
          << "while selecting " << std::get< 0 >( showGen( trace.back() ) ) << "." << std::endl
          << "    Selector came to dead end after selecting: "
          << brick::string::fmt( trace ) << std::endl;
        return s.str();
    }
};

struct NoDefault : std::runtime_error {
    NoDefault( FixArray< Key > trace ) :
        runtime_error( format( trace ) )
    {}
private:
    static std::string format( FixArray< Key > trace ) {
        std::ostringstream s;
        s << "Selector came to dead end after selecting: "
          << brick::string::fmt( trace ) << std::endl
          << "    No default available." << std::endl;
        return s.str();
    }
};

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

            std::cerr << "WARNING: symbols " << brick::string::fmt_container( skipped, '{', '}' )
                      << " were not selected because " << std::get< 1 >( showGen( k ) )
                      << " has higher priority." << std::endl
                      << "    When instantiating " << std::get< 0 >( showGen( k ) ) << "." << std::endl;
            return;
        }
    }
    ASSERT_UNREACHABLE( "Invalid key" );
}

}

using namespace instantiate;

template< typename Trace >
std::string stringTrace( Trace trace ) {
    std::vector< std::string > vec;
    for ( auto i : trace )
        vec.emplace_back( std::get< 1 >( showGen( i ) ) );
    return brick::string::fmt( vec );
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

    if ( component->empty() )
        throw std::runtime_error( "FATAL: component list empty at " + brick::string::fmt( sofar ) );

    bool available = false;

    for ( int retry = 0; retry < 2; ++ retry ) {
        for ( auto c : *component ) {
            auto candidate = appendArray( sofar, c );
            if ( !_valid( candidate ) )
                continue;
            if ( retry || _select( meta, c ) ) {
                if ( _traits( c ) ) {
                    available = true;
                    warnOtherAvailable( meta, c );
                    _postSelect( meta, c );
                    if ( auto x = divine::select( meta, appendArray( sofar, c ), component + 1, end ) )
                        return x;
                } else if ( !retry ) {
                    _deactivate( meta, c );
                    /* die right away if there is no hope of recovery */
                    if ( options[ c.type ].has( SelectOption::ErrUnavailable ) )
                        throw ErrorMissing( appendArray( sofar, c ) );
                    else
                        warningMissing( c );
                }
            }
        }
    }

    if ( !available ) {
        throw std::runtime_error( 
            "FATAL: no valid component for " + std::get< 0 >( showGen( *component->begin() ) ) +
            " was built, at " + brick::string::fmt( sofar ) );
    }

    return nullptr;
}

AlgorithmPtr select( Meta &meta )
{
    return divine::select( meta, Trace(), instantiation.begin(), instantiation.end() );
}

}
