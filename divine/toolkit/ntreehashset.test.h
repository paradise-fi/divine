// -*- C++ -*- (c) 2013 Vladimir Still <xstill@fi.muni.cz>
//             (c) 2013 Petr Ročkai <me@mornfall.net>

#ifndef TESTS_NTREE_HASH_SET
#define TESTS_NTREE_HASH_SET

#include <divine/toolkit/ntreehashset.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/blob.h>
#include <divine/algorithm/common.h> // hasher
#include <divine/toolkit/pool.h>
#include <random>
#include <cstdint>
#include <algorithm>
#include <numeric>

using namespace divine;
using divine::algorithm::Hasher;

struct FakeGeneratorFlat {
    Pool *_pool;
    Pool& pool() {
        return *_pool;
    }

    template< typename Yield >
    void splitHint( Blob, intptr_t, intptr_t length, Yield yield ) {
        yield( Recurse::No, length, 0 );
    }

    template< typename Yield >
    void splitHint( Blob n, Yield yield ) {
        splitHint( n, 0, pool().size( n ), yield );
    }
};

struct FakeGeneratorBinary {

    Pool *_pool;
    Pool& pool() {
        return *_pool;
    }

    template< typename Yield >
    void splitHint( Blob, intptr_t, intptr_t length, Yield yield ) {
        if ( length < 32 )
            yield( Recurse::No, length, 0 );
        else {
            auto half = length >> 1;
            yield( Recurse::Yes, length - half, 1 );
            yield( Recurse::Yes, half, 0 );
        }
    }

    template< typename Yield >
    void splitHint( Blob n, Yield yield ) {
        splitHint( n, 0, pool().size( n ), yield );
    }
};

#if 0
struct BestNTreeHashSet {
    using BlobSet = NTreeHashSet< FastSet, Blob, Hasher >;

    FakeGeneratorBinary fgb;
    FakeGeneratorFlat fgf;
    Pool pool;
    Hasher hasher;

    BlobSet::ThreadData< FakeGeneratorBinary > tdb;
    BlobSet::ThreadData< FakeGeneratorFlat > tdf;

    TestNTreeHashSet()
        : hasher( pool ), tdb( pool, fgb ), tdf( pool, fgf )
    {
        fgf._pool = fgb._pool = &pool;
    }

    static unsigned c2u( char c ) {
        return static_cast< unsigned >( static_cast< unsigned char >( c ) );
    }

    Test binary2() {
        auto set = BlobSet( hasher ).withTD( tdb );

        assert_eq( hasher.slack, 0 );

        Blob b = pool.allocate( 33 );
        for ( unsigned i = 0; i < 33; ++i )
            pool.dereference( b )[ i ] = char(i & 0xff);

        auto root = set.insertHinted( b, hasher.hash( b ).first );
        assert( root.isnew() );
        assert( !root->leaf( pool ) );
        assert_eq( size_t( pool.size( root->b ) ),
                   sizeof( BlobSet::Root::Header ) + 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( root->forkcount( pool ), 2 );

        auto children = root->childvector( pool );

        assert( children[ 0 ].isLeaf() );
        assert( children[ 1 ].isLeaf() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].leaf().size( pool ), 17 );
        assert_eq( children[ 1 ].leaf().size( pool ), 16 );

        for ( unsigned i = 0; i < 33; ++i )
            assert_eq( c2u( children[ i / 17 ].leaf().data( pool )[ i % 17 ] ), i & 0xff );

        Blob b2 = root->reassemble( pool );
        assert( pool.equal( b, b2 ) );

        assert( !set.insertHinted( b, hasher.hash( b ).first ).isnew() );

        auto root2 = set.get( b );
        assert( !root2.isnew() );
        assert_eq( root->b.raw(), root2->b.raw() );

        for ( auto x : { b, b2 } )
            pool.free( x );
    }

    Test binary4() {
        BlobSet set( hasher );
        set._global.generator = &fgb;

        Blob b = pool.allocate( 67 );
        for ( unsigned i = 0; i < 67; ++i )
            pool.dereference( b )[ i ] = char(i & 0xff);

        auto root = set.insertHinted( b, hasher.hash( b ).first, td );
        assert( root.isnew() );
        assert( !root.leaf( pool ) );
        assert_eq( root->forkcount( pool ), 2 );
        auto children = root.childvector( pool );

        assert( children[ 0 ].isFork() );
        assert( children[ 1 ].isFork() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].fork()->forkcount( pool ), 2 );
        assert_eq( children[ 1 ].fork()->forkcount( pool ), 2 );

        auto left = children[ 0 ].fork()->childvector( pool );
        auto right = children[ 1 ].fork()->childvector( pool );

        assert( left[ 0 ].isLeaf() );
        assert( left[ 1 ].isLeaf() );
        assert( right[ 0 ].isLeaf() );
        assert( right[ 1 ].isLeaf() );

        assert_eq( left.size(), 2UL );
        assert_eq( right.size(), 2UL );

        assert_eq( left[ 0 ].leaf().size( pool ), 17 );
        assert_eq( left[ 1 ].leaf().size( pool ), 17 );
        assert_eq( right[ 0 ].leaf().size( pool ), 17 );
        assert_eq( right[ 1 ].leaf().size( pool ), 16 );

