#include "clocks.h"
#include <external/dbm/include/dbm/dbm.h>
#include <external/dbm/include/dbm/print.h>
#include <brick-assert.h>

// #define LU_EXTRAPOLATION_OFF

void Clocks::up() {
    ASSERT( dbm_isValid( data, dim ) );
    dbm_up( data, dim );
}

bool Clocks::isUnbounded() const {
    return dbm_isUnbounded( data, dim );
}

bool Clocks::constrainBelow( unsigned int id, int32_t value, bool strict ) {
    ASSERT( id + 1 < dim );
    ASSERT_LEQ( value, bounds[ dim + id + 1 ] );  // incorrect upper bound
    bool ret = dbm_constrain1( data, dim, id + 1, 0, dbm_boundbool2raw( value, strict ) );
    return ret;
}

bool Clocks::constrainAbove( unsigned int id, int32_t value, bool strict ) {
    ASSERT( id + 1 < dim );
    ASSERT_LEQ( value, bounds[ id + 1 ] );    // incorrect lower bound
    bool ret = dbm_constrain1( data, dim, 0, id + 1, dbm_boundbool2raw( -value, strict ) );
    return ret;
}

bool Clocks::constrainClocks( unsigned int c1, unsigned int c2, int32_t value, bool strict ) {
    ASSERT( c1 + 1 < dim && c2 + 1 < dim );
    bool ret = dbm_constrain1( data, dim, c1 + 1, c2 + 1, dbm_boundbool2raw( value, strict ) );
    return ret;
}

void Clocks::initial() {
    dbm_zero( data, dim );
}

void Clocks::set( unsigned int id, int32_t value ) {
    ASSERT( value >= 0);
    ASSERT( id + 1 < dim );
    dbm_updateValue( data, dim, id + 1, value );
}

void Clocks::resize( unsigned int clocks ) {
    dim = clocks + 1;
    bounds.clear();
    bounds.resize( 2*dim, -dbm_INFINITY );
    bounds[ dim ] = bounds[ 0 ] = 0;
}

std::ostream& Clocks::print_name( std::ostream& o, unsigned int id ) const {
    if ( id - 1 < names.size() && !names[ id - 1 ].empty() ) {
        o << names[ id - 1 ];
    } else {
        o << "x" << id;
    }
    return o;
}

void Clocks::print( std::ostream& o ) const {
    for ( unsigned int i = 0; i < dim; i++ )
        if ( data[ i * dim + i ] != dbm_LE_ZERO ) {
            o << "empty\n";
            return;
        }
    unsigned int printed = 0;
    for ( unsigned int i = 1; i < dim; i++ )
        rowFmt( o, printed, print_c( o, i ) );
    for ( unsigned int i = 1; i < dim; i++ )
        for ( unsigned int j = i + 1; j < dim; j++ )
            rowFmt( o, printed, print_d( o, i, j ) );
    o << "\n";
}

void Clocks::print_m( std::ostream& o ) const {
    dbm_cppPrint( o, data, dim );
}

const char* Clocks::boundEq( int32_t bound ) {
    return dbm_rawIsStrict( bound ) ? " < " : " \u2264 ";
}

void Clocks::rowFmt( std::ostream& o, unsigned int& count, bool printed ) {
    if ( !printed )
        return;
    if ( ++count % 2u == 0 )
        o << "\n";
    else
        o << "\t";
}

bool Clocks::print_c( std::ostream& o, unsigned int i ) const {
    ASSERT( i > 0 );
    raw_t upper = data[ i * dim + 0 ];
    raw_t lower = data[ 0 * dim + i ];
    if ( lower != dbm_LE_ZERO )
        o << ( -dbm_raw2bound( lower ) ) << boundEq( lower );
    if ( lower != dbm_LE_ZERO || upper != dbm_LS_INFINITY )
        print_name( o, i );
    else
        return false;
    if ( upper != dbm_LS_INFINITY )
        o << boundEq( upper ) << dbm_raw2bound( upper );
    return true;
}

bool Clocks::print_d( std::ostream& o, unsigned int i, unsigned int j ) const {
    ASSERT( i > 0 && j > 0 );
    raw_t upper = data[ i * dim + j ];
    raw_t lower = data[ j * dim + i ];
    if ( lower != dbm_LS_INFINITY )
        o << ( -dbm_raw2bound( lower ) ) << boundEq( lower );
    if ( lower != dbm_LS_INFINITY || upper != dbm_LS_INFINITY ) {
        print_name( o, i );
        o << "-";
        print_name( o, j );
    } else
        return false;
    if ( upper != dbm_LS_INFINITY )
        o << boundEq( upper ) << dbm_raw2bound( upper );
    return true;
}

void Clocks::setName( unsigned int id, const std::string& name ) {
    names.resize( std::max( id + 1, unsigned( names.size() ) ) );
    names[ id ] = name;
}

void Clocks::setLowerBound( unsigned int id, int32_t limit ) {
    ASSERT( id + 1 < dim );
    bounds[ id + 1 ] = limit;
}

bool Clocks::updateLowerBound( unsigned int id, int32_t limit ) {
    ASSERT( id + 1 < dim );
    if ( limit > bounds[ id + 1 ] ) {
        bounds[ id + 1 ] = limit;
        return true;
    }
    return false;
}

void Clocks::setUpperBound( unsigned int id, int32_t limit ) {
    ASSERT( id + 1 < dim );
    bounds[ dim + id + 1 ] = limit;
}

bool Clocks::updateUpperBound( unsigned int id, int32_t limit ) {
    ASSERT( id + 1 < dim );
    if ( limit > bounds[ dim + id + 1 ] ) {
        bounds[ dim + id + 1 ] = limit;
        return true;
    }
    return false;
}

void Clocks::extrapolateMaxBounds() {
   dbm_extrapolateMaxBounds( data, dim, &bounds[ 0 ] );
}

void Clocks::extrapolateDiagonalMaxBounds() {
   dbm_diagonalExtrapolateMaxBounds( data, dim, &bounds[ 0 ] );
}

void Clocks::extrapolate() {
   dbm_diagonalExtrapolateLUBounds( data, dim, &bounds[ 0 ], &bounds[ dim ] );
}
