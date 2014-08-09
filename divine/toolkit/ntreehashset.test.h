// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2013 Petr Ročkai <me@mornfall.net>

#ifndef TESTS_NTREE_HASH_SET
#define TESTS_NTREE_HASH_SET

#include <divine/toolkit/ntreehashset.h>
#include <divine/toolkit/blob.h>
#include <divine/algorithm/common.h> // hasher
#include <divine/toolkit/pool.h>
#include <bricks/brick-hashset.h>
#include <random>
#include <cstdint>
#include <algorithm>
#include <numeric>

using namespace divine;
using divine::algorithm::Hasher;

struct FakeGeneratorBase {
    Pool _pool;
    Pool& pool() {
        return _pool;
    }
};

struct FakeGeneratorFlat : public FakeGeneratorBase
{
    template< typename Coroutine >
    void splitHint( Coroutine &cor, int = 0 ) {
        cor.consume( pool().size( cor.item ) );
    }
};

struct FakeGeneratorBinary : public FakeGeneratorBase {

    template< typename Coroutine >
    void splitHint( Coroutine &cor, int a = 0, int b = 0 )
    {
        if ( !a )
            a = cor.unconsumed();

        for ( int l : { a, b } ) {
            if ( !l )
                continue;

            if ( l < 32 )
                cor.consume( l );
            else
                cor.recurse( 2, l - l / 2, l / 2 );
        }
    }
};

struct FakeGeneratorFixed : public FakeGeneratorBase {

    std::vector< int > sizes;

    FakeGeneratorFixed( std::initializer_list< int > sz )
        : sizes( sz )
    {
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor )
    {
        std::vector< int > splits;
        int to_split = cor.unconsumed();
        auto it = sizes.begin();

        while ( to_split && it != sizes.end() ) {
            int chunk = std::min( *it++, to_split );
            to_split -= chunk;
            splits.push_back( chunk );
        }

        if ( to_split )
            splits.push_back( to_split );

        if ( splits.size() > 1 )
            cor.split( splits.size() );

        for ( int i : splits )
            cor.consume( i );

        if ( splits.size() > 1 )
            cor.join();
    }
};

template< typename Generator, typename Set >
struct ThreadData : Set::ThreadData {
    Generator *generator;
};

using BlobSet = NTreeHashSet< hashset::Fast, Blob, Hasher >;

template< typename Generator >
struct TestBase {
    Generator _gen;
    using TD = BlobSet::ThreadData< Generator >;
    TD _td;
    BlobSet _set;

    template< typename... Args >
    TestBase( Args &&... args ) : _gen( std::forward< Args >( args )... ),
        _set( Hasher( _gen.pool() ) )
    {
        _td._splitter = &_gen;
        _td._pool = &pool();
    }

    template< typename T >
    TestBase( std::initializer_list< T > il ) : _gen( il ),
        _set( Hasher( _gen.pool() ) )
    {
        _td._splitter = &_gen;
        _td._pool = &pool();
    }

    Pool &pool() {
        return _gen.pool();
    }

    BlobSet &set() {
        return _set;
    }

    auto leaves() -> decltype( ( _set._d.leaves.withTD( _td.leaves ) ) ) {
        return _set._d.leaves.withTD( _td.leaves );
    }

    auto forks() -> decltype( ( _set._d.forks.withTD( _td.forks ) ) ) {
        return _set._d.forks.withTD( _td.forks );
    }

    auto roots() -> decltype( ( _set._d.roots.withTD( _td.roots ) ) ) {
        return _set._d.roots.withTD( _td.roots );
    }

    auto applyTD( BlobSet &bs ) -> decltype( bs.withTD( _td ) ) {
        return bs.withTD( _td );
    }

    auto insertSet( BlobSet &bs, Blob b )
        -> decltype( this->applyTD( bs ).insertHinted( b, 0 ) )
    {
        return applyTD( bs ).insertHinted( b, pool().hash( b ).first );
    }

    auto insert( Blob b ) -> decltype( this->insertSet( _set, b ) ) {
        return insertSet( _set, b );
    }

    auto getSet( BlobSet &bs, Blob b ) -> decltype( this->applyTD( bs ).find( b ) ) {
        return applyTD( bs ).find( b );
    }

    auto get( Blob b ) -> decltype( this->getSet( _set, b ) ) {
        return getSet( _set, b );
    }
};

struct TestNTreeHashSet {

    static unsigned c2u( char c ) {
        return static_cast< unsigned >( static_cast< unsigned char >( c ) );
    }

    template< typename T >
    auto valDeref( T t ) -> typename std::remove_reference< decltype( *t ) >::type {
        return *t;
    }

