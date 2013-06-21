// -*- C++ -*- (c) 2013 Vladimir Still <xstill@fi.muni.cz>
//             (c) 2013 Petr Ročkai <me@mornfall.net>

#include <wibble/test.h> // assert
#include <tuple>
#include <algorithm>
#include <cstring>
#include <queue>
#include <cstdint>
#include <vector>
#include <numeric>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>

#ifndef N_TREE_HASH_SET_H
#define N_TREE_HASH_SET_H

namespace divine {

enum class Recurse : uint8_t { Yes, No };

template< typename T >
struct NewtypeHasher {
    Pool &p;
    NewtypeHasher( Pool &p ) : p( p ) {}
    hash128_t hash( T t ) const { return p.hash( t.b ); }
    bool valid( T t ) const { return p.valid( t.b ); }
    bool equal( T a, T b ) const { return p.equal( a.b, b.b ); }
};

// NTreeHashSet :: ( * -> * -> * ) -> * -> * -> *
// Item can be any type with same interface as Blob
template< template< typename, typename > class HashSet,
          typename _Item, typename _Hasher >
struct NTreeHashSet
{
    typedef _Item Item;
    typedef _Hasher UncompressedHasher;
    typedef NTreeHashSet< HashSet, _Item, _Hasher > This;

    using InputHasher = UncompressedHasher;

    NTreeHashSet() : NTreeHashSet( Hasher() ) { }

    NTreeHashSet( UncompressedHasher _hasher ) :
    hasher( _hasher ),
    _roots( hasher ),
    _forks( ForkHasher( hasher.pool() ) ),
    _leafs( LeafHasher( hasher.pool() ) )
    { }

    struct Leaf {
        using IsBlobNewtype = void;
        Blob b;

        Leaf() = default;
        Leaf( Blob b ) : b( b ) {}
        Leaf( int32_t size, char* source, Pool& pool )
        {
            assert_leq( 1, size );
            b = pool.allocate( size );
            std::copy( source, source + size, pool.dereference( b ) );
        }
        char *data( Pool &p ) { return p.dereference( b ); }
        int32_t size( Pool &p ) { return p.size( b ); }
    };

    template < typename Fork >
    class LeafOr {
        uint64_t ptr;
        static const uint64_t unionbit = 1ULL << 63;
    public:
        LeafOr() : ptr( 0 ) { }

        LeafOr( Fork f ) : ptr( f.b.raw() | unionbit )
        {
            assert_eq( f.b.raw() & unionbit, 0UL );
        }

        LeafOr( Leaf l ) : ptr( l.b.raw() )
        {
            assert_eq( l.b.raw() & unionbit, 0UL );
        }

        Blob blob() const { return Blob::fromRaw( ptr & ~unionbit ); }
        bool isLeaf() const { return !isFork(); }
        bool isFork() const { return ptr & unionbit; }
        int32_t size( Pool &p ) const { return p.size( blob() ); }
        bool isNull( Pool &p ) const { return !p.valid( blob() ); }

        Fork fork() {
            assert( isFork() );
            return Fork( blob() );
        }

        Leaf leaf() {
            assert( isLeaf() );
            return Leaf( blob() );
        }
    };

    template< typename Self, typename Fork >
    struct WithChildren
    {
        Self &self() { return *static_cast< Self * >( this ); }

        template< typename Yield >
        void forChildren( Pool &p, Yield yield ) {
            assert( self().forkcount( p ) );
            for ( int32_t i = 0; i < self().forkcount( p ); ++i )
                yield( self().forkdata( p )[ i ] );
        }

        // emit all leafs until false is returned from yield
        template< typename Yield >
        bool forAllLeaves( Pool& p, Yield yield ) {
            bool result = false;
            self().forChildren( p, [&]( LeafOrFork lof ) {
                    if ( lof.isLeaf() ) {
                        if ( !yield( lof.leaf() ) )
                            return false;
                    } else {
                        if ( !lof.fork().forAllLeaves( p, yield ) )
                            return false;
                    }
                    return result = true;
                } );
            return result;
        }

        std::vector< LeafOr< Fork > > childvector( Pool &p ) {
            std::vector< LeafOrFork > lof;
            forChildren( p, [&lof]( LeafOr< Fork > x ) { lof.push_back( x ); } );
            return lof;
        }
    };

