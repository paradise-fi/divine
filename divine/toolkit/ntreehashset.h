// -*- C++ -*- (c) 2013 Vladimir Still <xstill@fi.muni.cz>
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
#include <divine/graph/graph.h>

#include <iostream>

#ifndef N_TREE_HASH_SET_H
#define N_TREE_HASH_SET_H

namespace divine {

    // NTreeHashSet :: ( * -> * -> * ) -> * -> * -> *
    // Item can be any type with same interface as Blob
    template< template< typename, typename > class HashSet,
        typename _Item, typename _Hasher >
    struct NTreeHashSet
    {
        typedef _Item Item;
        typedef _Hasher Hasher;
        typedef NTreeHashSet< HashSet, _Item, _Hasher > This;

        NTreeHashSet() : NTreeHashSet( Hasher() ) { }

        template< typename... Args >
        NTreeHashSet( Hasher hasher, Args&&... args ) :
            _roots( RootHasher( hasher ), args... ),
            _forks( ForkHasher(), args... ),
            _leafs( LeafHasher(), args... ),
            hasher( hasher )
#ifndef O_PERFORMANCE
            , inserts( 0 ), leafReuse( 0 ), forkReuse( 0 ), rootReuse( 0 )
#endif
        { }

#ifndef O_PERFORMANCE
        ~NTreeHashSet() {
            std::cout << "~NTreeHashSet: "
                      << "inserts: " << inserts << ", "
                      << "leafReuse: " << leafReuse << ", "
                      << "forkReuse: " << forkReuse << ", "
                      << "rootReuse: " << rootReuse << ", "
                      << "roots: " << _roots.size() << ", "
                      << "forks: " << _forks.size() << ", "
                      << "leafs: " << _leafs.size() << std::endl;
        }
#endif

        struct Leaf {
            uint16_t size;
            char data[ 2 ];

            static Leaf* create( uint32_t size, char* source, Pool& pool ) {
                assert_leq( 2, size );
                Leaf* leaf = reinterpret_cast< Leaf* >(
                        pool.allocate( size + sizeof( uint16_t ) ) );
                new ( leaf ) Leaf( size );
                memcpy( leaf->data, source, size );
                return leaf;
            }

            void free( Pool& p ) {
                p.free( reinterpret_cast< char* >( this ),
                        sizeof( uint16_t ) + size * sizeof( char ) );
            }

          private:
              Leaf( uint32_t size ) : size( size ) { }
        };

        template < typename Fork >
        class LeafOr {
            uintptr_t ptr;
          public:
            LeafOr() : ptr( 0 ) { }

            LeafOr( Fork* f ) :
                ptr( reinterpret_cast< uintptr_t >( f ) | 0x1 )
            {
                assert_eq( reinterpret_cast< uintptr_t >( f ) & 0x3, 0 );
            }

            LeafOr( Leaf* l ) :
                ptr( reinterpret_cast< uintptr_t >( l ))
            {
                assert_eq( reinterpret_cast< uintptr_t >( l ) & 0x3, 0 );
            }

            bool end() const {
                return ptr & 0x2;
            }

            void setEnd( bool end ) {
                ptr &= ~0x2;
                ptr |= end ? 0x2 : 0;
            }

            bool isLeaf() const {
                return !isFork();
            }

            bool isFork() const {
                return ptr & 0x1;
            }

            Fork* fork() {
                assert( isFork() );
                return reinterpret_cast< Fork* >( ptr & ~0x3 );
            }

            Leaf* leaf() {
                assert( isLeaf() );
                return reinterpret_cast< Leaf* >( ptr & ~0x3 );
            }

            bool isNull() {
                return ptr == 0;
            }
        };

        struct Fork {
            LeafOr< Fork > childs[ 2 ];

            uint32_t size() const {
                uint32_t i = 0;
                for ( ; !this->childs[ i ].end(); ++i ) { }
                return (i + 1) * sizeof( LeafOr< Fork > );
            }

            static Fork* create( uint32_t childs, Pool& pool ) {
                assert_leq( 2, childs );
                uint32_t size = childs * sizeof( LeafOr< Fork > );
                Fork* fork = reinterpret_cast< Fork* >( pool.allocate( size ) );
                std::memset( fork, 0, size );
                return fork;
            }

            void free( Pool& p ) {
                p.free( reinterpret_cast< char* >( this ), size() );
            }

          private:
            Fork() = default;
        };

        typedef LeafOr< Fork > LeafOrFork;

        struct Root {
            hash_t hash;
            uint32_t selfSize:24;
            bool leaf:1;
            bool permanent:1;
            uint32_t __pad:6;

            LeafOrFork* childs() {
                assert( !leaf );
                return reinterpret_cast< LeafOrFork* >( this + 1 );
            }

            char* data() {
                assert( leaf );
                return reinterpret_cast< char* >( this + 1 );
            }

