#if __cplusplus >= 201103L

#include <wibble/union.h>
#include <string>
#include <sstream>

using namespace wibble;

namespace wibble {

struct A { };
struct B { B() { }; ~B() { } };
struct C { int x; C( int x ) : x( x ) {} C() : x( 0 ) {} };

static_assert( _impl::In< int, int >::value, "" );
static_assert( _impl::In< A, A, B >::value, "" );
static_assert( _impl::In< A, B, A >::value, "" );

template struct Union<>;
template struct Union< int, long >;
template struct Union< int, long, A >;
template struct Union< int, long, A, B >;
template struct Union< int, long, A, B, std::string >;

struct TestUnion {
    Test basic() {
        Union< int > u( 1 );
        assert( !!u );
        assert( !u.empty() );
        assert( u.is< int >() );
        assert_eq( u.get< int >(), 1 );
        u = 2; // move
        assert( !!u );
        assert_eq( u.get< int >(), 2 );
        int i = 3;
        u = i; // copy
        assert( !!u );
        assert_eq( u.get< int >(), 3 );
        u = Union< int >( 4 );
        assert( u.is< int >() );
        assert_eq( u.get< int >(), 4 );
        u = Union< int >();
        assert( !u );
        assert( !u.is< int >() );
        u = 5;
        assert( u );
        assert( u.is< int >() );
        assert_eq( u.get< int >(), 5 );
    }

    Test moveNoCopy() {
        // if one of contained structures does not define copy ctor+assignment
        // move should still be available
        struct Move {
            Move() = default;
            Move( const Move & ) = delete;
            Move( Move && ) = default;

            Move &operator=( Move ) { return *this; }
        };
        Union< long, Move > wierd;
        assert( wierd.empty() );

        wierd = 2L;
        assert( !!wierd );
        assert( wierd.is< long >() );
        assert_eq( wierd.get< long >(), 2L );

        wierd = Move();
        assert( !!wierd );
        assert( wierd.is< Move >() );
    }

    Test ctorCast() {
        assert( ( Union< int, long >{ int( 1 ) }.is< int >() ) );
        assert( ( Union< int, long >{ long( 1 ) }.is< long >() ) );

        assert( ( Union< long, std::string >{ int( 1 ) }.is< long >() ) );

        struct A { operator int(){ return 1; } };
        assert( ( Union< int, A >{ A() }.is< A >() ) );
        assert( ( Union< int, std::string >{ A() }.is< int >() ) );

        struct B { B( int ) { } B() = default; };
        assert( ( Union< int, B >{ B() }.is< B >() ) );
        assert( ( Union< int, B >{ 1 }.is< int >() ) );
        assert( ( Union< B, std::string >{ 1 }.is< B >() ) );
    }

    static C idC( C c ) { return c; };
    static C constC( B b ) { return C( 32 ); };

    Test apply() {
        Union< B, C > u;
        u = B();

        Maybe< C > result = u.match( idC, constC );
        assert( !result.isNothing() );
        assert_eq( result.value().x, 32 );

        u = C( 12 );
        result = u.match( idC, constC );
        assert( !result.isNothing() );
        assert_eq( result.value().x, 12 );

        result = u.match( constC );
        assert( result.isNothing() );
    }
};

}

#endif
