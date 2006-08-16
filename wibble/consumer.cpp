/** -*- C++ -*-
    @file utils/consumer.cpp
    @author Peter Rockai <me@mornfall.net>
*/

#include <wibble/config.h>
#include <wibble/consumer.h>
#include <wibble/operators.h>

using namespace wibble::operators;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests/tut-wibble.h>
#include <list>

namespace tut {

using namespace wibble;

struct consumer_shar {
};

TESTGRP( consumer );

template<> template<>
void to::test<1> ()
{
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    std::list<int> b;
    ensure( a != b );
    Consumer< int > c = consumer( back_inserter( b ) );
    std::copy( r.begin(), r.end(), c );
    ensure( a == b );
}

template<> template<>
void to::test<2> ()
{
    std::set< int > s;
    Consumer< int > c = consumer( s );
    c.consume( 1 );
    ensure( *s.begin() == 1 );
    ensure( s.begin() + 1 == s.end() );
}

template<> template<>
void to::test<3> ()
{
    std::vector< int > v;
    Consumer< int > c = consumer( v );
    c.consume( 2 );
    c.consume( 1 );
    ensure( *v.begin() == 2 );
    ensure( *(v.begin() + 1) == 1 );
    ensure( (v.begin() + 2) == v.end() );
}

}

#endif
