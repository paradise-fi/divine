#ifndef DIVINE_TIMED_CLOCKS_H
#define DIVINE_TIMED_CLOCKS_H

#include <cstdint>
#include <iostream>
#include <cassert>
#include <vector>

#undef ASSERT
#define TRUE TRUE_DBM
#define FALSE FALSE_DBM
#include <dbm/fed.h>
#undef TRUE
#undef FALSE
#undef ASSERT

#include <brick-assert.h>

typedef dbm::fed_t Federation;

class Clocks {
    unsigned int dim;
    raw_t *data;
    std::vector< int32_t > bounds; // maximal lower and upper bounds
    std::vector< std::string > names;

    // prind bounds from DBM
    void print( std::ostream& o ) const;

    // print DBM matrix
    void print_m( std::ostream& o ) const;

    static const char* boundEq( int32_t bound );

    static void rowFmt( std::ostream& o, unsigned int& count, bool printed );

    bool print_c( std::ostream& o, unsigned int i ) const;

    bool print_d( std::ostream& o, unsigned int i, unsigned int j ) const;

    std::ostream& print_name( std::ostream& o, unsigned int id ) const;

public:
    // set clock count
    void resize( unsigned int clocks );

	// performs clock extrapolation based on currently set limits
	// if the argument is true, diagonal extrapolation is performed,
	// which can break constraints including clock differences
    void extrapolate();
    void extrapolateMaxBounds();
    void extrapolateDiagonalMaxBounds();

    // Returns number of bytes required to store clocks (always multiple of 4)
    unsigned int getReqSize() const {
        return dim * dim * sizeof( raw_t );
    }

    // Set data to work on, has to have at least getReqSize() bytes
    void setData( char *ptr ) {
        data = reinterpret_cast< raw_t* >( ptr );
    }

    // Set valuation to the initial region (reset all clocks)
    void initial();

    // Assign integer value to given clock
    void set( unsigned int id, int32_t value );

    void reset( unsigned int id ) {
        set( id, 0 );
    }

    void up();

    // Adds constrain x_id <= value ("<" for strict = true)
    // returns true if the result is non-empty
    bool constrainBelow( unsigned int id, int32_t value, bool strict );

    // Adds constrain x_id >= value (">" for strict = true)
    // returns true if the result is non-empty
    bool constrainAbove( unsigned int id, int32_t value, bool strict );

    // Adds constrain x_c1 - x_c2 <= value ("<" for strict = true)
    // returns true if the result is non-empty
    bool constrainClocks( unsigned int c1, unsigned int c2, int32_t value, bool strict );

    bool isUnbounded() const;

    // Set the highest value a clock can be compared to from below (clock > value)
    void setLowerBound( unsigned int id, int32_t limit );

    // Set the highest value a clock can be compared to from below,
    // if the current bound is higher than given one, it is not changed and false is returned
    bool updateLowerBound( unsigned int id, int32_t limit );

    // Set the highest value a clock can be compared to from above (clock < value)
    void setUpperBound( unsigned int id, int32_t limit );

    // Set the highest value a clock can be compared to from above,
    // if the current bound is higher than given one, it is not changed and false is returned
    bool updateUpperBound( unsigned int id, int32_t limit );

    void setName( unsigned int id, const std::string& name );

    friend std::ostream& operator<<( std::ostream& o, const Clocks& c ) {
        c.print( o );
        return o;
    }

    // Returns true if the described zone is empty
    bool isEmpty() {
        return *data != 1;
    }

    // Return federation equal to current zone
    Federation createFed() const {
        return Federation( data, dim );
    }

    // returns the intersection of the current zone and given federation
    Federation intersection( Federation fed ) const {
        ASSERT_EQ( fed.getDimension(), dim );
        return fed & data;
    }

    dbm::dbm_t intersection( const dbm::dbm_t &d ) const {
        ASSERT_EQ( d.getDimension(), dim );
        return d & data;
    }

    // set zone from given pointer, dimensions have to be equal
    void assignZone( const raw_t* src ) {
        memcpy( data, src, getReqSize() );
    }

    void assignZone( const dbm::dbm_t &d ) {
        ASSERT_EQ( d.getDimension(), dim );
        ASSERT( d() );
        memcpy( data, d(), getReqSize() );
    }
};

#endif
