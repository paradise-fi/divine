#ifndef BIT_OPS_H
#define BIT_OPS_H

#include <type_traits>

namespace divine {
namespace bitops {

namespace compiletime{

template< typename T >
constexpr unsigned MSB( T x ) {
    return x > 1 ? 1 + MSB( x >> 1 ) : 0;
}

template< typename T >
constexpr T fill( T x ) {
    return x ? x | fill( x >> 1 ) : x;
}

template< typename T >
constexpr size_t sizeOf() {
    return std::is_empty< T >::value ? 0 : sizeof( T );
}

}
/*
 *  Fills `x` by bits up to the most si significant bit.
 *  Comlexity is O(log n), n is sizeof(x)*8
 */
template< typename number >
static inline number fill( register number x ) {
    static const unsigned m = sizeof( number ) * 8;
    register unsigned r = 1;
    if ( !x )
        return 0;
    while ( m != r ) {
        x |= x >> r;
        r <<= 1;
    }
    return x;
}

// get index of Most Significant Bit
// templated by argument to int, long, long long (all unsigned)
template< typename T >
static inline unsigned MSB( T x ) {
    unsigned position = 0;
    while ( x ) {
        x >>= 1;
        ++position;
    }
    return position - 1;
}

template<>
inline unsigned MSB< unsigned int >( unsigned int x ) {
    static const unsigned long bits = sizeof( unsigned int ) * 8 - 1;
    return bits - __builtin_clz( x );
}

template<>
inline unsigned MSB< unsigned long >( unsigned long x ) {
    static const unsigned bits = sizeof( unsigned long ) * 8 - 1;
    return bits - __builtin_clzl( x );
}

template<>
inline unsigned MSB< unsigned long long >( unsigned long long x ) {
    static const unsigned bits = sizeof( unsigned long long ) * 8 - 1;
    return bits - __builtin_clzll( x );
}

// gets only Most Significant Bit
template< typename number >
static inline number onlyMSB( number x ) {
    return number(1) << MSB( x );
}

// gets number without Most Significant Bit
template< typename number >
static inline number withoutMSB( number x ) {
    return x & ~onlyMSB( x );
}


}
}

#endif
