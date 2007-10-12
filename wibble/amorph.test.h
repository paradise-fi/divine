// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <wibble/amorph.h>

namespace {

using namespace wibble;

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

struct _TestAmorph {
    Test basic()
    {
        Test1 t1;
        Test2 t2;
        Test3 t3;
        Test t = testMorph( t1 );
        assert_eq( t.value(), 1 );
        assert_eq( t.ifType( ExtractT1Value() ), Maybe< int >::Just( 1 ) );
        t = testMorph( t2 );
        assert_eq( t.value(), 2 );
        assert_eq( t.ifType( ExtractT1Value() ), Maybe< int >::Nothing() );
        t = testMorph( t3 );
        assert_eq( t.value(), 3 );
        assert_eq( t.ifType( ExtractT1Value() ), Maybe< int >::Just( 3 ) );
    }
};

}

typedef _TestAmorph TestAmorph;
