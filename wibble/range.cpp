/** -*- C++ -*-
    @file wibble/range.cpp
    @author Peter Rockai <me@mornfall.net>
*/

#include <wibble/config.h>
#include <wibble/range.h>
using namespace wibble;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>
#include <list>

namespace wibble {
namespace tut {

struct range_shar {
};

TESTGRP( range );

template<> template<>
void to::test<1> ()
{
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    ensure_equals( r.current(), 10 );
    ensure_equals( (++r).current(), 20 );
    ensure( ++r == r.end() );
}

template<> template<>
void to::test<2> ()
{
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    std::list<int> b;
    ensure( a != b );
    std::copy( r, r.end(), back_inserter( b ) );
    ensure( a == b );
}

template<> template<>
void to::test<3> ()
{
    VectorRange<int> vr;
    Range<int> _vr = vr;
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    std::copy( r, r.end(), consumer( vr ) );
    Range< int > _vr1 = _vr;
    ensure_equals( *_vr1, 10 );
    ++_vr1;
    ensure_equals( *_vr1, 20 );
    ++_vr1;
    ensure( _vr1 == _vr1.end() );
    while ( r != r.end() )
        ensure_equals( *r++, *_vr++ );
}

template<> template<>
void to::test<4> ()
{
    VectorRange<int> vr;
    Range<int> _vr = vr;
    Range<int> _fr =
        filteredRange( _vr,
                       std::bind1st( std::equal_to< int >(), 10 ) );
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    r.output( consumer( vr ) );
    ensure_equals( *_fr, 10 );
    ++_fr;
    ensure( _fr == _fr.end() );
}

template<> template<>
void to::test<5> ()
{
    VectorRange<int> vr;
    std::list<int> a;
    a.push_back( 20 );
    a.push_back( 10 );
    a.push_back( 30 );
    Range< int > r = range( a.begin(), a.end() );
    r.output( consumer( vr ) );
    std::sort( vr, vr.end() );
    ensure_equals( *vr, 10 );
    ++vr;
    ensure_equals( *vr, 20 );
    ++vr;
    ensure_equals( *vr, 30 );
    ++vr;
    ensure( vr == vr.end() );
}

template<> template<>
void to::test<6> ()
{
    std::set<int> a;
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 20 );
    Range< int > r = range( a.begin(), a.end() ).sorted();
    ensure_equals( *r, 10 );
    ++r;
    ensure_equals( *r, 20 );
    ++r;
    ensure_equals( *r, 30 );
    ++r;
    ensure( r == r.end() );
}

template<> template<>
void to::test<7> ()
{
    std::vector<int> a;
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 20 );
    Range< int > r = range( a.begin(), a.end() ).sorted();
    ensure_equals( *r, 10 );
    ++r;
    ensure_equals( *r, 20 );
    ++r;
    ensure_equals( *r, 30 );
    ++r;
    ensure( r == r.end() );
}

template<> template<>
void to::test<8> ()
{
    VectorRange< int > a;
    a.consume( 10 );
    a.consume( 30 );
    a.consume( 20 );
    VectorRange< int > b;
    b.consume( 5 );
    b.consume( 19 );
    b.consume( 30 );
    b.consume( 10 );
    Range< int > r = intersectionRange( a.sorted(), b.sorted() );
    ensure_equals( *r, 10 );
    ++r;
    ensure_equals( *r, 30 );
    ++r;
    ensure( r == r.end() );
}


template<> template<>
void to::test<9> ()
{
    std::vector<int> a;
    a.push_back( 10 );
    a.push_back( 30 );
    a.push_back( 20 );
    std::vector<int> b;
    b.push_back( 5 );
    b.push_back( 19 );
    b.push_back( 30 );
    b.push_back( 10 );
    Range< int > r = intersectionRange(
        range( a.begin(), a.end() ).sorted(),
        range( b.begin(), b.end() ).sorted() );
    ensure_equals( *r, 10 );
    ++r;
    ensure_equals( *r, 30 );
    ++r;
    ensure( r == r.end() );
}

template<> template<>
void to::test<10> ()
{
    std::vector<int> a;
    a.push_back( 10 );
    a.push_back( 30 );
    a.push_back( 20 );
    std::vector<int> b;
    b.push_back( 5 );
    b.push_back( 19 );
    b.push_back( 30 );
    b.push_back( 10 );
    b.push_back( 40 );
    b.push_back( 50 );
    Range< int > r = intersectionRange(
        range( a.begin(), a.end() ).sorted(),
        range( b.begin(), b.end() ).sorted() );
    ensure_equals( *r, 10 );
    r.advance();
    ensure_equals( *r, 30 );
    r.advance();
    ensure( r == r.end() );
}

