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

namespace divine {

#define ChunkSize 32

    // TreeCompressedHashSetBase :: ( * -> * -> * ) -> * -> * -> int -> *
    // Creates tree compression topology with given chunk size (in bytes)
    // Item can be any type with same interface as Blob
    template< template< typename, typename > class _HashSet,
        typename _Item, typename _Hasher = default_hasher< _Item > >
    struct TreeCompressedHashSetBase
    {
        static_assert( ChunkSize > 0, "ChunkSize must be positive" );

        typedef _Item Item;
        typedef _Hasher Hasher;
        typedef _HashSet< Item, Hasher > BaseHashSet;

        template< typename... Args >
        TreeCompressedHashSetBase( Hasher hasher, Args&&... args ) :
            m_base( Hasher( hasher, hasher.slack + int( sizeof( Leaf ) ) ),
                    std::forward< Args >( args )... ),
            m_slack( hasher.slack ),
            m_size( 0 ),
            hasher( m_base.hasher )
#ifndef O_PERFORMANCE
            , inserts( 0 ), leafReuse( 0 ), forkReuse( 0 ), rootReuse( 0 )
#endif
        {
            static_assert( sizeof( Leaf ) == sizeof( Node ),
                    "Algorithm assumes this" );
            static_assert( sizeof( Leaf ) <= offsetof( Fork, left ),
                    "Algorithm assumes this" );
            static_assert( sizeof( Leaf ) <= offsetof( Fork, right ),
                    "Algorithm assumes this" );
        }

        BaseHashSet m_base;
        const int m_slack;
        size_t m_size;

        Hasher& hasher;

#ifndef O_PERFORMANCE
        size_t inserts;
        size_t leafReuse;
        size_t forkReuse;
        size_t rootReuse;
#endif

        inline Pool& pool() {
            return m_base.hasher.pool;
        }

        inline const Pool& pool() const {
            return m_base.hasher.pool;
        }

        inline int slack() const {
            return m_slack;
        }

        inline bool valid( Item i ) {
            return m_base.hasher.valid( i );
        }

        struct Fork {
            const int size:24;
            const bool fork:1;
            bool root:1;
            const bool valid:1;
            const int __pad:5;
            const Item left;
            const Item right;
            Fork( int size, Item left, Item right, bool root ) : size( size ),
                fork( true ), root( root ), valid( true ), __pad( 0 ),
                left( left ), right( right )
            { }
        };

        struct Leaf {
            const int size:24;
            const bool fork:1;
            bool root:1;
            const bool valid:1;
            const int __pad:5;
            Leaf( int size, bool root ) : size( size ), fork( false ),
                    root( root ), valid( true ), __pad( 0 )
            { }
        };

        struct Node {
            const int size:24;
            const bool fork:1;
            const bool root:1;
            const bool valid:1;
            const int __pad:5;
            static const Node invalid;
          private:
            Node() : size( 0 ), fork( false ), root( false ),
                    valid( false ), __pad( 0 )
            { }
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

        inline const Node& header( Item item ) {
            if ( !valid( item ) )
                return Node::invalid;
            assert( pool().size( item ) >= slack() + int( sizeof( Node ) ) );
            return pool().template get< Node >( item, slack() );
        }

        inline Fork* fork( Item item, bool unsafe = false ) {
            assert( pool().size( item ) >= slack() + int( sizeof( Fork ) ) );
            Fork* f = &pool().template get< Fork >( item, slack() );
            assert( unsafe || f->fork );
            return f;
        }

        inline Leaf* leaf( Item item, bool unsafe = false ) {
            assert( pool().size( item ) >= slack() + int( sizeof( Leaf ) ) );
            Leaf* l = &pool().template get< Leaf >( item, slack() );
            assert( unsafe || !l->fork );
            return l;
        }

        inline char* slackPtr( Item item ) {
            return pool().data( item );
        }

        inline Item initFork( int size, Item left, Item right, bool root ) {
            assert( valid( left ) );
            // right can be invalid -> unbalanced tree
            Item it( pool(), slack() + int( sizeof( Fork ) ) );
            new ( fork( it, true ) ) Fork( size, left, right, root );
            return it;
        }