    Test binary2() {
        TestBase< FakeGeneratorBinary > test;

        // assert_eq( test.set().hasher.slack(), 0 );

        Blob b = test.pool().allocate( 33 );
        for ( unsigned i = 0; i < 33; ++i )
            test.pool().dereference( b )[ i ] = char(i & 0xff);

        auto root = test.insert( b );
        assert( root.isnew() );
        assert( !root->leaf( test.pool() ) );
        assert_eq( size_t( test.pool().size( root->b() ) ),
                   sizeof( BlobSet::Root::Header ) + 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( root->forkcount( test.pool() ), 2 );

        auto children = root->childvector( test.pool() );

        assert( children[ 0 ].isLeaf() );
        assert( children[ 1 ].isLeaf() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].leaf().size( test.pool() ), 17 );
        assert_eq( children[ 1 ].leaf().size( test.pool() ), 16 );

        for ( unsigned i = 0; i < 33; ++i )
            assert_eq( c2u( children[ i / 17 ].leaf().data( test.pool() )[ i % 17 ] ), i & 0xff );

        Blob b2 = root->reassemble( test.pool() );
        assert( test.pool().equal( b, b2 ) );

        auto rootVal = valDeref( root ); // avoid iterator invalidation on insert

        assert( !test.insert( b ).isnew() );

        auto root2 = test.get( b );
        assert( !root2.isnew() );
        assert_eq( rootVal.b().raw(), root2->b().raw() );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    Test binary4() {
        TestBase< FakeGeneratorBinary > test;

        BlobSet set( Hasher( test.pool() ) );

        Blob b = test.pool().allocate( 67 );
        for ( unsigned i = 0; i < 67; ++i )
            test.pool().dereference( b )[ i ] = char(i & 0xff);

        auto root = test.insert( b );
        assert( root.isnew() );
        assert( !root->leaf( test.pool() ) );
        assert_eq( root->forkcount( test.pool() ), 2 );
        auto children = root->childvector( test.pool() );

        assert( children[ 0 ].isFork() );
        assert( children[ 1 ].isFork() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].fork().forkcount( test.pool() ), 2 );
        assert_eq( children[ 1 ].fork().forkcount( test.pool() ), 2 );

        auto left = children[ 0 ].fork().childvector( test.pool() );
        auto right = children[ 1 ].fork().childvector( test.pool() );

        assert( left[ 0 ].isLeaf() );
        assert( left[ 1 ].isLeaf() );
        assert( right[ 0 ].isLeaf() );
        assert( right[ 1 ].isLeaf() );

        assert_eq( left.size(), 2UL );
        assert_eq( right.size(), 2UL );

        assert_eq( left[ 0 ].leaf().size( test.pool() ), 17 );
        assert_eq( left[ 1 ].leaf().size( test.pool() ), 17 );
        assert_eq( right[ 0 ].leaf().size( test.pool() ), 17 );
        assert_eq( right[ 1 ].leaf().size( test.pool() ), 16 );

        for ( unsigned i = 0; i < 67; ++i )
            assert_eq( c2u( ( i < 34 ? left : right )[ (i / 17) % 2 ]
                            .leaf().data( test.pool() )[ i % 17 ] ),
                       i & 0xff );

        Blob b2 = root->reassemble( test.pool() );
        assert( test.pool().equal( b, b2 ) );

        auto rootVal = valDeref( root );

        assert( !test.insert( b ).isnew() );

        auto r = test.get( b );
        assert( !r.isnew() );
        assert_eq( rootVal.b().raw(), r->b().raw() );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    Test basicFlat() {
        return basic< FakeGeneratorFlat, true >();
    }

    Test basicBinary() {
        return basic< FakeGeneratorBinary, false >();
    }

    template< typename Generator, bool leaf  >
    void basic() {
        TestBase< Generator > test;

        BlobSet set( Hasher( test.pool() ) );

        Blob b = test.pool().allocate( 1000 );
        for ( unsigned i = 0; i < 1000; ++i )
            test.pool().dereference( b )[ i ] = char(i & 0xff);

        auto root = test.insert( b );
        assert( root.isnew() );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( test.pool().dereference( b )[ i ] ), i & 0xff );

        assert_eq( root->leaf( test.pool() ), leaf );
        if ( root->leaf( test.pool() ) ) {
            for ( unsigned i = 0; i < 1000; ++i )
                assert_eq( c2u( root->data( test.pool() )[ i ] ), i & 0xff );
        }

        Blob b2 = root->reassemble( test.pool() );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( test.pool().dereference( b2 )[ i ] ), i & 0xff );
        assert( test.pool().equal( b, b2 ) );

        auto rootVal = valDeref( root );

        assert( !test.insert( b ).isnew() );

        auto root2 = test.get( b );
        assert( !root2.isnew() );
        assert_eq( rootVal.b().raw(), root2->b().raw() );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    static std::mt19937 random;