            uint32_t dataSize( uint32_t slack ) {
                return reinterpret_cast< uintptr_t >( this ) + selfSize
                    - slack - reinterpret_cast< uintptr_t >( data() );
            }

            uint32_t forksSize() {
                uint32_t i = 0;
                for ( ; !this->childs()[ i ].end(); ++i ) { }
                return (i + 1) * sizeof( LeafOrFork );
            }

            char* slack() {
                return leaf ? data()
                    : reinterpret_cast< char* >( childs() ) + forksSize();
            }

            Item reassemble( Pool& pool ) {

                assert( this != nullptr );
                if ( leaf ) {
                    int size = selfSize - sizeof( Root );
                    assert_leq( 0, size );
                    Item out( pool, size );
                    std::memcpy( pool.data( out ), data(), size );
                    return out;
                }

                std::vector< Leaf* > leafs;

                forAllLeafs( pool, [ &leafs ]( Leaf* leaf ) -> bool {
                        leafs.push_back( leaf );
                        return true; // demand all
                    } );

                char* slackptr = slack();
                int slacksize = selfSize
                        - int( reinterpret_cast< uintptr_t >( slackptr )
                            - reinterpret_cast< uintptr_t >( this ) );
                int size = std::accumulate( leafs.begin(), leafs.end(), slacksize,
                        []( int s, Leaf* l ) { return s + l->size; } );
                Item out( pool, size );
                char* outptr = pool.data( out );
                std::memcpy( outptr, slackptr, slacksize );
                outptr += slacksize;
                for ( Leaf* l : leafs ) {
                    assert_leq( outptr - pool.data( out ), pool.size( out ) );
                    std::memcpy( outptr, l->data, l->size );
                    outptr += l->size;
                }
                assert_eq( outptr - pool.data( out ), pool.size( out ) );
                return out;
            }

            // emit all leafs until false is returned from yield
            template< typename Yield >
            void forAllLeafs( Pool& pool, Yield yield ) {

                std::vector< LeafOrFork > stack;

                LeafOrFork* childptr;
                for ( childptr = childs(); !childptr->end(); ++childptr ) { }
                for ( ; childptr >= childs(); --childptr )
                    stack.push_back( *childptr );
                assert_eq( childptr, childs() - 1 );

                while ( !stack.empty() ) {
                    LeafOrFork lf = stack.back();
                    stack.pop_back();
                    if ( lf.isLeaf() ) {
                        if ( !yield( lf.leaf() ) )
                            return;
                    }
                    else {
                        Fork* f = lf.fork();

                        LeafOrFork* ptr;
                        for ( ptr = f->childs; !ptr->end(); ++ptr ) { }
                        for ( ; ptr >= f->childs; --ptr )
                            stack.push_back( *ptr );
                        assert_eq( ptr + 1, f->childs );
                    }
                }
            }

            void free( Pool& p ) {
                if ( !permanent )
                    p.free( reinterpret_cast< char* >( this ), selfSize );
            }

            static Root* createFlat( Item it, Pool& pool ) {
                assert( it.valid() );
                uintptr_t size = sizeof( Root ) + pool.size( it );
                Root* root = reinterpret_cast< Root* >( pool.allocate( size ) );
                new ( root ) Root( size, true );
                std::memcpy( root->data(), pool.data( it ), size - sizeof( Root ) );
                return root;
            }

            static Root* create( Item it, uint32_t childs, int slack, Pool& pool ) {
                assert_leq( 2, childs );
                assert_leq( 0, slack );
                uintptr_t size = sizeof( Root ) + slack +
                    sizeof( LeafOrFork ) * childs;
                Root* root = reinterpret_cast< Root* >( pool.allocate( size ) );
                new ( root ) Root( size, false );
                std::memset( root->childs(), 0, sizeof( LeafOrFork ) * childs );
                std::memcpy( reinterpret_cast< char* >( root ) + size - slack,
                        pool.data( it ), slack );
                return root;
            }

            template < typename BS >
            friend bitstream_impl::base< BS >& operator<<(
                    bitstream_impl::base< BS >& bs, Root* r )
            {
                return bs << reinterpret_cast< uintptr_t >( r );
            }
            template < typename BS >
            friend bitstream_impl::base< BS >& operator>>(
                     bitstream_impl::base< BS >& bs, Root*& r )
            {
                uintptr_t ri;
                bs >> ri;
                r = reinterpret_cast< Root* >( ri );
                return bs;
            }

          private:
            Root( uint32_t selfSize, bool leaf ) :
                hash( 0 ), selfSize( selfSize ), leaf( leaf ), __pad( 0 )
            { }
        };

        struct LeafHasher {
            uint32_t salt;

            LeafHasher() : LeafHasher( 0 ) { }
            LeafHasher( uint32_t salt ) : salt( salt ) { }

