// -*- C++ -*- (c) 2013 Petr Rockai <me@mornfall.net>

#include <cmath>
#include <brick-unittest.h> // ASSERT

#ifndef DIVINE_TOOLKIT_PROBABILITY_H
#define DIVINE_TOOLKIT_PROBABILITY_H

namespace divine {
namespace graph {

constexpr bool isprime( int i, int f ) {
    return (f * f > i) ? true : ( i % f == 0 ) ? false : isprime( i, f + 1 );
}
constexpr bool isprime( int i ) {
    return isprime( i, 2 );
}
constexpr int prime( int i, int p ) {
    return i == 0 ? ( isprime( p ) ? p : prime( i, p + 1 ) )
                  : ( isprime( p ) ? prime( i - 1, p + 1 ) : prime( i, p + 1 ) );
}
constexpr int prime( int i ) {
    return prime( i, 1 );
}

static_assert( prime( 1 ) ==  2, "x" );
static_assert( prime( 2 ) ==  3, "x" );
static_assert( prime( 3 ) ==  5, "x" );
static_assert( prime( 4 ) ==  7, "x" );
static_assert( prime( 5 ) == 11, "x" );

struct Probability
{
    uint64_t _cluster;
    int numerator:16;
    unsigned denominator:16;
    int _level;

    Probability() : Probability( 0 ) {}
    Probability( int tid ) : Probability( pow( 2, tid ), 1, 1 ) {}
    Probability( int c, int x, int y ) : _cluster( c ), numerator( x ), denominator( y ), _level( 0 ) {}

    Probability operator*( std::pair< int, int > x ) {
        auto p = *this;
        p.numerator *= x.first;
        p.denominator *= x.second;
        // p.normalize();
        return p;
    }

    Probability levelup( int i ) {
        auto p = *this;
        ++ p._level;
        p._cluster *= std::pow( prime( p._level ), i );
        return p;
    }

    int cluster() { return _cluster; }
    void cluster( int i ) { _cluster = i; }

    std::string text() const {
        std::stringstream s;
        if ( numerator ) {
            s << _cluster << ": " << numerator << "/" << denominator;
        }
        return s.str();
    }
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, const Probability &p )
{
    return bs << p._cluster << p.numerator << p.denominator;
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, Probability &p )
{
    int x, y;
    auto &r = bs >> p._cluster >> x >> y;
    p.numerator = x;
    p.denominator = y;
    return r;
}

static inline std::ostream &operator<<( std::ostream &os, const Probability &p ) {
    return os << p.text();
}

}
}

#endif
