#include <wibble/config.h>
#include <wibble/operators.h>
using namespace std;
using namespace wibble::operators;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct operators_shar {};
TESTGRP( operators );

static set<int> mkset(int i1)
{
	set<int> a; a.insert(i1); return a;
}
static set<int> mkset(int i1, int i2)
{
	set<int> a; a.insert(i1); a.insert(i2); return a;
}
static set<int> mkset(int i1, int i2, int i3)
{
	set<int> a; a.insert(i1); a.insert(i2); a.insert(i3); return a;
}
static set<int> mkset(int i1, int i2, int i3, int i4)
{
	set<int> a; a.insert(i1); a.insert(i2); a.insert(i3); a.insert(i4); return a;
}

template<> template<>
void to::test< 1 >() {
    set< int > a = mkset(4, 5);
	set< int > b = mkset(5);
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
    set< int > a = mkset(4, 3);
	set< int > b = mkset(5);
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

template<> template<>
void to::test< 3 >() {
    set< int > a;

    a = a | wibble::Empty<int>();
    ensure_equals( a.size(), 0u );

    a = a | wibble::Singleton<int>(1);
    ensure_equals( a.size(), 1u );
    ensure( a.find( 1 ) != a.end() );

    a = a - wibble::Empty<int>();
    ensure_equals( a.size(), 1u );
    ensure( a.find( 1 ) != a.end() );

    a = a - wibble::Singleton<int>(1);
    ensure_equals( a.size(), 0u );
    ensure( a.find( 1 ) == a.end() );

    a |= wibble::Empty<int>();
    ensure_equals( a.size(), 0u );

    a |= wibble::Singleton<int>(1);
    ensure_equals( a.size(), 1u );
    ensure( a.find( 1 ) != a.end() );

    a -= wibble::Empty<int>();
    ensure_equals( a.size(), 1u );
    ensure( a.find( 1 ) != a.end() );

    a -= wibble::Singleton<int>(1);
    ensure_equals( a.size(), 0u );
    ensure( a.find( 1 ) == a.end() );
}

template<> template<>
void to::test< 4 >() {
    set< int > a, b;
    ensure( a <= b );
    ensure( b <= a );
}

}
