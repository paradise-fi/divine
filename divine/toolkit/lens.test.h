#include "divine/toolkit/pool.h"
#include "divine/toolkit/lens.h"
#include "divine/toolkit/blob.h"

#include <iostream>

using namespace divine::lens;

struct TestLens {

    divine::Pool pool;

    Test tuple() {
        divine::Blob blob = pool.allocate( 3 * sizeof( int ) + sizeof( float ) );
        pool.clear( blob );

        struct IntA { int i; IntA( int i = 0 ) : i( i ) {} };
        struct IntB { int i; IntB( int i = 0 ) : i( i ) {} };
        struct IntC { int i; IntC( int i = 0 ) : i( i ) {} };

        typedef Tuple< IntA, IntB, IntC, float > Foo;

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Foo > lens( a );

        lens.get( IntA() ) = 1;
        lens.get( IntB() ) = 2;
        lens.get( IntC() ) = 3;

        assert_eq( divine::fmtblob( pool, blob ), "[ 1, 2, 3, 0 ]" );
    }

    Test fixedArray() {
        divine::Blob blob = pool.allocate( sizeof( int ) * 10 );
        pool.clear( blob );

        typedef FixedArray< int > Array;

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Array > lens( a );

        lens.get().length() = 8;
        for ( int i = 0; i < 8; ++i )
            lens.get( i ) = i + 1;

        assert_eq( divine::fmtblob( pool, blob ), "[ 8, 1, 2, 3, 4, 5, 6, 7, 8, 0 ]" );
    }

    Test basic() {
        divine::Blob blob = pool.allocate( 200 );
        pool.clear( blob );

        struct IntArray1 : FixedArray< int > {};
        struct IntArray2 : FixedArray< int > {};
        struct Matrix : Array< FixedArray< int > > {};
        struct Witch : Tuple< IntArray1, IntArray2 > {};
        struct Doctor: Tuple< IntArray1, int, Witch, float, Matrix > {};

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Doctor > lens( a );

        lens.get( IntArray1() ).length() = 5;
        lens.get( Witch(), IntArray1() ).length() = 3;
        lens.get( Witch(), IntArray2() ).length() = 4;
        lens.get( float() ) = -1;

        lens.get( Matrix() ).length() = 4;
        lens.get( Matrix(), 0 ).length() = 4;
        lens.get( Matrix(), 1 ).length() = 4;
        lens.get( Matrix(), 2 ).length() = 4;
        lens.get( Matrix(), 3 ).length() = 4;

        for ( int i = 0; i < 4; ++i )
            for ( int j = 0; j < 4; ++j )
                lens.get( Matrix(), i, j ) = 100 + j + i * j;

        lens.get( int() ) = int( 365 );

        assert_eq( divine::fmtblob( pool, blob ),
                   "[ 5, 0, 0, 0, 0, 0, 365, 3, 0, 0, 0, 4, 0, 0, 0, 0,"
                   " 3212836864, 4, 4, 100, 101, 102, 103, 4, 100, 102,"
                   " 104, 106, 4, 100, 103, 106, 109, 4, 100, 104, 108,"
                   " 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]" );

        for ( int i = 0; i < lens.get( IntArray1() ).length(); ++ i )
            lens.get( IntArray1(), i ) = i + 10;
        for ( int i = 0; i < lens.get( Witch(), IntArray1() ).length(); ++ i )
            lens.get( Witch(), IntArray1(), i ) = i + 20;
        for ( int i = 0; i < lens.get( Witch(), IntArray2() ).length(); ++ i )
            lens.get( Witch(), IntArray2(), i ) = i + 30;


        Lens< LinearAddress, Witch > lens2 = lens.sub( Witch() );

        assert_eq( lens.get( int() ), 365 );

        assert_eq( lens.get( IntArray1() ).length(), 5 );
        assert_eq( lens.get( IntArray1(), 0 ), 10 );
        assert_eq( lens2.get( IntArray1(), 1 ), 21 );
        assert_eq( lens.get( Witch(), IntArray2(), 0 ), 30 );

        assert_eq( divine::fmtblob( pool, blob ),
                   "[ 5, 10, 11, 12, 13, 14, 365, 3, 20, 21, 22, 4, 30, 31, 32, 33, 3212836864,"
                   " 4, 4, 100, 101, 102, 103, 4, 100, 102, 104, 106, 4, 100, 103, 106, 109, 4,"
                   " 100, 104, 108, 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]" );
    }

    Test copy() {
        int size = 2 * sizeof( int );
        divine::Blob blob = pool.allocate( size ), copy = pool.allocate( size );
        pool.clear( blob );
        pool.clear( copy );

        struct IntA { int i; IntA( int i = 0 ) : i( i ) {} };
        struct IntB { int i; IntB( int i = 0 ) : i( i ) {} };

        typedef Tuple< IntA, IntB > Foo;

        LinearAddress a( &pool, blob, 0 );
        LinearAddress b( &pool, copy, 0 );

        Lens< LinearAddress, Foo > lens( a );

        lens.get( IntA() ) = 1;
        lens.get( IntB() ) = 2;

        lens.sub( IntA() ).copy( b );
        assert_eq( divine::fmtblob( pool, copy ), "[ 1, 0 ]" );
        pool.clear( copy );

        lens.copy( b );
        assert_eq( divine::fmtblob( pool, copy ), "[ 1, 2 ]" );
    }

};
