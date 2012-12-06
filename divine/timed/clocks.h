#ifndef DIVINE_TIMED_CLOCKS_H
#define DIVINE_TIMED_CLOCKS_H
#include <stdint.h>
#include <iostream>
#include <cassert>
#include <vector>

class Clocks {
    unsigned int dim;
    int32_t *data;
    std::vector< int32_t > bounds; // maximal lower and upper bounds

    // prind bounds from DBM
    void print( std::ostream& o ) const;

    // print DBM matrix
    void print_m( std::ostream& o ) const;

    static const char* boundEq( int32_t bound );

    static void rowFmt( std::ostream& o, unsigned int& count, bool printed );

    bool print_c( std::ostream& o, unsigned int i ) const;

    bool print_d( std::ostream& o, unsigned int i, unsigned int j ) const;

public:
    // set clock count
    void resize( unsigned int clocks );

    void extrapolate();

    // Returns number of bytes required to store clocks (always multiple of 4)
    unsigned int getReqSize() const {
        return dim * dim * sizeof( int32_t );
    }

    // Set data to work on, has to have at least getReqSize() bytes
    void setData( char *ptr ) {
        data = reinterpret_cast< int32_t* >( ptr );
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

    friend std::ostream& operator<<( std::ostream& o, const Clocks& c ) {
        c.print( o );
        return o;
    }

    bool isEmpty() {
        return *data != 1;
    }
};

#endif
