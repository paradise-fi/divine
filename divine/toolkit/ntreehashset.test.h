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
    void splitHint( Blob n, intptr_t from, intptr_t length, Yield yield ) {
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
    void splitHint( Blob n, intptr_t from, intptr_t length, Yield yield ) {
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

struct TestNTreeHashSet {
    using BlobSet = NTreeHashSet< HashSet, Blob, Hasher >;

    static unsigned c2u( char c ) {
        return static_cast< unsigned >( static_cast< unsigned char >( c ) );
    }

/*
    template< typename Table >
    void free( Table& t, Pool& pool ) {
        for ( int i = 0; i < t._roots.size(); ++i ) {
            if ( t._roots[ i ] != nullptr )
                t._roots[ i ]->free( pool );
        }
        for ( int i = 0; i < t._forks.size(); ++i ) {
            if ( t._forks[ i ] != nullptr )
                t._forks[ i ]->free( pool );
        }
        for ( int i = 0; i < t._leafs.size(); ++i ) {
            if ( t._leafs[ i ] != nullptr )
                t._leafs[ i ]->free( pool );
        }
    }
*/

    Test binary2() {
        FakeGeneratorBinary fg;
        BlobSet set( Hasher( fg.pool() ) );

        assert_eq( set.hasher.slack, 0 );

        Blob b = fg.pool().allocate( 33 );
        for ( unsigned i = 0; i < 33; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        BlobSet::Root root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
        assert( inserted );
        assert( !root.leaf( fg.pool() ) );
        assert_eq( fg.pool().size( root.b ),
                   sizeof( BlobSet::Root::Header ) + 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( root.forkcount( fg.pool() ), 2 );

        auto children = root.childvector( fg.pool() );

        assert( children[ 0 ].isLeaf() );
        assert( children[ 1 ].isLeaf() );

        assert_eq( children.size(), 2 );

        assert_eq( children[ 0 ].leaf().size( fg.pool() ), 17 );
        assert_eq( children[ 1 ].leaf().size( fg.pool() ), 16 );

        for ( unsigned i = 0; i < 33; ++i )
            assert_eq( c2u( children[ i / 17 ].leaf().data( fg.pool() )[ i % 17 ] ), i & 0xff );

        Blob b2 = root.reassemble( fg.pool() );
        assert( fg.pool().equal( b, b2 ) );

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
        assert( !inserted );

        BlobSet::Root root2;
        bool had;
        std::tie( root2, had ) = set.get( b );
        assert( had );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
        // free( set, fg.pool() );
    }

/*
    void binary4() {
        FakeGeneratorBinary fg;
        BlobSet set( Hasher( fg.pool() ) );

        Blob b = fg.pool().allocate( 67 );
        for ( unsigned i = 0; i < 67; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        BlobSet::Root* root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
        assert( inserted );
        assert( !root->leaf );
        assert_eq( root->selfSize, sizeof( BlobSet::Root ) + 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( root->forksSize(), 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( uintptr_t( root->slack() ), uintptr_t( root ) + root->selfSize );
        assert_eq( root + root->selfSize, root + sizeof( BlobSet::Root ) + root->forksSize() );
        auto childs = root->childs();

        assert( childs[ 0 ].isFork() );
        assert( childs[ 1 ].isFork() );

        assert( !childs[ 0 ].end() );
        assert( childs[ 1 ].end() );

        assert_eq( childs[ 0 ].fork()->size(), 2 * sizeof( BlobSet::LeafOrFork ) );
        assert_eq( childs[ 1 ].fork()->size(), 2 * sizeof( BlobSet::LeafOrFork ) );

        assert( childs[ 0 ].fork()->childs[ 0 ].isLeaf() );
        assert( childs[ 0 ].fork()->childs[ 1 ].isLeaf() );
        assert( childs[ 1 ].fork()->childs[ 0 ].isLeaf() );
        assert( childs[ 1 ].fork()->childs[ 1 ].isLeaf() );

        assert( !childs[ 0 ].fork()->childs[ 0 ].end() );
        assert( childs[ 0 ].fork()->childs[ 1 ].end() );
        assert( !childs[ 1 ].fork()->childs[ 0 ].end() );
        assert( childs[ 1 ].fork()->childs[ 1 ].end() );

        assert_eq( childs[ 0 ].fork()->childs[ 0 ].leaf()->size, 17 );
        assert_eq( childs[ 0 ].fork()->childs[ 1 ].leaf()->size, 17 );
        assert_eq( childs[ 1 ].fork()->childs[ 0 ].leaf()->size, 17 );
        assert_eq( childs[ 1 ].fork()->childs[ 1 ].leaf()->size, 16 );

        for ( unsigned i = 0; i < 67; ++i )
            assert_eq( c2u( childs[ i / 34 ].fork()->childs[ (i / 17) % 2 ]
                        .leaf()->data[ i % 17 ] ), i & 0xff );

        Blob b2 = root->reassemble( fg.pool() );
        assert( fg.pool().equal( b, b2 ) );

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
        assert( !inserted );

        BlobSet::Root* root2;
        bool had;
        std::tie( root2, had ) = set.get( b );
        assert( had );
        assert_eq( root, root2 );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
        free( set, fg.pool() );
    }
*/
    Test basicFlat() {
        return basic< FakeGeneratorFlat, true >();
    }

    Test basicBinary() {
        return basic< FakeGeneratorBinary, false >();
    }

    template< typename Generator, bool leaf  >
    void basic() {
        Generator fg;
        BlobSet set( Hasher( fg.pool() ) );

        Blob b = fg.pool().allocate( 1000 );
        for ( unsigned i = 0; i < 1000; ++i )
            fg.pool().dereference( b )[ i ] = char(i & 0xff);

        typename BlobSet::Root root;
        bool inserted;
        std::tie( root, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
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

        std::tie( std::ignore, inserted ) = set.insertHinted( b, set.hasher.hash( b ), fg );
        assert( !inserted );

        typename BlobSet::Root root2;
        bool had;
        std::tie( root2, had ) = set.get( b );
        assert( had );
        assert_eq( root.b.raw(), root2.b.raw() );

        for ( auto x : { b, b2 } )
            fg.pool().free( x );
        // free( set, fg.pool() );
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
    NTreeHashSetTest test;
    test.basicFlat();
    test.binary2();
    test.binary4();
    test.basicBinary();
}

#endif