    struct Fork : WithChildren< Fork, Fork > {
        using IsBlobNewtype = void;
        Blob b;

        Fork() = default;
        Fork( Blob b ) : b( b ) {}
        Fork( int32_t children, Pool& pool ) {
            assert_leq( 2, children );
            int32_t size = children * sizeof( LeafOr< Fork > );
            b = pool.allocate( size );
            pool.clear( b );
        }

        int32_t forkcount( Pool &p ) {
            return p.size( b ) / sizeof( LeafOr< Fork > );
        }
        LeafOr< Fork > *forkdata( Pool &p ) {
            return p.dereference< LeafOrFork >( b );
        }
    };

    typedef LeafOr< Fork > LeafOrFork;

    struct Root : WithChildren< Root, Fork > {
        using IsBlobNewtype = void;
        Blob b;

        struct Header { int32_t forks; };

        Root() = default;
        explicit Root( Blob b ) : b( b ) {}
        Header &header( Pool &p ) { return *p.dereference< Header >( b ); }
        bool leaf( Pool &p ) { return header( p ).forks == 0; }
        int32_t forkcount( Pool &p ) { return header( p ).forks; }
        char *rawdata( Pool &p ) { return p.dereference( b ) + sizeof( Header ); }

        char *data( Pool &p ) {
            assert( leaf( p ) );
            return rawdata( p );
        }

        LeafOrFork *forkdata( Pool &p ) {
            assert( !leaf( p ) );
            return reinterpret_cast< LeafOrFork *> ( rawdata( p ) );
        }

        int32_t dataSize( Pool &p, int32_t slack ) {
            return p.size( b ) - sizeof( Header ) - slack;
        }

        int32_t slackoffset( Pool &p ) { return sizeof( LeafOrFork ) * forkcount( p ); }
        char *slack( Pool &p ) { return rawdata( p ) + slackoffset( p ); }

        Blob reassemble( Pool& p )
        {
            assert( p.valid( b ) );
            if ( leaf( p ) ) {
                int32_t size = p.size( b ) - sizeof( Header );
                assert_leq( 0, size );
                Blob out = p.allocate( size );
                std::copy( data( p ), data( p ) + size, p.dereference( out ) );
                return out;
            }

            std::vector< Leaf > leaves;
            int32_t size = 0;

            this->forAllLeaves( p, [ &p, &leaves, &size ]( Leaf leaf ) -> bool {
                    leaves.push_back( leaf );
                    size += leaf.size( p );
                    return true; // demand all
                } );

            char* slackptr = slack( p );
            int32_t slacksize = p.size( b ) - sizeof( Header ) - slackoffset( p );
            size += slacksize;
            Blob out = p.allocate( size );
            char* outptr = p.dereference( out );
            outptr = std::copy( slackptr, slackptr + slacksize, outptr );
            for ( auto l : leaves ) {
                assert_leq( outptr - p.dereference( out ), p.size( out ) );
                outptr = std::copy( l.data( p ), l.data( p ) + p.size( l.b ), outptr );
            }
            assert_eq( outptr - p.dereference( out ), p.size( out ) );
            return out;
        }

        static Root createFlat( Item it, Pool& pool ) {
            assert( pool.valid( it ) );
            Root r;
            r.b = pool.allocate( sizeof( Header ) + pool.size( it ) );
            r.header( pool ).forks = 0;
            std::copy( pool.dereference( it ), pool.dereference( it ) + pool.size( it ),
                       pool.dereference( r.b ) + sizeof( Header ) );
            return r;
        }

        static Root create( Item it, int32_t children, int32_t slack, Pool& pool ) {
            assert_leq( 2, children );
            assert_leq( 0, slack );
            uintptr_t size = sizeof( Header ) + slack + sizeof( LeafOrFork ) * children;
            Root root;
            root.b = pool.allocate( size );
            root.header( pool ).forks = children;
            std::memset( root.rawdata( pool ), 0, sizeof( LeafOrFork ) * children );
            std::memcpy( pool.dereference( root.b ) + size - slack,
                         pool.dereference( it ), slack );
            return root;
        }
    };

    struct Uncompressed { Item i; Uncompressed( Item i ) : i( i ) {} };

    using ForkHasher = NewtypeHasher< Fork >;
    using LeafHasher = NewtypeHasher< Leaf >;