    Blob randomBlob( size_t size, Pool &p ) {
        size_t excess = size & 0x3;
        size &= ~0x3;
        Blob b = p.allocate( size );
        std::for_each( reinterpret_cast< uint32_t * >( p.dereference( b ) ),
            reinterpret_cast< uint32_t * >( p.dereference( b ) + p.size( b ) ),
            []( uint32_t &ptr ) { ptr = TestNTreeHashSet::random(); } );
        if ( excess )
            std::for_each( reinterpret_cast< uint8_t * >( p.dereference( b ) ),
                reinterpret_cast< uint8_t * >( p.dereference( b ) + p.size( b ) ),
                []( uint8_t &ptr ) { ptr = TestNTreeHashSet::random(); } );
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
        return std::accumulate( set._table.begin(), set._table.end(), 0,
            [&]( size_t c, decltype( *set._table.begin() ) &cell ) -> size_t {
                return c + ( p.valid( cell.fetch().unwrap() ) ? 1 : 0 );
            } );
    }

    Test reuse() {
        TestBase< FakeGeneratorBinary > test;

        Blob b1 = randomBlob( 128, test.pool() );
        Blob b2 = randomBlob( 128, test.pool() );
        Blob c = concatBlob( { b1, b2 }, test.pool() );
        assert_neq( size_t( &test.set()._d.leaves ), size_t( &test.set()._d.forks ) );
        assert_neq( size_t( &test.set()._d.leaves ), size_t( &test.set()._d.roots ) );
        test.insert( b1 );
        test.insert( b2 );
        size_t leaves = count( test.leaves(), test.pool() );
        size_t forks = count( test.forks(), test.pool() );

        assert( !test.insert( b1 ).isnew() );
        assert_eq( leaves, count( test.leaves(), test.pool() ) );
        assert_eq( forks, count( test.forks(), test.pool() ) );
        assert_eq( 2ul, count( test.roots(), test.pool() ) );

        test.insert( c );
        assert_eq( leaves, count( test.leaves(), test.pool() ) );
        assert_eq( forks + 2, count( test.forks(), test.pool() ) );
        assert_eq( 3ul, count( test.roots(), test.pool() ) );
    }

    Test incrementalHash() {
        TestBase< FakeGeneratorBinary > test;

        Blob b = randomBlob( 128, test.pool() );
        hash128_t hash = test.pool().hash( b );
        auto root = *test.insert( b );
        hash128_t rootHash = test.set()._d.roots.hasher.hash( root );
        assert_eq( rootHash.first, hash.first );
        assert_eq( rootHash.second, hash.second );
    }

    Test internalReuse() {
        TestBase< FakeGeneratorBinary > test;

        Blob b = randomBlob( 128, test.pool() );
        assert( test.insert( b ).isnew() );
        size_t leaves = count( test.leaves(), test.pool() );
        size_t forks = count( test.forks(), test.pool() );
        assert_eq( 1ul, count( test.roots(), test.pool() ) );

        BlobSet set( Hasher( test.pool() ) );
        Blob composite = concatBlob( { b, b }, test.pool() );
        assert( test.insertSet( set, composite ).isnew() );

        assert_eq( leaves, count( set._d.leaves, test.pool() ) );
        assert_eq( forks + 1, count( set._d.forks, test.pool() ) ); // tree it higher by one
        assert_eq( 1ul, count( set._d.roots, test.pool() ) );
    }

    Test shortLeaves() {
        TestBase< FakeGeneratorFixed > test{ 32, 5, 4, 3, 2, 1 };

        Blob b32 = randomBlob( 32, test.pool() );
        auto r32 = valDeref( test.insert( b32 ) );
        assert( test.pool().equal( b32, r32.reassemble( test.pool() ) ) );

        Blob b37 = randomBlob( 37, test.pool() );
        auto r37 = valDeref( test.insert( b37 ) );
        assert( test.pool().equal( b37, r37.reassemble( test.pool() ) ) );

        Blob b41 = randomBlob( 41, test.pool() );
        auto r41 = valDeref( test.insert( b41 ) );
        assert( test.pool().equal( b41, r41.reassemble( test.pool() ) ) );

        Blob b44 = randomBlob( 44, test.pool() );
        auto r44 = valDeref( test.insert( b44 ) );
        assert( test.pool().equal( b44, r44.reassemble( test.pool() ) ) );

        Blob b46 = randomBlob( 46, test.pool() );
        auto r46 = valDeref( test.insert( b46 ) );
        assert( test.pool().equal( b46, r46.reassemble( test.pool() ) ) );

        Blob b47 = randomBlob( 47, test.pool() );
        auto r47 = valDeref( test.insert( b47 ) );
        assert( test.pool().equal( b47, r47.reassemble( test.pool() ) ) );
    }

};

std::mt19937 TestNTreeHashSet::random = std::mt19937( 0 );

#endif
