// -*- C++ -*- (c) 2013,2015 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2013 Petr Ročkai <me@mornfall.net>

#include <tuple>
#include <algorithm>
#include <cstring>
#include <queue>
#include <cstdint>
#include <vector>
#include <numeric>

#include <brick-assert.h>
#include <brick-hashset.h>

#include <divine/toolkit/pool.h>

#ifndef N_TREE_HASH_SET_H
#define N_TREE_HASH_SET_H

namespace divine {

namespace types = brick::types;

template< typename T >
struct NewtypeHasher {
    Pool &p;
    NewtypeHasher( Pool &p ) : p( p ) {}
    hash128_t hash( T t ) const { return p.hash( t.unwrap() ); }
    bool valid( T t ) const { return p.valid( t.unwrap() ); }
    bool equal( T a, T b ) const { return p.equal( a.unwrap(), b.unwrap() ); }
};

template< typename T, typename Self >
struct NewTypeTag {
    NewTypeTag() noexcept {}

    static const int tagBits = T::tagBits;
    Self &self() { return *static_cast< Self * >( this ); }
    int tag() { return self().unwrap().tag(); }
    void setTag( int t ) { return self().unwrap().setTag( t ); }
};

// NTreeHashSet :: ( * -> * -> * ) -> * -> * -> *
// Item can be any type with same interface as Blob
template< template< typename, typename > class HashSet, typename T, typename Hasher >
struct NTreeHashSet
{
    struct Root;

    using insert_type = T;
    using value_type = Root;

    typedef NTreeHashSet< HashSet, T, Hasher > This;

    NTreeHashSet() : NTreeHashSet( Hasher() ) { }
    NTreeHashSet( Hasher _hasher ) : _d( _hasher )
    {}

    struct Leaf : types::NewType< T >, NewTypeTag< T, Leaf > {
        Leaf() noexcept {}
        Leaf( T b ) noexcept : types::NewType< T >( b ) {}
        Leaf( int32_t size, char* source, Pool& pool )
        {
            ASSERT_LEQ( 1, size );
            this->unwrap() = pool.allocate( size );
            std::copy( source, source + size, pool.dereference( this->unwrap() ) );
        }
        char *data( Pool &p ) { return p.dereference( this->unwrap() ); }
        int32_t size( Pool &p ) { return p.size( this->unwrap() ); }
        explicit operator bool() { return !!this->unwrap(); }
    };

    template < typename Fork >
    struct LeafOr : types::NewType< T >, NewTypeTag< T, LeafOr< Fork > > {
        LeafOr() noexcept {}

        const T &b() const { return this->unwrap(); }
        T &b() { return this->unwrap(); }

        LeafOr( Fork f ) : types::NewType< T >( f.unwrap() ) {
            ASSERT_EQ( b().tag(), 0UL );
            b().setTag( 1 );
        }

        LeafOr( Leaf l ) : types::NewType< T >( l.unwrap() ) {
            ASSERT_EQ( b().tag(), 0UL );
        }

        T blob() const { T blob( b() ); blob.setTag( 0 ); return blob; }
        bool isLeaf() const { return !isFork(); }
        bool isFork() const { ASSERT( b().tag() == 1 || b().tag() == 0 ); return b().tag(); }
        int32_t size( Pool &p ) const { return p.size( blob() ); }
        bool isNull( Pool &p ) const { return !p.valid( blob() ); }

        Fork fork() {
            ASSERT( isFork() );
            return Fork( blob() );
        }

        Leaf leaf() {
            ASSERT( isLeaf() );
            return Leaf( blob() );
        }
    };

    template< typename Self, typename Fork >
    struct WithChildren
    {
        Self &self() { return *static_cast< Self * >( this ); }

        template< typename Yield >
        void forChildren( Pool &p, int32_t slack, Yield yield ) {
            const int32_t count = self().forkcount( p, slack );
            ASSERT_LEQ( 1, count );
            for ( int32_t i = 0; i < count; ++i )
                yield( self().forkdata( p )[ i ] );
        }

