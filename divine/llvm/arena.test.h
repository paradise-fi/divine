// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>

#include <divine/llvm/arena.h>
#include <cstring>

#define _unused(x) ((void)x)

using namespace divine::llvm;
using namespace divine;

struct TestArena {
    Test index() {
        Arena::Index i( 3, 8 );
        int x = i;
        Arena::Index j = x;
        assert_eq( i, j );
        assert_eq( x, j );
        assert_eq( x, i );       
    }

    Test allocate() {
        Arena a;
        Arena::Index i = a.allocate( 5 );
        strcpy( (char *) a.translate( i ), "hey!" );
        assert( !strcmp( (char *) a.translate( i ), "hey!" ) );
        Arena::Index j = a.allocate( 9 );
        assert( !strcmp( (char *) a.translate( i ), "hey!" ) );
	_unused (j);
    }

    Test free() {
        Arena a;
        Arena::Index i = a.allocate( 5 ),
                     j = a.allocate( 9 ),
                     k = a.allocate( 5 );
        a.free( k );
        assert_eq( a.allocate( 5 ), k );
	_unused(i);
	_unused(j);
    }

    Test spill() {
        Arena a;
        Arena::Index i = a.allocate( 5 );
        strcpy( (char *) a.translate( i ), "hey!" );
        assert( !strcmp( (char *) a.translate( i ), "hey!" ) );
        Arena::Index x[ 10000 ];
        for ( int i = 0; i < 10000; ++i ) {
            x[ i ] = a.allocate( 5 );
            *(int *)a.translate( x[ i ] ) = i;
        }

        for ( int i = 0; i < 10000; ++i )
            assert_eq( *(int *)a.translate( x[ i ] ), i );
        for ( int i = 2000; i < 7000; ++i )
            a.free( x[ i ] );

        for ( int i = 2000; i < 7000; ++i ) {
            x[ i ] = a.allocate( 5 );
            *(int *)a.translate( x[ i ] ) = i;
        }

        for ( int i = 0; i < 10000; ++i )
            assert_eq( *(int *)a.translate( x[ i ] ), i );

        assert( !strcmp( (char *) a.translate( i ), "hey!" ) );
    }

    Test expand() {
        Arena a, b;
        Pool p;
        Arena::Index i = a.allocate( 5 );
        strcpy( (char *) a.translate( i ), "hey!" );
        assert( !strcmp( (char *) a.translate( i ), "hey!" ) );

        Arena::Index j = a.allocate( 9 );
        divine::Blob blob = a.compact( 0, p );
        b.expand( blob, 0 );
	_unused(j);

        assert( !strcmp( (char *) b.translate( i ), "hey!" ) );
    }
};