        for ( unsigned i = 0; i < 67; ++i )
            assert_eq( c2u( ( i < 34 ? left : right )[ (i / 17) % 2 ]
                            .leaf().data( pool )[ i % 17 ] ),
                       i & 0xff );

        Blob b2 = root.reassemble( pool );
        assert( pool.equal( b, b2 ) );

        assert( !set.insertHinted( b, hasher.hash( b ).first, td ).isnew() );

        auto root2 = set.get( b, td );
        assert( !root2.isnew() );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            pool.free( x );
    }

    Test basicFlat() {
        return basic< FakeGeneratorFlat, true >( fgf );
    }

    Test basicBinary() {
        return basic< FakeGeneratorBinary, false >( fgb );
    }

    template< typename Generator, bool leaf  >
    void basic( Generator &fg ) {
        BlobSet set( hasher );
        set._global.generator = &fg;

        Blob b = pool.allocate( 1000 );
        for ( unsigned i = 0; i < 1000; ++i )
            pool.dereference( b )[ i ] = char(i & 0xff);

        auto root = set.insertHinted( b, hasher.hash( b ).first, td );
        assert( root.isnew() );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( pool.dereference( b )[ i ] ), i & 0xff );

        assert_eq( root.leaf( pool ), leaf );
        if ( root.leaf( pool ) ) {
            for ( unsigned i = 0; i < 1000; ++i )
                assert_eq( c2u( root.data( pool )[ i ] ), i & 0xff );
        }

        Blob b2 = root.reassemble( pool );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( pool.dereference( b2 )[ i ] ), i & 0xff );
        assert( pool.equal( b, b2 ) );

        assert( !set.insertHinted( b, hasher.hash( b ).first, td ).isnew() );

        auto root2 = set.get( b, td );
        assert( !root2.isnew() );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            pool.free( x );
    }

    static std::mt19937 random;

    Blob randomBlob( size_t size, Pool &p ) {
        Blob b = p.allocate( size );
        std::for_each( reinterpret_cast< uint32_t * >( p.dereference( b ) ),
            reinterpret_cast< uint32_t * >( p.dereference( b ) + p.size( b ) ),
            []( uint32_t &ptr ) { ptr = TestNTreeHashSet::random(); } );
        return b;
    }

    Blob concatBlob( std::initializer_list< Blob > bs, Pool &p ) {
        size_t size = std::accumulate( bs.begin(), bs.end(), 0,
            [ &p ]( const size_t &acc, const Blob &b ) { return acc + p.size( b ); } );
        return std::get< 0 >( std::accumulate( bs.begin(), bs.end(), std::make_tuple( p.allocate( size ), 0 ),
            [ &p ]( const std::tuple< Blob, size_t > &target, const Blob &b ) {
                std::copy( p.dereference( b ), p.dereference( b ) + p.size( b ),
                    p.dereference( std::get< 0 >( target ) ) + std::get< 1 >( target ) );
                return std::make_tuple( std::get< 0 >( target ), std::get< 1 >( target ) + p.size( b ) );
            } ) );
    }

    template< typename Set >
    size_t count( Set &set, Pool &p ) {
        return std::accumulate( set.m_table.begin(), set.m_table.end(), 0,
            [ &p ]( const size_t &c, const typename Set::Cell &b ) {
                return c + ( p.valid( b.fetch().b ) ? 1 : 0 );
            } );
    }

    Test reuse() {
        BlobSet set( Hasher( pool ) );
        set._global.generator = &fgb; // FIXME

        Blob b1 = randomBlob( 128, pool );
        Blob b2 = randomBlob( 128, pool );
        Blob c = concatBlob( { b1, b2 }, pool );
        assert_neq( size_t( &set._leafs ), size_t( &set._forks ) );
        assert_neq( size_t( &set._leafs ), size_t( &set._roots ) );
        set.insertHinted( b1, pool.hash( b1 ).first, td );
        set.insertHinted( b2, pool.hash( b2 ).first, td );
        size_t leafs = count( set._leafs, pool );
        size_t forks = count( set._forks, pool );

        assert( !set.insertHinted( b1, pool.hash( b1 ).first, td ).isnew() );
        assert_eq( leafs, count( set._leafs, pool ) );
        assert_eq( forks, count( set._forks, pool ) );
        assert_eq( 2ul, count( set._roots, pool ) );

        set.insertHinted( c, pool.hash( c ).first, td );
        assert_eq( leafs, count( set._leafs, pool ) );
        assert_eq( forks + 2, count( set._forks, pool ) );
        assert_eq( 3ul, count( set._roots, pool ) );
    }

    Test incrementalHash() {
        BlobSet set( hasher );
        set._global.generator = &fgb;

        Blob b = randomBlob( 128, pool );
        hash128_t hash = pool.hash( b );
        auto root = set.insertHinted( b, hash.first );
        hash128_t rootHash = set.hasher.hash( root );
        assert_eq( rootHash.first, hash.first );
        assert_eq( rootHash.second, hash.second );
    }
};

std::mt19937 TestNTreeHashSet::random = std::mt19937( 0 );
#endif

struct TestNTreeHashSet {
    void foo();
};
#endif

#ifdef NTREE_STANDALONE_TEST

int main( void ) {
    TestNTreeHashSet test;
    test.basicFlat();
    test.binary2();
    test.binary4();
    test.basicBinary();
    test.reuse();
}

#endif