        // emit all leafs until false is returned from yield
        template< typename Yield >
        bool forAllLeaves( Pool& p, int32_t slack, Yield yield ) {
            bool result = false;
            self().forChildren( p, slack, [&]( LeafOrFork lof ) {
                    if ( lof.isLeaf() ) {
                        if ( !yield( lof.leaf() ) )
                            return false;
                    } else {
                        if ( !lof.fork().forAllLeaves( p, slack, yield ) )
                            return false;
                    }
                    return result = true;
                } );
            return result;
        }

        std::vector< LeafOr< Fork > > childvector( Pool &p, int32_t slack ) {
            std::vector< LeafOrFork > lof;
            forChildren( p, slack, [&]( LeafOr< Fork > x ) { lof.push_back( x ); } );
            return lof;
        }
    };

    struct Fork : WithChildren< Fork, Fork >, types::NewType< T >, NewTypeTag< T, Fork > {
        Fork() noexcept {}
        Fork( T b ) noexcept : types::NewType< T >( b ) {}
        Fork( int32_t children, Pool& pool ) {
            ASSERT_LEQ( 2, children );
            int32_t size = children * sizeof( LeafOr< Fork > );
            this->unwrap() = pool.allocate( size );
            pool.clear( this->unwrap() );
        }

        int32_t forkcount( Pool &p, int32_t = 0 ) {
            return p.size( this->unwrap() ) / sizeof( LeafOr< Fork > );
        }
        LeafOr< Fork > *forkdata( Pool &p ) {
            return p.dereference< LeafOrFork >( this->unwrap() );
        }
        explicit operator bool() { return !!this->unwrap(); }
    };

    typedef LeafOr< Fork > LeafOrFork;

    struct Root : WithChildren< Root, Fork >, types::NewType< T >, NewTypeTag< T, Root > {
        T &b() { return this->unwrap(); }

        Root() noexcept {}
        explicit Root( T b ) noexcept : types::NewType< T >( b ) {}
        int32_t forkcount( Pool &p, int32_t slack ) {
            ASSERT_EQ( 0, dataSize( p, slack ) % int( sizeof( LeafOrFork ) ) );
            return dataSize( p, slack ) / sizeof( LeafOrFork );
        }
        char *rawdata( Pool &p ) { return p.dereference( b() ); }

        explicit operator bool() { return !!this->unwrap(); }

        LeafOrFork *forkdata( Pool &p ) {
            return reinterpret_cast< LeafOrFork *> ( rawdata( p ) );
        }

        int32_t dataSize( Pool &p, int32_t slack ) { return p.size( b() ) - slack; }
        int32_t slackoffset( Pool &p, int32_t slack ) { return dataSize( p, slack ); }

        char *slack( Pool &p, int32_t slack ) {
            return rawdata( p ) + slackoffset( p, slack );
        }

        template< typename Alloc >
        T reassemble( Alloc alloc, Pool& p, int32_t slacksize )
        {
            ASSERT( p.valid( b() ) );

            std::vector< Leaf > leaves;
            int32_t size = 0;

            this->forAllLeaves( p, slacksize, [&]( Leaf leaf ) -> bool {
                    leaves.push_back( leaf );
                    size += leaf.size( p );
                    return true; // demand all
                } );

            char* slackptr = slack( p, slacksize );
            size += slacksize;
            T out = alloc.get( p, size );
            char* outptr = p.dereference( out );
            outptr = std::copy( slackptr, slackptr + slacksize, outptr );
            for ( auto l : leaves ) {
                ASSERT_LEQ( outptr - p.dereference( out ), p.size( out ) );
                outptr = std::copy( l.data( p ), l.data( p ) + p.size( l.unwrap() ), outptr );
            }
            ASSERT_EQ( outptr - p.dereference( out ), size );
            return out;
        }

