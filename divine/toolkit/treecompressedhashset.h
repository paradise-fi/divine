// -*- C++ -*- (c) 2013 Vladimir Still <xstill@fi.muni.cz>
#ifndef TREE_COMPRESSED_HASH_SET_H
#define TREE_COMPRESSED_HASH_SET_H

#include <wibble/test.h> // assert
#include <tuple>
#include <algorithm>
#include <cstring>
#include <stack>
#include <queue>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>

#include <iostream>

namespace divine {

    // TreeCompressedHashSet :: ( * -> * -> * ) -> * -> * -> *
    // Item can be any type with same interface as Blob
    template< template< typename, typename > class _HashSet,
        typename _Item, typename _Hasher = default_hasher< _Item > >
    struct TreeCompressedHashSet
    {
        typedef _Item Item;
        typedef _Hasher Hasher;
        typedef _HashSet< Item, Hasher > BaseHashSet;

        template< typename... Args >
        TreeCompressedHashSet( Hasher hasher, int chunkSize, Args&&... args ) :
            m_roots( Hasher( hasher, hasher.slack + int( sizeof( Root ) ) ), args... ),
            m_forks( Hasher( hasher, 0 ), args... ),
            m_leafs( Hasher( hasher, 0 ), args... ),
            m_slack( hasher.slack ),
            m_chunkSize( chunkSize ),
            m_size( 0 ),
            hasher( hasher )
#ifndef O_PERFORMANCE
            , inserts( 0 ), leafReuse( 0 ), forkReuse( 0 ), rootReuse( 0 )
#endif
        {
            static_assert( sizeof( RootLeaf ) == sizeof( Root ),
                    "Algorithm assumes this" );
            static_assert( sizeof( RootLeaf ) <= offsetof( RootFork, left ),
                    "Algorithm assumes this" );
            static_assert( sizeof( RootLeaf ) <= offsetof( RootFork, right ),
                    "Algorithm assumes this" );
            static_assert( sizeof( Fork ) == 2 * sizeof( Item ),
                    "Misaligned Fork structure" );
            assert( chunkSize > 0 );
        }

#ifndef O_PERFORMANCE
        ~TreeCompressedHashSet() {
            std::cout << "~TreeCompressedHashSet: "
                      << "size: " << m_size << ", "
                      << "inserts: " << inserts << ", "
                      << "leafReuse: " << leafReuse << ", "
                      << "forkReuse: " << forkReuse << ", "
                      << "rootReuse: " << rootReuse << std::endl;
        }
#endif

        BaseHashSet m_roots; // stores slack and roots of tree
                             // (also in case when root leaf)
        BaseHashSet m_forks; // stores intermediate nodes
        BaseHashSet m_leafs; // stores leafs if they are not root
        const int m_slack;   // algorithm slack
        const int m_chunkSize;
        size_t m_size;

        Hasher hasher;

#ifndef O_PERFORMANCE
        size_t inserts;
        size_t leafReuse;
        size_t forkReuse;
        size_t rootReuse;
#endif

        inline Pool& pool() {
            return m_roots.hasher.pool;
        }

        inline const Pool& pool() const {
            return m_roots.hasher.pool;
        }

        inline int slack() const {
            return m_slack;
        }

        inline bool valid( Item i ) {
            return m_roots.hasher.valid( i );
        }

        inline Item makePermanent( Item i ) {
            assert( valid( i ) );
            pool().header( i ).permanent = true;
            return i;
        }

        struct Fork {
            const Item left;
            const Item right;
            Fork( Item left, Item right ) : left( left ), right( right ) { }
        };

        struct RootFork {
            const int size:24;
            const bool fork:1;
            const bool valid:1;
            const int __pad:6;
            const hash_t hash;
            const Item left;
            const Item right;
            RootFork( int size, Item left, Item right, hash_t hash ) : size( size ),
                fork( true ), valid( true ), __pad( 0 ), hash( hash ),
                left( left ), right( right )
            { }
        };

        struct RootLeaf {
            const int size:24;
            const bool fork:1;
            const bool valid:1;
            const int __pad:6;
            RootLeaf( int size ) : size( size ), fork( false ),
                    valid( true ), __pad( 0 )
            { }
        };

        struct Root {
            const int size:24;
            const bool fork:1;
            const bool valid:1;
            const int __pad:5;
            static const Root invalid;
          private:
            Root() : size( 0 ), fork( false ), valid( false ), __pad( 0 ) { }
        };

        /* The hash table entry layout:
         * +--------------------------+-----------------------+---------------+
         * | slack (set by algorithm) | header (Leaf || Fork) | data?         |
         * +--------------------------+-----------------------+---------------+
         * data are present only if header is Leaf
         *
         * slack is meaningfull only if header.root holds
         * length is without slack
         */

        inline const Root& header( Item item ) {
            if ( !valid( item ) )
                return Root::invalid;
            assert( pool().size( item ) >= slack() + int( sizeof( Root ) ) );
            return pool().template get< Root >( item, slack() );
        }

