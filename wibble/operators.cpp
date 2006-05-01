#include <wibble/config.h>
#include <wibble/operators.h>
using namespace std;
using namespace wibble::operators;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>

namespace wibble {
namespace tut {

struct operators_shar {};
TESTGRP( operators );

template<> template<>
void to::test< 1 >() {
    set< int > a, b;
    a.insert( a.begin(), 4 );
    a.insert( a.begin(), 5 );
    b.insert( b.begin(), 5 );
    set< int > c = a & b;
    ensure_equals( c.size(), 1u );
    ensure( c.find( 4 ) == c.end() );
    ensure( c.find( 5 ) != c.end() );
    c = a | b;
    ensure_equals( c.size(), 2u );
    ensure( c.find( 4 ) != c.end() );
    ensure( c.find( 5 ) != c.end() );
    c = a - b;
    ensure_equals( c.size(), 1u );
    ensure( c.find( 4 ) != c.end() );
    ensure( c.find( 5 ) == c.end() );
}

template<> template<>
void to::test< 2 >() {
    set< int > a, b;
    a.insert( a.begin(), 4 );
    a.insert( a.begin(), 3 );
    b.insert( b.begin(), 5 );
    b |= 3;
    ensure_equals( b.size(), 2u );
    ensure( b.find( 2 ) == b.end() );
    ensure( b.find( 3 ) != b.end() );
    ensure( b.find( 4 ) == b.end() );
    ensure( b.find( 5 ) != b.end() );
    b |= a;
    ensure_equals( b.size(), 3u );
    ensure( b.find( 3 ) != b.end() );
    ensure( b.find( 4 ) != b.end() );
    ensure( b.find( 5 ) != b.end() );
    b &= a;
    ensure_equals( b.size(), 2u );
    ensure( b.find( 3 ) != b.end() );
    ensure( b.find( 4 ) != b.end() );
    ensure( b.find( 5 ) == b.end() );
    b.insert( b.begin(), 2 );
    b -= a;
    ensure_equals( b.size(), 1u );
    ensure( b.find( 2 ) != b.end() );
    ensure( b.find( 3 ) == b.end() );
    ensure( b.find( 4 ) == b.end() );
}

}
}

#endif