        static Root create( insert_type it, int32_t children, int32_t slack, Pool& pool ) {
            ASSERT_LEQ( 1, children );
            ASSERT_LEQ( 0, slack );
            const uintptr_t size = slack + sizeof( LeafOrFork ) * children;
            Root root;
            root.unwrap() = pool.allocate( size );
            std::memset( root.rawdata( pool ), 0, sizeof( LeafOrFork ) * children );
            std::memcpy( pool.dereference( root.unwrap() ) + size - slack,
                         pool.dereference( it ), slack );
            return root;
        }
    };

    struct Uncompressed { insert_type i; Uncompressed( insert_type i ) : i( i ) {} };

    using ForkHasher = NewtypeHasher< Fork >;
    using LeafHasher = NewtypeHasher< Leaf >;

    struct RootHasher {
        Hasher uhasher;
        uint32_t salt;

        RootHasher( Hasher uhasher ) : RootHasher( uhasher, 0 ) { }
        RootHasher( Hasher uhasher, uint32_t salt )
            : uhasher( uhasher), salt( salt )
        { }

        Pool &pool() { return uhasher.pool(); }
        int32_t slack() { return uhasher.slack; }

        hash128_t hash( Uncompressed u ) { return uhasher.hash( u.i ); }
        hash128_t hash( Root r ) {
            brick::hash::jenkins::SpookyState state( salt, salt );
            r.forAllLeaves( pool(), slack(), [&]( Leaf l ) {
                        state.update( l.data( this->pool() ), l.size( this->pool() ) );
                        return true;
                    } );
            return state.finalize();
        }

        bool valid( Root r ) { return pool().valid( r.unwrap() ); }
        bool valid( Uncompressed r ) { return pool().valid( r.i ); }
        void setSeed( hash64_t s ) { uhasher.setSeed( s ); }

        bool equal( Root r1, Root r2 ) {
            if ( r1.unwrap().raw() == r2.unwrap().raw() )
                return true;

            int32_t s1 = pool().size( r1.unwrap() );
            int32_t s2 = pool().size( r2.unwrap() );
            return s1 == s2 && pool().equal( r1.unwrap(), r2.unwrap(), 0, s1 - slack() );

            /*
             * Note on root equality: Leaves and forks are cononical, that is
             * only one instance of leaf/fork representing same subnode exists
             * per HashSet, and instances from diffent HashSets can't represent
             * same nodes (as they differ in hash).  Therefore roots can be
             * compared trivially (on pointers to leafs/forks.
             */
        }

        bool equal( Root root, Uncompressed item ) {
            return equal( item, root );
        }

        bool equal( Uncompressed item, Root root ) {
            char* itemPtr = pool().dereference( item.i ) + slack();
            ASSERT( itemPtr != nullptr );

            int32_t pos = slack();
            bool equal = true;

            root.forAllLeaves( pool(), slack(), [&]( Leaf leaf ) {
                    Pool &p = this->pool();
                    const int32_t ls = leaf.size( p );

                    ASSERT( equal );
                    ASSERT( this->pool().valid( leaf.unwrap() ) );
                    ASSERT_LEQ( 0, pos );

                    ASSERT_LEQ( pos + ls, this->pool().size( item.i ) );
                    equal = std::memcmp( itemPtr, leaf.data( p ), ls ) == 0;
                    itemPtr += ls;
                    pos += ls;
                    return equal;
                } );
            ASSERT_LEQ( 0, pos );
            return equal;
        }

        bool equal( Uncompressed i1, Uncompressed i2 ) const {
            return uhasher.equal( i1.i, i2.i );
        }
    };

    using RootSet = HashSet< Root, RootHasher >;
    using ForkSet = HashSet< Fork, ForkHasher >;
    using LeafSet = HashSet< Leaf, LeafHasher >;

    using iterator = typename RootSet::iterator;

    template< typename Splitter >
    struct ThreadData {
        typename RootSet::ThreadData roots;
        typename ForkSet::ThreadData forks;
        typename LeafSet::ThreadData leaves;
        Splitter *_splitter;
        Pool *_pool;
        Splitter &splitter() { ASSERT( _splitter ); return *_splitter; }
        Pool &pool() { ASSERT( _pool ); return *_pool; }