        inline RootFork* rootFork( Item item, bool unsafe = false ) {
            assert( pool().size( item ) >= slack() + int( sizeof( RootFork ) ) );
            RootFork* f = &pool().template get< RootFork >( item, slack() );
            assert( unsafe || f->fork );
            return f;
        }

        inline RootLeaf* rootLeaf( Item item, bool unsafe = false ) {
            assert( pool().size( item ) >= slack() + int( sizeof( RootLeaf ) ) );
            RootLeaf* l = &pool().template get< RootLeaf >( item, slack() );
            assert( unsafe || !l->fork );
            return l;
        }

        inline Fork* fork( Item item ) {
            assert( pool().size( item ) >= int( sizeof( Fork ) ) );
            return &pool().template get< Fork >( item );
        }

        inline char* slackPtr( Item item ) {
            return pool().data( item );
        }

        inline Item newRoot( Item slackSource, Item left, Item right, hash_t hash ) {
            assert( valid( slackSource ) );
            assert( valid( left ) );
            Item r( pool(), slack() + int( sizeof( RootFork ) ) );
            new ( rootFork( r, true ) ) RootFork(
                    pool().size( slackSource ) - slack(), left, right, hash );
            copySlack( slackSource, r );
            return r;
        }

        inline Item newRoot( Item source ) {
            assert( valid( source ) );
            assert_leq( pool().size( source ), slack() + m_chunkSize );
            int size = pool().size( source ) + int( sizeof( RootLeaf ) );
            Item r( pool(), size );
            size -= slack() + int( sizeof( RootLeaf ) );
            new ( rootLeaf( r, true ) ) RootLeaf( size );
            copySlack( source, r );
            pool().copyTo( source, r, slack(),
                    slack() + int( sizeof( RootLeaf ) ), size );
            return r;
        }

        inline void copySlack( Item slackSource, Item root ) {
            pool().copyTo( slackSource, root, slack() );
        }

        inline Item newFork( Item left, Item right ) {
            assert( valid( left ) );
            // right can be invalid -> unbalanced tree
            Item f( pool(), int( sizeof( Fork ) ) );
            new ( fork( f ) ) Fork( left, right );
            return f;
        }

        inline Item newLeaf( Item source, int start, int length ) {
            assert( valid( source ) );
            assert_leq( 0, start );
            assert_leq( 0, length );
            assert_leq( start + length + slack(), pool().size( source ) );

            Item it( pool(), length );
            toLeaf( source, it, start, length );
            return it;
        }

        inline void toLeaf( Item source, Item leaf, int sStart, int length ) {
            assert( valid( source ) );
            assert( valid( leaf ) );
            pool().copyTo( source, leaf, slack() + sStart, 0, length );
        }

        // copy data from leaf returning leaf size
        template < bool ROOT >
        inline int fromLeaf( Item l, Item target, int tStart )
        {
            assert( valid( l ) );
            int size = pool().size( l );
            pool().copyTo( l, target, ROOT ? slack() + int( sizeof( RootLeaf ) ) : 0,
                    slack() + tStart, size );
            return size;
        }

        Item getReassembled( Item item ) {
            assert( valid( item ) );
//            assert( m_roots.has( item ) );
            const Root& node = header( item );
            Item out( pool(), node.size + slack() );
            copySlack( item, out );

            if ( node.fork ) {
                RootFork* f = rootFork( item );
                std::stack< Item > stack;
                stack.push( f->right );
                stack.push( f->left );
                int position = 0;

                while ( !stack.empty() ) {
                    Item ci = stack.top();
                    stack.pop();
                    if ( !valid( ci ) )
                        continue;

                    if ( m_forks.has( ci ) ) {
                        Fork* cf = fork( ci );
                        stack.push( cf->right );
                        stack.push( cf->left );
                    } else {
                        position += fromLeaf< false >( ci, out, position );
                    }
                }
            } else {
                fromLeaf< true >( item, out, 0 );
            }
            return out;
        }

        size_t size() {
            return m_roots.size();
        }