        inline Item newRoot( Item left, Item right, Item slackSource ) {
            assert( valid( slackSource ) );
            assert( pool().size( slackSource ) - slack() ==
                    header( left ).size + header( right ).size );
            Item r = initFork( pool().size( slackSource ) - slack(),
                        left, right, true );
            copySlack( slackSource, r );
        }

        inline void copySlack( Item slackSource, Item root ) {
            pool().copyTo( slackSource, root, slack() );
        }

        inline Item newFork( Item left, Item right ) {
            Item f = initFork( header( left ).size + header( right ).size,
                        left, right, false);
            std::memset( slackPtr( f ), 0, slack() );
            std::memset( slackPtr( f ) + slack() + sizeof( Node ), 0,
                    offsetof( Fork, left ) - sizeof( Node ) );
            return f;
        }

        inline Item newLeaf( Item source, int start, int length ) {
            return newLeaf( false, source, start, length );
        }

        inline Item newLeaf( Item source ) {
            Item l = newLeaf( true, source, 0,
                        pool().size( source ) - slack() );
            pool().copyTo( source, l, slack() );
            return l;
        }

        inline Item newLeaf( bool root, Item source, int start, int length )
        {
            assert( valid( source ) );
            assert( start >= 0 );
            assert( length >= 0 );
            assert( !root || start == 0 );
            assert( !root || length == pool().size( source ) - slack() );
            assert( start + length + slack() <= pool().size( source ) );

            Item it( pool(), slack() + int( sizeof( Leaf ) ) + length );
            new ( leaf( it, true ) ) Leaf( length, root );
            toLeaf( source, it, start, length );
            return it;
        }

        inline void toLeaf( Item source, Item leaf, int sStart, int length ) {
            assert( valid( source ) );
            assert( valid( leaf ) );
            assert( header( leaf ).size >= length );
            pool().copyTo( source, leaf, slack() + sStart,
                    slack() + int( sizeof( Leaf ) ), length );
        }

        // copy data from leaf returning leaf size
        inline int fromLeaf( Item l, Item target, int tStart )
        {
            assert( valid( l ) );
            int size = leaf( l )->size;
            pool().copyTo( l, target, slack() + int( sizeof( Leaf ) ),
                    slack() + tStart, size );
            return size;
        }

        Item reassemble( Item item ) {
            assert( valid( item ) );
            const Node& node = header( item );
            assert( node.root );
            Item out( pool(), node.size + slack() );
            pool().copyTo( item, out, slack() );

            if ( node.fork ) {
                Fork* f = fork( item );
                std::stack< Item > stack;
                stack.push( f->right );
                stack.push( f->left );
                int position = 0;

                while ( !stack.empty() ) {
                    Item ci = stack.top();
                    stack.pop();
                    if ( !valid( ci ) )
                        continue;
                    const Node& cn = header( ci );

                    if ( cn.fork ) {
                        Fork* cf = fork( ci );
                        stack.push( cf->right );
                        stack.push( cf->left );
                    } else {
                        position += fromLeaf( ci, out, position );
                    }
                }
            } else {
                fromLeaf( item, out, 0 );
            }
            return out;
        }

        size_t size() {
            return m_size;
        }

        bool empty() {
            return m_base.empty();
        }