        ThreadData() : _splitter( nullptr ), _pool( nullptr ) {}
        ThreadData( Pool &p, Splitter &sp )
            : _splitter( &sp ), _pool( &p )
        {}
    };

    struct Data
    {
        Data( Hasher h ) :
            pool( h.pool() ), // FIXME
            slack( h.slack ),
            roots( RootHasher( h ) ),
            forks( ForkHasher( h.pool() ) ),
            leaves( LeafHasher( h.pool() ) )
        {}

        Pool &pool;
        int slack; // FIXME
        RootSet roots; // stores slack and roots of tree
                        // (also in case when root leaf)
        ForkSet forks; // stores intermediate nodes
        LeafSet leaves; // stores leaves if they are not root
    };

    /* struct NoSplitter
    {
        template< typename Yield >
        void splitHint( T, intptr_t, intptr_t length, Yield yield ) {
            yield( Recurse::No, length, 0 );
        }

        template< typename Yield >
        void splitHint( T n, Yield yield ) {
            splitHint( n, 0, pool().size( n ), yield );
        }
    }; */

    Pool& pool() { return _d.pool; }
    int32_t slack() { return _d.slack; }
    char* slackPtr( insert_type item ) { return pool().dereference( item ); }

    void copySlack( insert_type slackSource, insert_type root ) {
        pool().copy( slackSource, root, slack() );
    }

    size_t size() { return _d.roots.size(); }
    bool empty() { return _d.roots.empty(); }
    hash64_t hash( const value_type &t ) { return hash128( t ).first; }
    hash128_t hash128( const value_type &t ) { return _d.roots.hash128( t ); }

    Data _d;
    // ThreadData< NoSplitter > _global; /* for single-thread access */

    template< typename Splitter >
    struct WithTD
    {
        using iterator = typename RootSet::iterator;
        using insert_type = T;
        using value_type = Root;

        Data &_d;
        ThreadData< Splitter > &_td;

        WithTD( Data &d, ThreadData< Splitter > &td )
            : _d( d ), _td( td )
        {}

        struct SplitCoroutine {
            Data &d;
            ThreadData< Splitter > &td;
            insert_type item;
            Root root;
            std::vector< std::pair< Fork, int > > stack;
            char *start, *current;
            int size;

            Fork &fork() { ASSERT( stack.size() > 1 ); return stack.back().first;  }

            int forkcount() {
                ASSERT( !stack.empty() );
                return stack.size() == 1 ?
                       root.forkcount( td.pool(), d.slack ) :
                       fork().forkcount( td.pool() );
            }

            LeafOrFork *forkdata() {
                ASSERT( !stack.empty() );
                return stack.size() == 1 ?
                       root.forkdata( td.pool() ) :
                       fork().forkdata( td.pool() );
            }

            LeafOrFork &target() { return *(forkdata() + stack.back().second); }

            SplitCoroutine( Data &d, ThreadData< Splitter > &td, insert_type item )
                : d( d ), td( td ), item( item ),
                  start( td.pool().dereference( item ) + d.slack ),
                  current( start ), size( td.pool().size( item ) - d.slack )
            {}

            int unconsumed() {
                return start + size - current;
            }

            template< typename Table, typename I >
            LeafOrFork insert( Table &table, typename Table::ThreadData &table_td, I i ) {
                auto c = table.withTD( table_td ).insert( i );
                if ( !c.isnew() )
                    td.pool().free( i.unwrap() );
                return c.copy();
            }

            void split( int subdivides ) {
                if ( stack.empty() ) { /* no root yet, create it */
                    ASSERT( !td.pool().valid( root.unwrap() ) );
                    root = Root::create( item, subdivides, d.slack, td.pool() );
                    stack.emplace_back( Fork(), 0 );
                } else
                    stack.emplace_back( Fork( subdivides, td.pool() ), 0 );
            }