        bool empty() {
            return m_roots.empty();
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

        std::tuple< Item, bool > insert( Item item ) {
            return insertHinted( item, hasher.hash( item ) );
        }

        std::tuple< Item, bool > insertHinted( Item item, hash_t hash ) {
            incInserts();

            int size = pool().size( item ) - slack();
            assert( size > 0 );
            if ( size <= m_chunkSize ) {
                Item it = newRoot( item ), it2;
                bool inserted;
                std::tie( it2, inserted ) = m_roots.insertHinted( it, hash );
                if ( !inserted ) {
                    pool().free( it );
                    incLeafReuse();
                    incRootReuse();
                } else
                    makePermanent( it2 );
                return std::make_tuple( it2, inserted );
            }

            auto initAct = [ this ]( Item source, int position, int size ) -> Item
            {
                Item ci = this->newLeaf( source, position, size ), ci2;
                bool inserted;
                std::tie( ci2, inserted ) = this->m_leafs.insert( ci );
                if ( !inserted ) {
                    this->pool().free( ci );
                    this->incLeafReuse();
                } else
                    this->makePermanent( ci2 );
                assert( this->valid ( ci2 ) );
                return ci2;
            };
            auto forkAct = [ this ]( Item l, Item r ) -> Item {
                Item fork = this->newFork( l, r ), fork2;
                bool inserted;
                std::tie( fork2, inserted ) = this->m_forks.insert( fork );
                if ( !inserted ) {
                    this->pool().free( fork );
                    this->incForkReuse();
                } else
                    this->makePermanent( fork2 );
                assert( this->valid( fork2 ) );
                return fork2;
            };
            bool rootInserted = false;
            auto rootAct = [ this, hash, &rootInserted ]
                ( Item source, Item l, Item r ) -> Item
            {
                Item root = this->newRoot( source, l, r, hash ), tableRoot;
                std::tie( tableRoot, rootInserted ) = this->m_roots.insert( root );
                if ( !root.alias( tableRoot ) ) {
                    this->incRootReuse();
                    this->pool().free( root );
                } else
                    this->makePermanent( tableRoot );
                assert( this->valid( tableRoot ) );
                return tableRoot;
            };
            return std::make_tuple(
                    bottomUpBuild( initAct, forkAct, rootAct, item, size ),
                    rootInserted );
        }

        std::tuple< Item, bool > get( Item item ) {
            return getHinted( item, hasher.hash( item ) );
        }

        std::tuple< Item, bool > getHinted( Item item, hash_t hash ) {
            assert( valid( item ) );
            int size = pool().size( item ) - slack();
            assert( size > 0 );
            if ( size <= m_chunkSize ) {
                Item l = newRoot( item ), l2;
                bool had;
                std::tie( l2, had ) = m_roots.getHinted( l, hash );
                pool().free( l );
                assert_eq( this->valid( l2 ), had );
                assert( !this->valid( l2 ) ||  this->header( l2 ).size == size );
                return std::make_tuple( l2, had );
            }

            auto initAct = [ this ]( Item source, int position, int size ) -> Item
            {
                Item l = this->newLeaf( source, position, size );
                Item l2 = std::get< 0 >( this->m_leafs.get( l ) );
                this->pool().free( l );
                return l2;
            };
            auto forkAct = [ this ]( Item l, Item r ) -> Item {
                Item f = this->newFork( l, r );
                Item f2 = std::get< 0 >( this->m_forks.get( f ) );
                this->pool().free( f );
                return f2;
            };
            bool hadRoot = false;
            auto rootAct = [ this, size, hash, &hadRoot ]
                ( Item source, Item l, Item r ) -> Item
            {
                Item root = this->newRoot( source, l, r, hash ), tableRoot;
                std::tie( tableRoot, hadRoot ) = this->m_roots.get( root );
                assert_eq( this->valid( tableRoot ), hadRoot );
                assert( !this->valid( tableRoot ) ||
                        this->header( root ).size == this->header( tableRoot ).size );
                assert( !this->valid( tableRoot ) ||
                        this->pool().size( source ) - this->slack() ==
                        this->header( tableRoot ).size );
                this->pool().free( root );
                return tableRoot;
            };
            return std::make_tuple(
                    bottomUpBuild( initAct, forkAct, rootAct, item, size ),
                    hadRoot );
        }

        template < typename InitAction, typename ForkAction, typename RootAction >
        Item bottomUpBuild( InitAction initAct, ForkAction forkAct,
                RootAction rootAct, Item source, int sourceSize )
        {
            assert( sourceSize <= pool().size( source ) - slack() );
            std::queue< Item > items;

            for ( int position = 0; position < sourceSize; position += m_chunkSize ) {
                Item i = initAct( source, position,
                        std::min( m_chunkSize, sourceSize - position ) );
                if ( !valid( i ) )
                    return i;
                else
                    items.push( i );
            }

            while ( true ) {
                items.push( Item() );
                bool cont = true;
                while ( cont && valid( items.front() ) ) {
                    Item l = items.front();
                    items.pop();
                    Item r = items.front();
                    items.pop();
                    cont = valid( r );
                    if ( cont && items.size() == 1 ) {
                        return rootAct( source, l, r );
                    }
                    Item f = forkAct( l, r );
                    if ( valid( f ) )
                        items.push( f );
                    else
                        return Item();
                }
                if ( cont && !items.empty() )
                    items.pop();
            }
        }

        bool has( Item i ) {
            return valid( get( i ) );
        }

        void setSize( size_t s ) {
        }

        void clear() {
            m_roots.clear();
            m_forks.clear();
            m_leafs.clear();
        }

        Item operator[]( int off ) {
            return m_roots[ off ];
        }
    };

    template< template< typename, typename > class _HashSet,
        typename Item, typename Hasher >
    const typename TreeCompressedHashSet< _HashSet, Item, Hasher >::Root
        TreeCompressedHashSet< _HashSet, Item, Hasher >::Root::invalid;
}
#endif
