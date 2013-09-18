// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/hashset.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/pool.h>

namespace divine {

template<>
struct default_hasher< Lake::Pointer >
{
    typedef Lake::Pointer T;
    Pool& _pool;
    Pool &pool() { return _pool; }
    default_hasher( Pool& p ) : _pool( p ) { }
    default_hasher( const default_hasher &o ) : _pool( o._pool ) {}
    hash128_t hash( T t ) const { return _pool.hash( t ); }
    bool valid( T t ) const { return _pool.valid( t ); }
    bool equal( T a, T b ) const { return _pool.equal( a, b ); }
};

template<>
struct default_hasher< int > {
    template< typename X >
    default_hasher( X& ) { }
    default_hasher() = default;
    hash128_t hash( int t ) const { return std::make_pair( t, t ); }
    bool valid( int t ) const { return t != 0; }
    bool equal( int a, int b ) const { return a == b; }
};

}

using namespace divine;

struct TestHashSet {
    Test basic() {
        FastSet< int > set;
        set.insert( 1 );
        assert( set.count( 1 ) );
    }

    Test stress() {
        FastSet< int > set;
        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            assert( set.count( i ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.count( i ) );
        }
    }

    Test set() {
        FastSet< int > set;

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( !set.count( i ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            assert( set.count( i ) );
            assert( !set.count( i + 1 ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.count( i ) );
        }

        for ( int i = 32*1024; i < 64 * 1024; ++i ) {
            assert( !set.count( i ) );
        }
    }

    Test blobish() {
        Pool p;
        FastSet< Blob > set( ( typename FastSet< Blob >::Hasher( p ) ) );
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b = p.allocate( sizeof( int ) );
            p.get< int >( b ) = i;
            set.insert( b );
            assert( set.count( b ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b = p.allocate( sizeof( int ) );
            p.get< int >( b ) = i;
            assert( set.count( b ) );
        }
    }
};