            void advance() { ASSERT( !stack.empty() ); stack.back().second ++; }

            void join() {
                auto f = stack.back().first;
                ASSERT_EQ( forkcount(), stack.back().second );
                stack.pop_back();
                if ( !stack.empty() ) {
                    ASSERT_LEQ( &target(), forkdata() + forkcount() );
                    target() = insert( d.forks, td.forks, f );
                    advance();
                }
                ASSERT( td.pool().valid( root.unwrap() ) );
            }

            template< typename... R >
            void recurse( int subdivides, R... r )
            {
                split( subdivides );
                td.splitter().splitHint( *this, r... );
                join();
            }

            void consume( intptr_t length ) {
                ASSERT_LEQ( start, current );
                ASSERT_LEQ( current, start + size );
                ASSERT_LEQ( current + length, start + size );

                if ( stack.empty() ) {
                    ASSERT( !td.pool().valid( root.unwrap() ) );
                    ASSERT_EQ( td.pool().size( item ) - d.slack, length );
                    ASSERT_EQ( current, td.pool().dereference( item ) + d.slack );

                    root = Root::create( item, 1, d.slack, td.pool() );
                    root.forkdata( td.pool() )[ 0 ] =
                            insert( d.leaves, td.leaves, Leaf( length, current, td.pool() ) );
                } else {
                    target() = insert( d.leaves, td.leaves,
                                          Leaf( length, current, td.pool() ) );
                    advance();
                }
                current += length;
            }
        };

        iterator insertHinted( insert_type item, hash64_t hash )
        {
            SplitCoroutine cor( _d, _td, item );
            _td.splitter().splitHint( cor );

            ASSERT_EQ( static_cast< void * >( cor.current ),
                       static_cast< void * >( cor.start + cor.size ) );

            ASSERT( cor.stack.empty() );
            auto tr = _d.roots.withTD( _td.roots ).insertHinted( cor.root, hash );
            if ( !tr.isnew() )
                _td.pool().free( cor.root.unwrap() );

            return tr;
        }

        iterator find( insert_type item ) {
            return findHinted( item, _d.roots.hasher.hash( item ).first );
        }

        iterator findHinted( insert_type i, hash64_t h ) {
            return _d.roots.withTD( _td.roots ).findHinted( Uncompressed( i ), h );
        }

        bool has( insert_type i ) {
            return get( i ).valid();
        }
    };

    template< typename Splitter >
    WithTD< Splitter > withTD( ThreadData< Splitter > &td ) {
        return WithTD< Splitter >( _d, td );
    }

    void setSize( size_t s ) {
        _d.roots.setSize( s );
        _d.forks.setSize( s );
        _d.leaves.setSize( s );
    }

    void clear() {
        _d.roots.clear();
        _d.forks.clear();
        _d.leaves.clear();
    }

    Root operator[]( intptr_t off ) {
        ASSERT_LEQ( 0, off );
        ASSERT_LEQ( off, intptr_t( size() ) - 1 );
        return _d.roots[ off ];
    }

    bool valid( intptr_t off ) {
        return _d.roots.valid( off );
    }

    /* iterator insertHinted( value_type item, hash64_t hash ) {
        return withTD( _global ).insertHinted( item, hash );
     } */
};

}

#include <random>
#include <divine/algorithm/common.h>

namespace divine_test {

using namespace brick;
using namespace divine;
using divine::algorithm::Hasher;
namespace hashset = brick::hashset;

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
        cor.consume( cor.size );
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

template< typename Generator, int32_t slack = 0 >
struct TestBase {
    Generator _gen;
    using TD = BlobSet::ThreadData< Generator >;
    TD _td;
    BlobSet _set;

    template< typename... Args >
    TestBase( Args &&... args ) : _gen( std::forward< Args >( args )... ),
        _set( Hasher( _gen.pool(), slack ) )
    {
        _td._splitter = &_gen;
        _td._pool = &pool();
    }

