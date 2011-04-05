// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <stdint.h>
#include <wibble/sys/mutex.h>

#ifndef DIVINE_HASHSET_H
#define DIVINE_HASHSET_H

namespace divine {

typedef uint32_t hash_t;

// default hash implementation
template< typename T >
struct hash {
    inline hash_t operator()( T t ) const {
        return t.hash();
    }
};

// default validity implementation
template< typename T >
struct valid {
    inline bool operator()( T t ) const {
        return t.valid();
    }
};

// default equality implementation
template< typename T >
struct equal {
    inline bool operator()( T a, T b ) const {
        return a == b;
    }
};

template<>
struct valid< int > {
    inline bool operator()( int t ) const {
        return t != 0;
    }
};

template<>
struct hash< int > {
    inline hash_t operator()( int t ) const {
        return t;
    }
};

/**
 * An implementation of high-performance hash table, used as a set. It's an
 * open-hashing implementation with a combination of linear and quadratic
 * probing. It also uses a hash-compacted prefilter to avoid fetches when
 * looking up an item and the item stored at the current lookup position is
 * distinct (a collision).
 *
 * An initial size may be provided to improve performance in cases where it is
 * known there will be many elements. Table growth is exponential with base 2
 * and is triggered at 75% load. (See maxcollision().)
 */
template< typename _Item,
          typename _Hash = divine::hash< _Item >,
          typename _Valid = divine::valid< _Item >,
          typename _Equal = divine::equal< _Item > >
struct HashSet
{
    typedef _Item Item;
    typedef _Hash Hash;
    typedef _Valid Valid;
    typedef _Equal Equal;

    int maxcollision() { return 65536; }
    int growthreshold() { return 75; } // percent

    struct Cell {
        Item item;
        hash_t hash;
    };

    typedef std::vector< Cell > Table;
    Table m_table;

    int m_used;
    int m_bits;
    size_t m_maxsize;
    bool growing;

    Hash hash;
    Valid valid;
    Equal equal;

    size_t usage() {
        return m_used;
    }

    size_t size() const { return m_table.size(); }
    bool empty() const { return !m_used; }

    int index( int h, int i ) const {
        const int Q = 1, R = 1, thresh = 16;
        if ( i <= thresh )
            return (h + i) & m_bits;
        else {
            i = i - thresh;
            return (h + thresh + (2 * Q + 1) * i + 2 * R * (i * i)) & m_bits;
        }
    };

    inline Item insert( Item i ) {
        return insertHinted( i, hash( i ) );
    }

    inline Item insertHinted( Item i, hash_t h ) {
        Cell c;
        c.item = i;
        c.hash = h;
        return insertCell( c, m_table, m_used );
    }

    bool has( Item i ) { return valid( get( i ) ); }
    Item get( Item i ) { return getHinted( i, hash( i ) ); }
    Item getHinted( Item i, hash_t h, bool* has = NULL ) {
        Cell c;
        c.item = i;
        c.hash = h;
        const Item& item = getCell( c );
        if ( has != NULL ) *has = valid( item );
        return item;
    }

    Item getCell( Cell c ) // const (bleh)
    {
        size_t idx;
        for ( int i = 0; i < maxcollision(); ++i ) {
            idx = index( c.hash, i );

            if ( !valid( m_table[ idx ].item ) )
                return Item(); // invalid

            if ( cellEq( c, m_table[ idx ] ) )
                return m_table[ idx ].item;
        }
        // we can be sure that the element is not in the table *because*: we
        // never create chains longer than "mc", and if we haven't found the
        // key in this many steps, it can't be in the table
        return Item();
    }

    inline bool cellEq( Cell a, Cell b) {
        return a.hash == b.hash && equal( a.item, b.item );
    }

    inline Item insertCell( Cell c, Table &table, int &used )
    {
        assert( valid( c.item ) ); // ensure key validity

        if ( !growing && m_used > (size() / 100) * 75 )
            grow();

        int idx;
        for ( int i = 0; i < maxcollision(); ++i ) {
            idx = index( c.hash, i );

            if ( !valid( table[ idx ].item ) ) {
                ++ used;
                table[ idx ] = c;
                return c.item; // done
            }

            if ( cellEq( c, table[ idx ] ) )
                return table[ idx ].item;

        }

        grow();

        return insertCell( c, table, used );
    }

    void grow() {
        if ( 2 * size() >= m_maxsize ) {
            std::cerr << "Sorry, ran out of space in the hash table!" << std::endl;
            abort();
        }

        if( growing ) {
            std::cerr << "Oops, too many collisions during table growth." << std::endl;
            abort();
        }

        growing = true;

        int used = 0;

        Table table;

        table.resize( 2 * size(), Cell() );
        m_bits |= (m_bits << 1); // unmask more

        for ( size_t i = 0; i < m_table.size(); ++i ) {
            if ( valid( m_table[ i ].item ) )
                insertCell( m_table[ i ], table, used );
        }

        std::swap( table, m_table );
        assert_eq( used, m_used );

        growing = false;
    }

    void setSize( size_t s )
    {
        m_bits = 0;
        while ((s = s >> 1))
            m_bits |= s;
        m_table.resize( m_bits + 1, Cell() );
    }

    void clear() {
        m_used = 0;
        std::fill( m_table.begin(), m_table.end(), Item() );
    }

    Item operator[]( int off ) {
        return m_table[ off ].item;
    }

    HashSet( Hash h = Hash(), Valid v = Valid(), Equal eq = Equal(),
             int initial = 32 )
        : hash( h ), valid( v ), equal( eq )
    {
        growing = false;
        m_used = 0;
        m_maxsize = -1;
        setSize( initial );

        // assert that default key is invalid, this is assumed
        // throughout the code
        assert( !valid( Item() ) );
    }
};

}

namespace std {

template< typename Key >
void swap( divine::HashSet< Key > &a, divine::HashSet< Key > &b )
{
    swap( a.m_table, b.m_table );
    swap( a.m_used, b.m_used );
}

}

#endif