    struct RootHasher {
        UncompressedHasher uhasher;
        uint32_t salt;

        RootHasher( UncompressedHasher uhasher ) : RootHasher( uhasher, 0 ) { }
        RootHasher( UncompressedHasher uhasher, uint32_t salt )
            : uhasher( uhasher), salt( salt )
        { }

        Pool &pool() { return uhasher.pool(); }
        int32_t slack() { return uhasher.slack; }

        hash128_t hash( Uncompressed u ) { return uhasher.hash( u.i ); }
        hash128_t hash( Root r ) {
            if ( r.leaf( pool() ) ) {
                auto offset = sizeof( typename Root::Header ) + uhasher.slack;
                return pool().hash( r.b, offset, offset + r.dataSize( pool(), uhasher.slack ) );
            } else {
                SpookyState state( salt, salt );
                r.forAllLeaves( pool(), [ this, &state ]( Leaf l ) {
                            state.update( l.data( this->pool() ), l.size( this->pool() ) );
                            return true;
                        } );
                return state.finalize();
            }
        }

        bool valid( Root r ) { return pool().valid( r.b ); }
        bool valid( Uncompressed r ) { return pool().valid( r.i ); }
        void setSeed( hash64_t s ) { uhasher.setSeed( s ); }

        bool equal( Root r1, Root r2 ) {
            if ( r1.b.raw() == r2.b.raw() )
                return true;
            if ( r1.leaf( pool() ) && r2.leaf( pool() ) )
                return pool().equal( r1.b, r2.b, sizeof( typename Root::Header )
                        + uhasher.slack );
            else if ( !r1.leaf( pool() ) && !r2.leaf( pool() ) ) {
                int32_t s1 = pool().size( r1.b );
                int32_t s2 = pool().size( r2.b );
                return s1 == s2
                    && pool().equal( r1.b, r2.b, sizeof( typename Root::Header ),
                            sizeof( typename Root::Header ) + r1.slackoffset( pool() ) );
            }

            /*
             * Note on root equality: Leaves and forks are cononical, that is
             * only one instance of leaf/fork representing same subnode exists
             * per HashSet, and instances from diffent HashSets can't represent
             * same nodes (as they differ in hash).  Therefore roots can be
             * compared trivially (on pointers to leafs/forks.
             */
            return false;
        }

        bool equal( Root root, Uncompressed item ) {
            return equal( item, root );
        }

        bool equal( Uncompressed item, Root root ) {
            int32_t itSize = pool().size( item.i ) - slack();
            char* itemPtr = pool().dereference( item.i ) + slack();
            if ( root.leaf( pool() ) ) {
                return itSize == root.dataSize( pool(), slack() )
                    && std::memcmp( itemPtr,
                                    root.data( pool() ) + slack(), itSize ) == 0;
            }

            itSize += slack();
            int32_t pos = slack();
            bool equal = true;

            root.forAllLeaves( pool(), [&] ( Leaf leaf ) {
                    assert( equal );
                    assert( this->pool().valid( leaf.b ) );
                    assert( itemPtr != nullptr );
                    assert_leq( 0, pos );

                    Pool &p = this->pool();
                    assert_leq( pos + leaf.size( p ), itSize );
                    equal = std::memcmp( itemPtr, leaf.data( p ),
                                         leaf.size( p ) ) == 0;
                    itemPtr += leaf.size( p );
                    pos += leaf.size( p );
                    return equal;
                } );
            assert_leq( 0, pos );
            return equal;
        }

        bool equal( Uncompressed i1, Uncompressed i2 ) const {
            return uhasher.equal( i1.i, i2.i );
        }
    };

    typedef Blob InsertItem;
    typedef Root StoredItem;

    using Hasher = RootHasher;

    struct ThreadData{
        typename HashSet< Root, RootHasher >::ThreadData roots;
        typename HashSet< Fork, ForkHasher >::ThreadData forks;
        typename HashSet< Leaf, LeafHasher >::ThreadData leafs;
    };

    RootHasher hasher;

    HashSet< Root, RootHasher > _roots; // stores slack and roots of tree
                                        // (also in case when root leaf)
    HashSet< Fork, ForkHasher > _forks; // stores intermediate nodes
    HashSet< Leaf, LeafHasher > _leafs; // stores leafs if they are not root

