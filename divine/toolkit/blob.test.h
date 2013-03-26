// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/blob.h>
#include <divine/toolkit/pool.h>
#include <wibble/sys/thread.h>

using namespace divine;

struct TestBlob {
    Test basic() {
        BlobDereference bd;
        Blob b( 32 );
        bd.get< int >( b ) = 42;
        assert_eq( bd.get< int >( b ), 42 );

        Blob c( sizeof( int ) );
        bd.get< int >( c ) = 42;
        assert_eq( bd.get< int >( c ), 42 );
    }

    template< typename T >
    void comparison() {
        Pool p;
        Blob b1( p, sizeof( T ) ), b2( p, sizeof( T ) ), b3( p, sizeof( T ) );
        BlobComparerEQ ecomp( p );
        BlobComparerLT lecomp( p );
        p.get< T >( b1 ) = 32;
        p.get< T >( b2 ) = 32;
        p.get< T >( b3 ) = 42;
        assert_eq( p.get< T >( b1 ), 32 );
        assert_eq( p.get< T >( b2 ), 32 );
        assert_eq( p.get< T >( b3 ), 42 );
        assert( lecomp( b1, b3 ) );
        assert( !lecomp(b1, b2) );
        assert( !lecomp(b2, b1) );
        assert( ecomp( b1, b2 ) );
        assert( !ecomp(b1, b3) );
        assert( !ecomp(b2, b3) );
    }

    Test comparisonShort() {
        comparison< short >();
    }

    Test comparisonInt() {
        comparison< int >();
    }

    Test comparisonChar() {
        comparison< char >();
    }

    Test allocSize() {
        for ( unsigned i = 0; i < 100; ++i )
            assert( i <= Blob::allocationSize( i ) );
    }

    Test writeAndRead() {
    /* TODO: use streams
        Pool p;
        Blob b1( p, sizeof( int ) ), b2;
        int32_t buf[ 2 ];

        p.get< int >( b1 ) = 42;
        b1.write32( buf );
        int32_t *end = b2.read32( &p, buf );
        assert_eq( b1.get< int >(), 42 );
        assert_eq( b1.get< int >(), b2.get< int >() );
        assert( b1 == b2 );

        assert_eq( buf + 2, end );
    */
    }

    struct Worker : wibble::sys::Thread {

        Blob &data;
        unsigned sequence;

        Worker( Blob &d, unsigned s )
            : data( d ), sequence( s )
        {}

        void *main() {
            for ( unsigned s = 0; s < sequence; ++s ) {
                data.acquire();

                data.get< int >() = 1;
                assert( data.get< int >() );
                data.get< int >() = 0;

                data.release();
            }
            return nullptr;
        }
    };

    Test concurrent() {
        Blob data( sizeof( int ) );
        data.get< int >() = 0;

        Worker a( data, 1000 );
        Worker b( data, 1000 );

        a.start();
        b.start();
        a.join();
        b.join();

        data.free();
    }
};