            hash_t hash( Leaf* l ) const {
                return jenkins3( l->data, l->size, salt );
            }
            bool valid( Leaf* l ) const { return l != nullptr; }
            bool equal( Leaf* l1, Leaf* l2 ) const {
                return l1 == l2 || ( l1->size == l2->size
                        && std::memcmp( l1->data, l2->data, l1->size ) == 0 );
            }
        };

        struct ForkHasher {
            uint32_t salt;

            ForkHasher() : ForkHasher( 0 ) { }
            ForkHasher( uint32_t salt ) : salt( salt ) { }

            hash_t hash( const Fork* f ) const {
                return jenkins3( f->childs, f->size(), salt );
            }
            bool valid( const Fork* f ) const { return f != nullptr; }
            bool equal( const Fork* f1, const Fork* f2 ) const {
                if ( f1 == f2 )
                    return true;
                uint32_t s1 = f1->size();
                uint32_t s2 = f2->size();
                return s1 == s2 && std::memcmp( f1->childs, f2->childs, s1 ) == 0;
            }
        };

        struct RootHasher {
            Hasher nodeHasher;
            uint32_t salt;

            RootHasher( Hasher nodeHasher ) : RootHasher( nodeHasher, 0 ) { }
            RootHasher( Hasher nodeHasher, uint32_t salt )
                : nodeHasher( nodeHasher ), salt( salt )
            { }

            hash_t hash( Root* r ) const {
                return r->hash;
            }
            hash_t hash( Item it ) const {
                return nodeHasher.hash( it );
            }

            bool valid( Root* r ) const { return r != nullptr; }
            bool valid( Item it ) const { return nodeHasher.valid( it ); }

            bool equal( Root* r1, Root* r2 ) const {
                if ( r1 == r2 )
                    return true;
                if ( r1->hash != r2->hash )
                    return false;
                if ( r1->leaf && r2->leaf ) {
                    return r1->selfSize == r2->selfSize
                        && std::memcmp( r1->data() + nodeHasher.slack,
                            r2->data() + nodeHasher.slack,
                            r1->dataSize( nodeHasher.slack ) ) == 0;
                } else if ( !r1->leaf && !r2->leaf ) {
                uint32_t s1 = r1->forksSize();
                uint32_t s2 = r2->forksSize();
                if ( s1 == s2 && std::memcmp( r1->childs(), r2->childs(), s1 ) == 0 )
                    return true;
                }
                // if this does not hold it can still
                      // be case that data are equal but saved using different
                      // geometry (probably due to mpi transfer)
                auto rr1 = r1->reassemble( nodeHasher.pool );
                auto rr2 = r2->reassemble( nodeHasher.pool );
                bool ret = nodeHasher.equal( rr1, rr2 );
                nodeHasher.pool.free( rr1 );
                nodeHasher.pool.free( rr2 );
                return ret;
            }

            bool equal( Root* root, Item item ) const {
                return equal( item, root );
            }
            bool equal( Item item, Root* root ) const {
                int itSize = nodeHasher.pool.size( item ) - nodeHasher.slack;
                char* itemPtr = nodeHasher.pool.data( item ) + nodeHasher.slack;
                if ( root->leaf ) {
                     return itSize == root->dataSize( nodeHasher.slack )
                         && std::memcmp( itemPtr,
                                 root->data() + nodeHasher.slack, itSize ) == 0;
                }

                itSize += nodeHasher.slack;
                int pos = nodeHasher.slack;
                bool equal = true;

                root->forAllLeafs( nodeHasher.pool,
                    [ &itemPtr, &pos, &equal, itSize ] ( Leaf* leaf ) {
                        assert( equal );
                        assert( leaf != nullptr );
                        assert( itemPtr != nullptr );
                        assert_leq( 0, pos );

                        assert_leq( pos + leaf->size, itSize );
                        equal = std::memcmp( itemPtr, leaf->data, leaf->size ) == 0;
                        itemPtr += leaf->size;
                        pos += leaf->size;
                        return equal;
                    } );
                assert_leq( 0, pos );
                return equal;
            }

            bool equal( Item i1, Item i2 ) const {
                return nodeHasher.equal( i1, i2 );
            }
        };

        typedef Root* TableItem;
        HashSet< Root*, RootHasher > _roots; // stores slack and roots of tree
                             // (also in case when root leaf)
        HashSet< Fork*, ForkHasher > _forks; // stores intermediate nodes
        HashSet< Leaf*, LeafHasher > _leafs; // stores leafs if they are not root
        Hasher hasher;

#ifndef O_PERFORMANCE
        size_t inserts;
        size_t leafReuse;
        size_t forkReuse;
        size_t rootReuse;
#endif

        inline Pool& pool() {
            return hasher.pool;
        }

        inline const Pool& pool() const {
            return hasher.pool;
        }

