#if __cplusplus >= 201103L

#include <wibble/union.h>
#include <string>
#include <sstream>

using namespace wibble;

namespace wibble {

struct A { };
struct B { B() { }; ~B() { } };

static_assert( _impl::In< int, int >::value, "" );
static_assert( _impl::In< A, A, B >::value, "" );
static_assert( _impl::In< A, B, A >::value, "" );

template class Union<>;
template class Union< int, long >;
template class Union< int, long, A >;
template class Union< int, long, A, B >;
template class Union< int, long, A, B, std::string >;

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

    Test sstream() {
        Union< long, std::stringstream > wierd;
        assert( wierd.empty() );

        wierd = 2L;
        assert( !!wierd );
        assert( wierd.is< long >() );

        wierd = std::stringstream( "baf" );
        assert( !!wierd );
        assert( wierd.is< std::stringstream >() );
    }
};

}

#endif
