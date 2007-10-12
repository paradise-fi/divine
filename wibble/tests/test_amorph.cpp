/** -*- C++ -*-
    @file wibble/tests/test_amorph.cpp
    @author Peter Rockai <me@mornfall.net>
*/

#include <wibble/config.h>
#include <wibble/amorph.h>

#include <wibble/tests/tut-wibble.h>
#include <list>

namespace tut {

using namespace wibble;

struct amorph_shar {
};

TESTGRP( amorph );

struct TestInterface {
    virtual int value() = 0;
};

template< typename W >
struct TestMorph : Morph< TestMorph< W >, W, TestInterface >
{
    TestMorph() {}
    TestMorph( const W &w ) : Morph< TestMorph, W, TestInterface >( w ) {}

    virtual int value() { return this->wrapped().value(); }
};

struct Test : Amorph< Test, TestInterface >
{
    Test( const MorphInterface< TestInterface > &i )
        : Amorph< Test, TestInterface >( i ) {}
    Test() {}

    int value() {
        return this->implementation()->value();
    }
};

struct Test1 : VirtualBase {
    virtual int value() const { return 1; }
    bool operator<=( const Test1 &o ) const {
        return value() <= o.value();
    }

};

struct Test3 : Test1 {
    virtual int value() const { return 3; }
};

struct Test2 {
    int value() const { return 2; }
    bool operator<=( const Test2 &o ) const {
        return value() <= o.value();
    }
};

struct ExtractT1Value {
    typedef int result_type;
    typedef Test1 argument_type;
    int operator()( const Test1 &t ) {
        return t.value();
    }
};

template< typename T >
TestMorph< T > testMorph( T t ) {
    return TestMorph< T >( t );
}

template<> template<>
void to::test<1> ()
{
    Test1 t1;
    Test2 t2;
    Test3 t3;
    Test t = testMorph( t1 );
    ensure_equals( t.value(), 1 );
    ensure_equals( t.ifType( ExtractT1Value() ), Maybe< int >::Just( 1 ) );
    t = testMorph( t2 );
    ensure_equals( t.value(), 2 );
    ensure_equals( t.ifType( ExtractT1Value() ), Maybe< int >::Nothing() );
    t = testMorph( t3 );
    ensure_equals( t.value(), 3 );
    ensure_equals( t.ifType( ExtractT1Value() ), Maybe< int >::Just( 3 ) );
}

}