        void freeAll() {
            for ( typename BaseHashSet::Cell i : m_base.m_table ) {
                pool().free( i.item );
            }
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

        Item insertHinted( Item item, hash_t ) {
            return insert( item );
        }

        Item insert( Item item ) {
            incInserts();

            int size = pool().size( item ) - slack();
            assert( size > 0 );
            if ( size <= ChunkSize ) {
                Item it = newLeaf( item );
                Item it2 = m_base.insert( it );
                if ( !it.alias( it2 ) ) {
                    pool().free( it );
                    incLeafReuse();
                    incRootReuse();
                }
                return it2;
            }

            auto initAct = [ this ]( Item source, int position, int size ) -> Item
            {
                Item ci = this->newLeaf( source, position, size );
                Item ci2 = this->m_base.insert( ci );
                if ( !ci.alias( ci2 ) ) {
                    this->pool().free( ci );
                    this->incLeafReuse();
                }
                assert( this->valid ( ci2 ) );
                return ci2;
            };
            auto forkAct = [ this ]( Item l, Item r ) -> Item {
                Item fork = this->newFork( l, r );
                Item fork2 = this->m_base.insert( fork );
                if ( !fork.alias( fork2 ) ) {
                    this->pool().free( fork );
                    this->incForkReuse();
                }
                assert( this->valid( fork2 ) );
                return fork2;
            };
            auto rootAct = [ this ]( Item source, Item root ) -> Item {
                this->copySlack( source, root );
                Item tableRoot = this->m_base.insert( root );
                if ( !root.alias( tableRoot ) ) {
                    this->incRootReuse();
                    this->pool().free( root );
                    if ( !this->fork( tableRoot )-> root )
                        this->copySlack( source, tableRoot );
                }
                assert( this->valid( tableRoot ) );
                this->fork( tableRoot )->root = true;
                return tableRoot;
            };
            return bottomUpBuild( initAct, forkAct, rootAct, item, size );
        }


        Item get( Item item ) {
            assert( valid( item ) );
            int size = pool().size( item ) - slack();
            assert( size > 0 );
            if ( size <= ChunkSize ) {
                Item l = newLeaf( item );
                Item l2 = m_base.get( l );
                pool().free( l );
                assert( !valid( l2 ) ||  header( l2 ).size == size );
                return l2;
            }

            auto initAct = [ this ]( Item source, int position, int size ) -> Item
            {
                Item l = this->newLeaf( source, position, size );
                Item l2 = this->m_base.get( l );
                this->pool().free( l );
                if ( !this->valid( l2 ) ) {
                    return Item(); // not found
                } else {
                    assert( this->header( l2 ).size <= ChunkSize );
                    return l2;
                }
            };
            auto forkAct = [ this ]( Item l, Item r ) -> Item {
                Item f = this->newFork( l, r );
                Item f2 = this->m_base.get( f );
                if ( !this->valid( f2 ) )
                    return Item(); // not found
                assert( this->header( f ).size == this->header( f2 ).size );
                this->pool().free( f );
                return f2;
            };
            auto rootAct = [ this, size ]( Item source, Item root ) -> Item {
                if ( this->header( root ).root ) {
                    assert( this->header( root ).size == size );
                    return root;
                } else {
                    // found but is not root -> false positive
                    return Item();
                }
            };
            return bottomUpBuild( initAct, forkAct, rootAct, item, size );
        }

        template < typename InitAction, typename ForkAction, typename RootAction >
        Item bottomUpBuild( InitAction initAct, ForkAction forkAct,
                RootAction rootAct, Item source, int sourceSize )
        {
            assert( sourceSize <= pool().size( source ) - slack() );
            std::queue< Item > items;

            for ( int position = 0; position < sourceSize; position += ChunkSize ) {
                Item i = initAct( source, position,
                        std::min( ChunkSize, sourceSize - position ) );
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
                    if ( !cont && items.empty() ) {
                        return rootAct( source, l );
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

        Item getFull( Item i ) {
            return reassemble( get( i ) );
        }

        Item getHinted( Item i, hash_t h, bool* has = nullptr )
        {
            Item ii = get( i );
            if ( has != nullptr )
                *has = valid( ii );
            return ii;
            assert_unimplemented();
        }

        void setSize( size_t s ) {
        }

        void clear() {
            m_base.clear();
        }

        Item operator[]( int off ) {
            Item i = m_base. m_table[ off ].item;
            if ( header( i ).root )
                return reassemble( i );
            return Item();
        }
    };

    // The deault tree compressed hash sed using standard hash set as base
    // it is drop-in replacement of standard hashset
    template< typename Item, typename Hasher = default_hasher< Item > >
    using TreeCompressedHashSet = TreeCompressedHashSetBase< HashSet, Item, Hasher >;


    template< template< typename, typename > class _HashSet,
        typename Item, typename Hasher >
    const typename TreeCompressedHashSetBase< _HashSet, Item, Hasher >::Node
        TreeCompressedHashSetBase< _HashSet, Item, Hasher >::Node::invalid;
}
#endif
