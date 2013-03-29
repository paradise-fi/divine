// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <atomic>
#include <stdint.h>
#include <wibble/sys/mutex.h>
#include <divine/toolkit/hashset.h>

#ifndef DIVINE_SHAREDHASHSET_H
#define DIVINE_SHAREDHASHSET_H

namespace divine {

/**
 * High-performance concurrent hash-table without support for resizing
 * (neither concurrent nor sequential). It's an open-hashing implementation
 * with a combination of linear and quadratic probing. It also uses a
 * hash-compacted prefilter to avoid fetches when looking up an item and the
 * item stored at the current lookup position is distinct (a collision).
 *
 * An initial size _must_ be provided.
 *
 * TODO: Merge SharedHashSet and HashSet, if feasible.
 */
template < typename T, typename Hasher = default_hasher< T > >
struct SharedHashSet {

    typedef T Item;

    enum { maxCollisions = 65536 };

    struct Cell {
        std::atomic< hash_t > hashLock;
        T value;

        /* TODO: parallel table initialization */
        Cell() : hashLock( 0 ) {}
        Cell( const Cell & ) : hashLock( 0 ) {}
    };

    unsigned mask;
    std::vector< Cell > table;
    Hasher hasher;
    unsigned _size;

    unsigned size() const { return _size; }

    /* TODO: need to revision of store change */
    explicit SharedHashSet( Hasher h = Hasher() )
        : mask( 0 ), hasher( h ), _size( 0 )
    { }

    void setSize( unsigned size ) {
        _size = size;
        mask = 0;
        while ((size = size >> 1))
            mask |= size;
        table.resize( mask + 1 );
    }

    void setHasher( Hasher h ) {
        hasher = h;
    }

    /* TODO: cache-line walking and double hashing */
    unsigned index( hash_t h, unsigned i ) const {
        const unsigned Q = 1, R = 1, thresh = 16;
        if ( i <= thresh )
            return (h + i) & mask;
        else {
            i = i - thresh;
            return (h + thresh + (2 * Q + 1) * i + 2 * R * (i * i)) & mask;
        }
    }

    /**
     * Insert an element into the set. Returns true if the element was added,
     * false if it was already in the set.
     */
    bool insert( const Item &x ) {
        return insertHinted( x, hasher.hash( x ) );
    }
    bool insertHinted( const Item &x, hash_t h) {
        h <<= 1;

        for ( unsigned i = 0; i < maxCollisions; ++i ) {
            Cell &cell = table[ index( h, i ) ];
            hash_t chl = cell.hashLock;

            if ( chl == 0 ) { /* empty → insert */
                /* Strong CAS since it won't be called twice. */
                if ( cell.hashLock.compare_exchange_strong( chl, h | 1 ) ) {
                    cell.value = x;
                    cell.hashLock.exchange( h & ~1 ); /* We need a barrier here. */
                    return true;
                }
            }

            if ( (chl | 1) == (h | 1) ) { /* hashes match → compare */
                /* Wait until value write finishes. */
                if ( chl & 1 )
                    while ( cell.hashLock & 1 );

                /* The wait here is not really necessary because x86 supports
                 * 8-byte CAS and x86_64 supports 16-byte CAS. We don't use it
                 * (yet) as the code would be less readable and, more
                 * importantly, gcc doesn't provide an API for 16-byte CAS.
                 * TODO: Remove the wait. */

                if ( hasher.equal( cell.value, x ) )
                    return false;
            }

            /* not empty and not equal → reprobe */
        }

        std::cerr << "too many collisions" << std::endl;
        abort();
        return false;
    }

    bool has( const Item &x ) {
        return has( x, hasher.hash( x ) );
    }

    bool has( const Item &x, hash_t h ) {
        h <<= 1;

        for ( unsigned i = 0; i < maxCollisions; ++i ) {
            Cell &cell = table[ index( h, i ) ];
            hash_t chl = cell.hashLock;

            if ( chl == 0 ) {
                return false;
            }
            if ( (chl | 1) == (h | 1) && ( hasher.equal( cell.value, x ) ) )
                return true;
        }

        std::cerr << "too many collisions" << std::endl;
        abort(); return false;
    }

    Item get( const Item &x ) {
        return getHinted( x, hasher.hash( x ) );
    }

    Item getHinted( const Item &x, hash_t h, bool* has = nullptr ) {

        h <<= 1;

        for ( unsigned i = 0; i < maxCollisions; ++i ) {
            Cell &cell = table[ index( h, i ) ];
            hash_t chl = cell.hashLock;

            if (chl == 0) {
                if (has) *has = false;
                return Item();
            }

            if ( (chl | 1) == (h | 1) && hasher.equal( cell.value, x ) ) {
                if (has) *has = true;
                return cell.value;
            }
        }
        std::cerr << "too many collisions" << std::endl;
        abort(); return x;
    }

    Item operator[]( unsigned index ) {
        return table[ index ].value;
    }

    SharedHashSet( const SharedHashSet & ) = delete;
    SharedHashSet &operator=( const SharedHashSet & )= delete;
};

}

#endif