        inline int slack() const {
            return hasher.slack;
        }

        inline bool valid( Item i ) {
            return hasher.valid( i );
        }

        inline char* slackPtr( Item item ) {
            return pool().data( item );
        }

        inline void copySlack( Item slackSource, Item root ) {
            pool().copyTo( slackSource, root, slack() );
        }

        Item getReassembled( Item item ) {
        }

        size_t size() {
            return _roots.size();
        }

        bool empty() {
            return _roots.empty();
        }

        inline void incInserts() {
#ifndef O_PERFORMANCE
            ++inserts;
#endif
        }

        inline void incLeafReuse() {
#ifndef O_PERFORMANCE
            ++leafReuse;
#endif
        }

        inline void incForkReuse() {
#ifndef O_PERFORMANCE
            ++forkReuse;
#endif
        }

        inline void incRootReuse() {
#ifndef O_PERFORMANCE
            ++rootReuse;
#endif
        }
#if 0
        std::tuple< Root*, bool > insert( Item item ) {
            return insertHinted( item, hasher.hash( item ) );
        }
#endif
        template < typename Generator >
        std::tuple< Root*, bool > insertHinted( Item item, hash_t hash,
                Generator& generator )
        {
            assert( hasher.valid( item ) );
            incInserts();

            Pool& pool = generator.alloc.pool();

            Root* root = nullptr;
            LeafOrFork* ptr = nullptr;
            char* from = pool.data( item ) + slack();
            generator.splitHint( item,
                [ &from, &root, &ptr, &pool, &generator, this, item ]
                ( Recurse rec, intptr_t length, intptr_t remaining )
                {
                    if ( rec == Recurse::No ) {
                        root = Root::createFlat( item, pool );
                        assert_eq( remaining, 0 );
                    } else {
                        if ( root == nullptr ) {
                            root = Root::create( item, remaining + 1,
                                this->slack(), pool );
                            ptr = root->childs();
                            assert_leq( 1, remaining );
                        }
                        assert( ptr != nullptr );

                        *ptr = this->createChild( item, from, length, pool, generator );
                        ++ptr;
                    }

                    assert( root != nullptr );

                    from += length;
                } );
            if ( ptr != nullptr )
                (ptr - 1)->setEnd( true );
            assert( root != nullptr );
            assert_eq( from, pool.data( item ) + pool.size( item ) );

            root->hash = hash;

            Root* tr;
            bool inserted;
            std::tie( tr, inserted ) = _roots.insertHinted( root, hash );
            if ( !inserted ) {
                incRootReuse();
                root->free( pool );
            } else
                tr->permanent = true;

            return std::make_tuple( tr, inserted );
        }

        template < typename Generator >
        LeafOrFork createChild( Item item, char* from, intptr_t length,
                Pool& pool, Generator& generator )
        {
            assert_leq( pool.data( item ) + slack(), from );
            assert_leq( from, pool.data( item ) + pool.size( item ) );

            LeafOrFork child;
            LeafOrFork* ptr = nullptr;
            generator.splitHint( item, from - pool.data( item ), length,
                [ &from, &child, &ptr, &pool, &generator, this, item  ]
                ( Recurse rec, intptr_t length, intptr_t remaining )
                {
                    if ( rec == Recurse::No ) {
                        assert( child.isNull() );
                        assert( ptr == nullptr );
                        assert_eq( remaining, 0 );
                        child = Leaf::create( length, from, pool );
                    } else {
                        if ( child.isNull() ) {
                            child = Fork::create( remaining + 1, pool );
                            ptr = child.fork()->childs;
                        }
                        assert( !child.isNull() );
                        assert( ptr != nullptr );

                        *ptr = this->createChild( item, from, length, pool, generator );
                        ++ptr;
                    }
                    from += length;
                } );
            if ( ptr != nullptr )
                (ptr - 1)->setEnd( true );

            bool inserted;
            LeafOrFork tlf;
            if ( child.isLeaf() ) {
                std::tie( tlf, inserted ) = _leafs.insert( child.leaf() );
                if ( !inserted ) {
                    child.leaf()->free( pool );
                    incLeafReuse();
                }
            } else {
                std::tie( tlf, inserted ) = _forks.insert( child.fork() );
                if ( !inserted ) {
                    child.fork()->free( pool );
                    incForkReuse();
                }
            }

            return tlf;
        }

        std::tuple< Root*, bool > get( Item item ) {
            return getHinted( item, hasher.hash( item ) );
        }

        template< typename T >
        std::tuple< Root*, bool > getHinted( T i, hash_t h ) {
            return _roots.getHinted( i, h );
        }

        bool has( Item i ) {
            return std::get< 1 >( get( i ) );
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

        Root* operator[]( int off ) {
            return _roots[ off ];
        }
    };
}
#endif
