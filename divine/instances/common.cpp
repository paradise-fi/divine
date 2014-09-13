// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2013 Petr Rockai <me@mornfall.net>

#include <divine/instances/definitions.h>
#include <divine/instances/select.h>

namespace divine {
namespace instantiate {

bool _evalSuppBy( const SupportedBy &suppBy, const std::vector< Key > &vec ) {
    if ( suppBy.is< Not >() )
        return !_evalSuppBy( *suppBy.get< Not >().val, vec );
    if ( suppBy.is< And >() ) {
        for ( auto s : suppBy.get< And >().val )
            if ( !_evalSuppBy( s, vec ) )
                return false;
        return true;
    }
    if ( suppBy.is< Or >() ) {
        for ( auto s : suppBy.get< Or >().val )
            if ( _evalSuppBy( s, vec ) )
                    return true;
        return false;
    }
    assert( suppBy.is< Key >() );
    for ( auto v : vec )
        if ( v == suppBy.get< Key >() )
            return true;
    return false;
}

bool _valid( FixArray< Key > trace ) {
    std::vector< Key > vec;
    for ( int i = 0; i < int( trace.size() ); ++i ) {
        auto supp = supportedBy.find( trace[ i ] );
        if ( supp != supportedBy.end() ) {
            if ( !_evalSuppBy( supp->second, vec ) )
                return false;
        }
        vec.push_back( trace[ i ] );
    }
    return true;
}

}
}