template<> template<>
void to::test<11> ()
{
    Range< int > a;
    ensure( a.empty() );
    a = VectorRange< int >();
    ensure( a.empty() );
    VectorRange< int > x;
    x.consume( 4 );
    a = x;
    ensure( !a.empty() );
}

template<> template<>
void to::test<12> ()
{
    Range< int > a;
    VectorRange< int > x;
    x.consume( 4 );
    x.consume( 8 );
    a = transformedRange( range( x ), std::bind1st( std::plus< int >(), 2 ) );
    ensure_equals( *a, 6 );
    ++a;
    ensure_equals( *a, 10 );
    ++a; ensure( a == a.end() );
}

template<> template<>
void to::test<13> ()
{
    Range< int > a;
    VectorRange< unsigned > x;
    x.consume( 4 );
    x.consume( 8 );
    a = transformedRange(
        range( x ), std::bind1st( std::plus< int >(), 2 ) );;
    ensure_equals( *a, 6 );
    ++a;
    ensure_equals( *a, 10 );
    ++a;
    ensure( a == a.end() );
}

template<> template<>
void to::test<14> ()
{
    std::vector<int> a;
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 20 );
    Range< int > r = range( a.begin(), a.end() );
    ensure_equals( *r, 20 );
    r = r.next();
    ensure_equals( *r, 10 );
    r = r.next();
    ensure_equals( *r, 30 );
    r = r.next();
    ensure( r == r.end() );
}

template<> template<>
void to::test<15> ()
{
    std::vector<int> a;
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 20 );
    Range< unsigned > r = castedRange< unsigned >( range( a.begin(), a.end() ) );
    ensure_equals( *r, 20u );
    r = r.next();
    ensure_equals( *r, 10u );
    r = r.next();
    ensure_equals( *r, 30u );
    r = r.next();
    ensure( r == r.end() );
}

template<> template<>
void to::test<16> ()
{
    std::vector<int> a;
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 20 );
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 20 );
    Range< int > r = uniqueRange( range( a.begin(), a.end() ).sorted() );
    ensure_equals( *r, 10 );
    r = r.next();
    ensure_equals( *r, 20 );
    r = r.next();
    ensure_equals( *r, 30 );
    r = r.next();
    ensure( r == r.end() );
}

/* template<> template<>
void to::test<17> ()
{
    std::vector<int> a;
    a.insert( a.begin(), 10 );
    a.insert( a.begin(), 30 );
    a.insert( a.begin(), 20 );
    Range< int > r = range( a.begin(), a.end() );
    ensure( r.contains( 10 ) );
    ensure( r.contains( 30 ) );
    ensure( r.contains( 20 ) );
    ensure( !r.contains( 25 ) );
    ensure( !r.contains( 15 ) );
} */

static void advance( int &i ) {
    ++i;
}
static bool isEnd( const int &i ) {
    return i >= 5;
}

template<> template<>
void to::test< 18 >() {
    Range< int > r = generatedRange( 0, advance, isEnd );
    ensure_equals( *r, 0 );
    r.advance(); ensure_equals( *r, 1 );
    r.advance(); ensure_equals( *r, 2 );
    r.advance(); ensure_equals( *r, 3 );
    r.advance(); ensure_equals( *r, 4 );
    r.advance(); ensure( r == r.end() );
}

template<> template<>
void to::test< 19 >() {
    VectorRange< int > r1 = VectorRange< int >();
    VectorRange< int > r2 = VectorRange< int >();
    r1.consume( 1 );
    r1.consume( 2 );
    r2.consume( 3 );
    r2.consume( 1 );
    Range< int > r = intersectionRange( r1.sorted(), r2.sorted() );
    ensure_equals( *r, 1 );
    r.advance();
    ensure( r == r.end() );
}

template<> template<>
void to::test< 20 >() {
    VectorRange< int > r1 = VectorRange< int >();
    VectorRange< int > r2 = VectorRange< int >();
    r1.consume( 1 );
    r1.consume( 2 );
    Range< int > r = intersectionRange( r1.sorted(), r2.sorted() );
    ensure( r == r.end() );
}

}

}

#endif
