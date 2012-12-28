#include <stdint.h>
#include <cassert>
#include <utap/utap.h>

#ifndef DIVINE_TIMED_UTILS_H
#define DIVINE_TIMED_UTILS_H

struct Cut {
    UTAP::expression_t pos;
    UTAP::expression_t neg;
    int pId;

    Cut( UTAP::expression_t p, UTAP::expression_t n, int pd ) : pos( p ), neg( n ), pId( pd ) {}
};

// negates a comparison
UTAP::expression_t negIneq( const UTAP::expression_t& expr );

// adds an expression and its negation to the list that can later be used for slicing, avoids duplicities
// returns true if the list was changed
bool addCut( const UTAP::expression_t& expr, int pId, std::vector< Cut >& cuts );

// builds vector of offsets representing selection of one element from each vector from _v_
// vector of zeroes is assumed to be the firs selection and when it is reached again, false is returned
template < typename T >
bool nextSelection( std::vector< unsigned int >& sel, const std::vector< std::vector< T > >& v ) {
    assert( sel.size() <= v.size() );
    // "increment" _sel_
    unsigned int pos;
    for ( pos = 0; pos < sel.size(); ++pos ) {
        assert( v[ pos ].size() );
        if ( ++sel[ pos ] == v[ pos ].size() )
            sel[ pos ] = 0;
        else break;
    }
    return pos < sel.size();
}

class Locations {
    typedef uint16_t loc_id;

    loc_id *locs;

public:
    Locations() : locs( NULL ) {}

    void setSource( char* ptr ) {
        locs = reinterpret_cast< loc_id* >( ptr );
    }

    int get( unsigned int i ) const {
        return static_cast< int >( locs[ i ] );
    }

    void set( unsigned int i, int loc ) {
        assert( loc == static_cast< loc_id >( loc ) );
        locs[ i ] = static_cast< loc_id >( loc );
    }

    int operator[]( unsigned int i ) const {
        return get( i );
    }

    static unsigned int getReqSize( unsigned int count ) {
        return (sizeof( loc_id ) * count + 3) & ~3;
    }
};

#endif