    template< typename T >
    TestBase( std::initializer_list< T > il ) : _gen( il ),
        _set( Hasher( _gen.pool(), slack ) )
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
        return applyTD( bs ).insertHinted( b, bs._d.roots.hasher.hash( b ).first );
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

    TEST(binary2) {
        TestBase< FakeGeneratorBinary > test;

        // ASSERT_EQ( test.set().hasher.slack(), 0 );

        Blob b = test.pool().allocate( 33 );
        for ( unsigned i = 0; i < 33; ++i )
            test.pool().dereference( b )[ i ] = char(i & 0xff);

        auto root = test.insert( b );
        ASSERT( root.isnew() );
        ASSERT_EQ( size_t( test.pool().size( root->b() ) ), 2 * sizeof( BlobSet::LeafOrFork ) );
        ASSERT_EQ( root->forkcount( test.pool(), 0 ), 2 );

        auto children = root->childvector( test.pool(), 0 );

        ASSERT( children[ 0 ].isLeaf() );
        ASSERT( children[ 1 ].isLeaf() );

        ASSERT_EQ( children.size(), 2UL );

        ASSERT_EQ( children[ 0 ].leaf().size( test.pool() ), 17 );
        ASSERT_EQ( children[ 1 ].leaf().size( test.pool() ), 16 );

        for ( unsigned i = 0; i < 33; ++i )
            ASSERT_EQ( c2u( children[ i / 17 ].leaf().data( test.pool() )[ i % 17 ] ), i & 0xff );

        Blob b2 = root->reassemble( LongTerm(), test.pool(), 0 );
        ASSERT( test.pool().equal( b, b2 ) );

        auto rootVal UNUSED = valDeref( root ); // avoid iterator invalidation on insert

        ASSERT( !test.insert( b ).isnew() );

        auto root2 UNUSED = test.get( b );
        ASSERT( !root2.isnew() );
        ASSERT_EQ( rootVal.b().raw(), root2->b().raw() );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    TEST(binary4) {
        TestBase< FakeGeneratorBinary > test;

        BlobSet set( Hasher( test.pool() ) );

        Blob b = test.pool().allocate( 67 );
        for ( unsigned i = 0; i < 67; ++i )
            test.pool().dereference( b )[ i ] = char(i & 0xff);

        auto root = test.insert( b );
        ASSERT( root.isnew() );
        ASSERT_EQ( root->forkcount( test.pool(), 0 ), 2 );
        auto children = root->childvector( test.pool(), 0 );

        ASSERT( children[ 0 ].isFork() );
        ASSERT( children[ 1 ].isFork() );

        ASSERT_EQ( children.size(), 2UL );

        ASSERT_EQ( children[ 0 ].fork().forkcount( test.pool() ), 2 );
        ASSERT_EQ( children[ 1 ].fork().forkcount( test.pool() ), 2 );

        auto left = children[ 0 ].fork().childvector( test.pool(), 0 );
        auto right = children[ 1 ].fork().childvector( test.pool(), 0 );

        ASSERT( left[ 0 ].isLeaf() );
        ASSERT( left[ 1 ].isLeaf() );
        ASSERT( right[ 0 ].isLeaf() );
        ASSERT( right[ 1 ].isLeaf() );

        ASSERT_EQ( left.size(), 2UL );
        ASSERT_EQ( right.size(), 2UL );

        ASSERT_EQ( left[ 0 ].leaf().size( test.pool() ), 17 );
        ASSERT_EQ( left[ 1 ].leaf().size( test.pool() ), 17 );
        ASSERT_EQ( right[ 0 ].leaf().size( test.pool() ), 17 );
        ASSERT_EQ( right[ 1 ].leaf().size( test.pool() ), 16 );

        for ( unsigned i = 0; i < 67; ++i )
            ASSERT_EQ( c2u( ( i < 34 ? left : right )[ (i / 17) % 2 ]
                            .leaf().data( test.pool() )[ i % 17 ] ),
                       i & 0xff );

        Blob b2 = root->reassemble( LongTerm(), test.pool(), 0 );
        ASSERT( test.pool().equal( b, b2 ) );

        auto rootVal UNUSED = valDeref( root );

        ASSERT( !test.insert( b ).isnew() );

        auto r UNUSED = test.get( b );
        ASSERT( !r.isnew() );
        ASSERT_EQ( rootVal.b().raw(), r->b().raw() );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    TEST(basicFlat) {
        return basic< FakeGeneratorFlat, true >();
    }

    TEST(basicBinary) {
        return basic< FakeGeneratorBinary, false >();
    }

    TEST(basicWSlackFlat) {
        return basic< FakeGeneratorFlat, true, 0xff00aa33 >();
    }

    TEST(basicWSlackBinary) {
        return basic< FakeGeneratorBinary, false, 0xff00aa33 >();
    }

    template< typename Generator, bool leaf, uint32_t slackpat = 0,
        int32_t slack = slackpat ? sizeof( uint32_t ) : 0 >
    void basic() {
        TestBase< Generator, slack > test;
        auto derefslack UNUSED = [&]( BlobSet::Root r ) {
            return reinterpret_cast< uint32_t * >( r.slack( test.pool(), slack ) )[ 0 ];
        };

        ASSERT_EQ( test._set.slack(), slack );

        Blob b = test.pool().allocate( 1000 + slack );
        reinterpret_cast< uint32_t * >( test.pool().dereference( b ) )[ 0 ] = slackpat;
        for ( unsigned i = 0; i < 1000; ++i )
            test.pool().dereference( b )[ slack + i ] = char(i & 0xff);

        auto root = test.insert( b );
        ASSERT( root.isnew() );
        for ( unsigned i = 0; i < 1000; ++i )
            ASSERT_EQ( c2u( test.pool().dereference( b )[ slack + i ] ), i & 0xff );

        ASSERT( !leaf || root->forkcount( test.pool(), slack ) == 1 );
        if ( slack )
            ASSERT_EQ( derefslack( *root ), slackpat );

        Blob b2 = root->reassemble( LongTerm(), test.pool(), slack );
        for ( unsigned i = 0; i < 1000; ++i )
            ASSERT_EQ( c2u( test.pool().dereference( b2 )[ slack + i ] ), i & 0xff );
        ASSERT_EQ( test.pool().size( b ), test.pool().size( b2 ) );
        ASSERT( test.pool().equal( b, b2 ) );

        auto rootVal UNUSED = valDeref( root );

        ASSERT( !test.insert( b ).isnew() );

        auto root2 UNUSED = test.get( b );
        ASSERT( test.pool().valid( root2->b() ) );
        ASSERT( !root2.isnew() );
        ASSERT_EQ( rootVal.b().raw(), root2->b().raw() );
        if ( slack )
            ASSERT_EQ( derefslack( *root2 ), slackpat );

        for ( auto x : { b, b2 } )
            test.pool().free( x );
    }

    std::mt19937 random;

    Blob randomBlob( size_t size, Pool &p ) {
        size_t excess = size & 0x3;
        size &= ~0x3;
        Blob b = p.allocate( size );
        std::for_each( reinterpret_cast< uint32_t * >( p.dereference( b ) ),
            reinterpret_cast< uint32_t * >( p.dereference( b ) + p.size( b ) ),
            [this]( uint32_t &ptr ) { ptr = this->random(); } );
        if ( excess )
            std::for_each( reinterpret_cast< uint8_t * >( p.dereference( b ) ),
                reinterpret_cast< uint8_t * >( p.dereference( b ) + p.size( b ) ),
                [this]( uint8_t &ptr ) { ptr = this->random(); } );
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

    TEST(reuse) {
        TestBase< FakeGeneratorBinary > test;

        Blob b1 = randomBlob( 128, test.pool() );
        Blob b2 = randomBlob( 128, test.pool() );
        Blob c = concatBlob( { b1, b2 }, test.pool() );
        ASSERT_NEQ( size_t( &test.set()._d.leaves ), size_t( &test.set()._d.forks ) );
        ASSERT_NEQ( size_t( &test.set()._d.leaves ), size_t( &test.set()._d.roots ) );
        test.insert( b1 );
        test.insert( b2 );
        size_t leaves UNUSED = count( test.leaves(), test.pool() );
        size_t forks UNUSED = count( test.forks(), test.pool() );

        ASSERT( !test.insert( b1 ).isnew() );
        ASSERT_EQ( leaves, count( test.leaves(), test.pool() ) );
        ASSERT_EQ( forks, count( test.forks(), test.pool() ) );
        ASSERT_EQ( 2ul, count( test.roots(), test.pool() ) );

        test.insert( c );
        ASSERT_EQ( leaves, count( test.leaves(), test.pool() ) );
        ASSERT_EQ( forks + 2, count( test.forks(), test.pool() ) );
        ASSERT_EQ( 3ul, count( test.roots(), test.pool() ) );
    }

    TEST(incrementalHash) {
        TestBase< FakeGeneratorBinary > test;

        Blob b = randomBlob( 128, test.pool() );
        hash128_t hash UNUSED = test.pool().hash( b );
        auto root UNUSED = *test.insert( b );
        hash128_t rootHash UNUSED = test.set()._d.roots.hasher.hash( root );
        ASSERT_EQ( rootHash.first, hash.first );
        ASSERT_EQ( rootHash.second, hash.second );
    }

    TEST(internalReuse) {
        TestBase< FakeGeneratorBinary > test;

        Blob b = randomBlob( 128, test.pool() );
        ASSERT( test.insert( b ).isnew() );
        size_t leaves UNUSED = count( test.leaves(), test.pool() );
        size_t forks UNUSED = count( test.forks(), test.pool() );
        ASSERT_EQ( 1ul, count( test.roots(), test.pool() ) );

        BlobSet set( Hasher( test.pool() ) );
        Blob composite UNUSED = concatBlob( { b, b }, test.pool() );
        ASSERT( test.insertSet( set, composite ).isnew() );

        ASSERT_EQ( leaves, count( set._d.leaves, test.pool() ) );
        ASSERT_EQ( forks + 1, count( set._d.forks, test.pool() ) ); // tree it higher by one
        ASSERT_EQ( 1ul, count( set._d.roots, test.pool() ) );
    }

    TEST(shortLeaves) {
        TestBase< FakeGeneratorFixed > test{ 32, 5, 4, 3, 2, 1 };

        Blob b32 = randomBlob( 32, test.pool() );
        auto r32 UNUSED = valDeref( test.insert( b32 ) );
        ASSERT( test.pool().equal( b32, r32.reassemble( LongTerm(), test.pool(), 0 ) ) );

        Blob b37 = randomBlob( 37, test.pool() );
        auto r37 UNUSED = valDeref( test.insert( b37 ) );
        ASSERT( test.pool().equal( b37, r37.reassemble( LongTerm(), test.pool(), 0 ) ) );

        Blob b41 = randomBlob( 41, test.pool() );
        auto r41 UNUSED = valDeref( test.insert( b41 ) );
        ASSERT( test.pool().equal( b41, r41.reassemble( LongTerm(), test.pool(), 0 ) ) );

        Blob b44 = randomBlob( 44, test.pool() );
        auto r44 UNUSED = valDeref( test.insert( b44 ) );
        ASSERT( test.pool().equal( b44, r44.reassemble( LongTerm(), test.pool(), 0 ) ) );

        Blob b46 = randomBlob( 46, test.pool() );
        auto r46 UNUSED = valDeref( test.insert( b46 ) );
        ASSERT( test.pool().equal( b46, r46.reassemble( LongTerm(), test.pool(), 0 ) ) );

        Blob b47 = randomBlob( 47, test.pool() );
        auto r47 UNUSED = valDeref( test.insert( b47 ) );
        ASSERT( test.pool().equal( b47, r47.reassemble( LongTerm(), test.pool(), 0 ) ) );
    }

};

}

#endif