    Pool& pool() { return hasher.pool(); }
    int32_t slack() { return hasher.slack(); }
    bool valid( Item i ) { return hasher.valid( i ); }
    char* slackPtr( Item item ) { return pool().dereference( item ); }

    void copySlack( Item slackSource, Item root ) {
        pool().copy( slackSource, root, slack() );
    }

    size_t size() { return _roots.size(); }
    bool empty() { return _roots.empty(); }

    template < typename TD >
    std::tuple< Root, bool > insertHinted( Item item, hash64_t hash,
                                           TD &td )
    {
        assert( hasher.valid( item ) );

        Pool& pool = td.generator->pool();

        Root root;

        LeafOrFork* ptr = nullptr;
        char* from = pool.dereference( item ) + slack();
        td.generator->splitHint( item, [&]( Recurse rec, intptr_t length, intptr_t remaining ) {
                if ( rec == Recurse::No ) {
                    root = Root::createFlat( item, pool );
                    assert_eq( remaining, 0 );
                } else {
                    if ( !pool.valid( root.b ) ) {
                        root = Root::create( item, remaining + 1,
                                             this->slack(), pool );
                        ptr = root.forkdata( pool );
                        assert_leq( 1, remaining );
                    }
                    assert( ptr != nullptr );

                    *ptr = this->createChild( item, from, length, pool, td );
                    ++ptr;
                }

                assert( pool.valid( root.b ) );
                from += length;
            } );

        assert( pool.valid( root.b ) );
        assert_eq( size_t( from ), size_t( pool.dereference( item ) + pool.size( item ) ) );

        Root tr;
        bool inserted;
        std::tie( tr, inserted ) = _roots.insertHinted( root, hash, td.roots );
        if ( !inserted )
            pool.free( root.b );

        return std::make_tuple( tr, inserted );
    }

    template < typename TD >
    LeafOrFork createChild( Item item, char* from, intptr_t length,
                            Pool &pool, TD &td )
    {
        assert_leq( pool.dereference( item ) + slack(), from );
        assert_leq( from, pool.dereference( item ) + pool.size( item ) );

        LeafOrFork child;
        LeafOrFork* ptr = nullptr;
        td.generator->splitHint( item, from - pool.dereference( item ), length,
                             [&]( Recurse rec, intptr_t length, intptr_t remaining ) {
                if ( rec == Recurse::No ) {
                    assert( child.isNull( pool ) );
                    assert( ptr == nullptr );
                    assert_eq( remaining, 0 );
                    child = Leaf( length, from, pool );
                } else {
                    if ( child.isNull( pool ) ) {
                        child = Fork( remaining + 1, pool );
                        ptr = child.fork().forkdata( pool );
                    }
                    assert( !child.isNull( pool ) );
                    assert( ptr != nullptr );

                    *ptr = this->createChild( item, from, length, pool, td );
                    ++ptr;
                }
                from += length;
            } );

        bool inserted;
        LeafOrFork tlf;
        if ( child.isLeaf() ) {
            std::tie( tlf, inserted ) = _leafs.insert( child.leaf(), td.leafs );
            if ( !inserted ) {
                pool.free( child.blob() );
            }
        } else {
            std::tie( tlf, inserted ) = _forks.insert( child.fork(), td.forks );
            if ( !inserted )
                pool.free( child.blob() );
        }

        return tlf;
    }

    template< typename TD >
    std::tuple< Root, bool > get( Item item, TD &td ) {
        return getHinted( item, hasher.hash( item ).first, td );
    }

    template< typename T, typename TD >
    std::tuple< Root, bool > getHinted( T i, hash64_t h, TD &td ) {
        return _roots.getHinted( Uncompressed( i ), h, td.roots );
    }

    template< typename TD >
    bool has( Item i, TD &td ) {
        return std::get< 1 >( get( i, td ) );
    }

    void setSize( size_t s ) {
        _roots.setSize( s );
        _forks.setSize( s );
        _leafs.setSize( s );
    }

    void clear() {
        _roots.clear();
        _forks.clear();
        _leafs.clear();
    }

    Root operator[]( intptr_t off ) {
        assert_leq( 0, off );
        assert_leq( off, intptr_t( size() ) - 1 );
        return _roots[ off ];
    }
};

}
#endif
