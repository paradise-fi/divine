/** -*- C++ -*-
    @file wibble/range.cpp
    @author Peter Rockai <me@mornfall.net>
*/

#include <wibble/config.h>
#include <wibble/range.h>
#include <wibble/operators.h>
using namespace wibble;
using namespace operators;

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
    Range< int >::iterator i = r.begin();
    ensure_equals( *i, 10 );
    ensure_equals( *(i + 1), 20 );
    ensure( i + 2 == r.end() );
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
    std::copy( r.begin(), r.end(), back_inserter( b ) );
    ensure( a == b );
}

template<> template<>
void to::test< 3 >()
{
    std::vector< int > &vec = *new (GC) std::vector< int >;
    std::list< int > a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    std::copy( r.begin(), r.end(), consumer( vec ) );
    Range< int > r1 = range( vec );
    ensure_equals( *r1.begin(), 10 );
    ensure_equals( *(r1.begin() + 1), 20 );
    ensure( r1.begin() + 2 == r1.end() );
    while ( !r.empty() ) {
        ensure_equals( *r, *r1 );
        r.advance();
        r1.advance();
    }
}

template<> template<>
void to::test< 4 > ()
{
    std::vector< int > v;
    std::list<int> a;
    a.push_back( 10 );
    a.push_back( 20 );
    Range< int > r = range( a.begin(), a.end() );
    r.output( consumer( v ) );

    Range<int> fr =
        filteredRange( range( v ),
                       std::bind1st( std::equal_to< int >(), 10 ) );
    ensure_equals( *fr, 10 );
    fr.advance();
    ensure( fr.empty() );
}

template<> template<>
void to::test<5> ()
{
    std::vector< int > v;
    std::list<int> a;
    a.push_back( 20 );
    a.push_back( 10 );
    a.push_back( 30 );
    Range< int > r = range( a.begin(), a.end() );
    r.output( consumer( v ) );
    std::sort( v.begin(), v.end() );
    ensure_equals( *(v.begin()), 10 );
    ensure_equals( *(v.begin() + 1), 20 );
    ensure_equals( *(v.begin() + 2), 30 );
    ensure( v.begin() + 3 == v.end() );
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
    r.advance();
    ensure_equals( *r, 20 );
    r.advance();
    ensure_equals( *r, 30 );
    r.advance();
    ensure( r.empty() );
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
    r.advance();
    ensure_equals( *r, 20 );
    r.advance();
    ensure_equals( *r, 30 );
    r.advance();
    ensure( r.empty() );
}

template<> template<>
void to::test<8> ()
{
    std::vector< int > av, bv;
    Consumer< int > a = consumer( av ), b = consumer( bv );
    a.consume( 10 );
    a.consume( 30 );
    a.consume( 20 );
    b.consume( 5 );
    b.consume( 19 );
    b.consume( 30 );
    b.consume( 10 );
    Range< int > r = intersectionRange( range( av ).sorted(), range( bv ).sorted() );
    ensure_equals( *r.begin(), 10 );
    ensure_equals( *(r.begin() + 1), 30 );
    ensure( r.begin() + 2 == r.end() );
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
    r.advance();
    ensure_equals( *r, 30 );
    r.advance();
    ensure( r.empty() );
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
    ensure( r.empty() );
}

template<> template<>
void to::test<11> ()
{
    std::vector< int > vec;
    Range< int > a;
    ensure( a.empty() );
    a = range( vec );
    ensure( a.empty() );
    vec.push_back( 4 );
    Range< int > b = range( vec );
    ensure( *b == 4 );
    a = b;
    ensure( !a.empty() );
    ensure_equals( *a, 4 );
}

template<> template<>
void to::test<12> ()
{
    Range< int > a;
    std::vector< int > xv;
    Consumer< int > x = consumer( xv );
    x.consume( 4 );
    x.consume( 8 );
    a = transformedRange( range( xv ), std::bind1st( std::plus< int >(), 2 ) );
    ensure_equals( *a, 6 );
    a.advance();
    ensure_equals( *a, 10 );
    a.advance();
    ensure( a.empty() );
}

template<> template<>
void to::test<13> ()
{
    Range< int > a;
    std::vector< unsigned > xv;
    Consumer< unsigned > x = consumer( xv );
    x.consume( 4 );
    x.consume( 8 );
    a = transformedRange(
        range( xv ), std::bind1st( std::plus< int >(), 2 ) );
    ensure_equals( *a, 6 );
    a.advance();
    ensure_equals( *a, 10 );
    a.advance();
    ensure( a.empty() );
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
    ensure( r.empty() );
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
    ensure( r.empty() );
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
    ensure( r.empty() );
}

/*
template<> template<>
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
}
*/

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
    r.advance(); ensure( r.empty() );
}

template<> template<>
void to::test<19> ()
{
    std::vector< int > av;
    Consumer< int > a = consumer( av );
    a.consume( 10 );
    a.consume( 30 );
    a.consume( 20 );
    Range< int > r = range( av ).sorted();
    ensure_equals( *r.begin(), 10 );
    ensure_equals( *(r.begin() + 1), 20 );
    ensure_equals( *(r.begin() + 2), 30 );
    ensure( r.begin() + 3 == r.end() );
}

template<> template<>
void to::test<20> ()
{
    std::vector< int > av;
    Consumer< int > a = consumer( av );
    a.consume( 10 );
    a.consume( 30 );
    a.consume( 20 );
    Range< unsigned > r = range( av ).sorted();
    ensure_equals( *r.begin(), 10 );
    ensure_equals( *(r.begin() + 1), 20 );
    ensure_equals( *(r.begin() + 2), 30 );
    ensure( r.begin() + 3 == r.end() );
}

/*
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

template<> template<>
void to::test< 21 >() {
    Consumer< int > c = consumer( shared< std::set< int > >() );
    c.consume( 1 );
    c.consume( 2 );
}
*/

}

}

#endif
