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

using namespace divine;
using divine::algorithm::Hasher;

struct FakeGeneratorFlat {
    Pool _pool;
    Pool& pool() {
        return _pool;
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

    Pool _pool;
    Pool& pool() {
        return _pool;
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

template< typename Generator, typename Set >
struct ThreadData : Set::ThreadData {
    Generator *generator;
};

struct TestNTreeHashSet {
    using BlobSet = NTreeHashSet< HashSet, Blob, Hasher >;

    static unsigned c2u( char c ) {
        return static_cast< unsigned >( static_cast< unsigned char >( c ) );
    }

    Test binary2() {
        FakeGeneratorBinary fg;
        ThreadData< FakeGeneratorBinary, BlobSet > td;
        td.generator = &fg;
        BlobSet set( Hasher( fg.pool() ) );

        assert_eq( set.hasher.slack(), 0 );

        Blob b = fg.pool().allocate( 33 );
        for ( unsigned i = 0; i < 33; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        BlobSet::Root root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( inserted );
        assert( !root.leaf( fg.pool() ) );
        assert_eq( size_t( fg.pool().size( root.b ) ),
                   sizeof( BlobSet::Root::Header ) + 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( root.forkcount( fg.pool() ), 2 );

        auto children = root.childvector( fg.pool() );

        assert( children[ 0 ].isLeaf() );
        assert( children[ 1 ].isLeaf() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].leaf().size( fg.pool() ), 17 );
        assert_eq( children[ 1 ].leaf().size( fg.pool() ), 16 );

        for ( unsigned i = 0; i < 33; ++i )
            assert_eq( c2u( children[ i / 17 ].leaf().data( fg.pool() )[ i % 17 ] ), i & 0xff );

        Blob b2 = root.reassemble( fg.pool() );
        assert( fg.pool().equal( b, b2 ) );

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( !inserted );

        BlobSet::Root root2;
        bool had;
        std::tie( root2, had ) = set.get( b, td );
        assert( had );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
    }

    Test binary4() {
        FakeGeneratorBinary fg;
        ThreadData< FakeGeneratorBinary, BlobSet > td;
        td.generator = &fg;

        BlobSet set( Hasher( fg.pool() ) );

        Blob b = fg.pool().allocate( 67 );
        for ( unsigned i = 0; i < 67; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        BlobSet::Root root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( inserted );
        assert( !root.leaf( fg.pool() ) );
        assert_eq( root.forkcount( fg.pool() ), 2 );
        auto children = root.childvector( fg.pool() );

        assert( children[ 0 ].isFork() );
        assert( children[ 1 ].isFork() );

        assert_eq( children.size(), 2UL );

        assert_eq( children[ 0 ].fork().forkcount( fg.pool() ), 2 );
        assert_eq( children[ 1 ].fork().forkcount( fg.pool() ), 2 );

        auto left = children[ 0 ].fork().childvector( fg.pool() );
        auto right = children[ 1 ].fork().childvector( fg.pool() );

        assert( left[ 0 ].isLeaf() );
        assert( left[ 1 ].isLeaf() );
        assert( right[ 0 ].isLeaf() );
        assert( right[ 1 ].isLeaf() );

        assert_eq( left.size(), 2UL );
        assert_eq( right.size(), 2UL );

        assert_eq( left[ 0 ].leaf().size( fg.pool() ), 17 );
        assert_eq( left[ 1 ].leaf().size( fg.pool() ), 17 );
        assert_eq( right[ 0 ].leaf().size( fg.pool() ), 17 );
        assert_eq( right[ 1 ].leaf().size( fg.pool() ), 16 );

        for ( unsigned i = 0; i < 67; ++i )
            assert_eq( c2u( ( i < 34 ? left : right )[ (i / 17) % 2 ]
                            .leaf().data( fg.pool() )[ i % 17 ] ),
                       i & 0xff );

        Blob b2 = root.reassemble( fg.pool() );
        assert( fg.pool().equal( b, b2 ) );

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( !inserted );

        BlobSet::Root root2;
        bool had;
        std::tie( root2, had ) = set.get( b, td );
        assert( had );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
    }

    Test basicFlat() {
        return basic< FakeGeneratorFlat, true >();
    }

    Test basicBinary() {
        return basic< FakeGeneratorBinary, false >();
    }

    template< typename Generator, bool leaf  >
    void basic() {
        Generator fg;
        ThreadData< Generator, BlobSet > td;
        td.generator = &fg;

        BlobSet set( Hasher( fg.pool() ) );

        Blob b = fg.pool().allocate( 1000 );
        for ( unsigned i = 0; i < 1000; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        typename BlobSet::Root root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( inserted );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( fg.pool().dereference( b )[ i ] ), i & 0xff );

        assert_eq( root.leaf( fg.pool() ), leaf );
        if ( root.leaf( fg.pool() ) ) {
            for ( unsigned i = 0; i < 1000; ++i )
                assert_eq( c2u( root.data( fg.pool() )[ i ] ), i & 0xff );
        }

        Blob b2 = root.reassemble( fg.pool() );
        for ( unsigned i = 0; i < 1000; ++i )
            assert_eq( c2u( fg.pool().dereference( b2 )[ i ] ), i & 0xff );
        assert( fg.pool().equal( b, b2 ) );

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), td );
        assert( !inserted );

        typename BlobSet::Root root2;
        bool had;
        std::tie( root2, had ) = set.get( b, td );
        assert( had );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
    }

    template< typename Generator, bool leaf >
    void reuse() {
        Generator fg;
        BlobSet set( Hasher( fg.pool() ) );


    }
};

#endif

#ifdef NTREE_STANDALONE_TEST

int main( void ) {
    TestNTreeHashSet test;
    test.basicFlat();
    test.binary2();
    test.binary4();
    test.basicBinary();
}

#endif
