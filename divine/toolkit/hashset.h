// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <stdint.h>
#include <tuple>
#include <wibble/sys/mutex.h>
#include <divine/toolkit/pool.h>

#ifndef DIVINE_HASHSET_H
#define DIVINE_HASHSET_H

namespace divine {

typedef uint32_t hash_t;

// default hash implementation
template< typename T >
struct default_hasher {
    Pool& pool;
    default_hasher( Pool& p ) : pool( p ) { }
    hash_t hash( T t ) const { return pool.hash( t ); }
    bool valid( T t ) const { return t.valid(); }
    bool equal( T a, T b )const { return pool.compare( a, b ) == 0; }
};

template<>
inline bool default_hasher< Blob >::valid( Blob b ) const { return pool.valid( b ); }

template<>
inline bool default_hasher< Blob >::alias( Blob a, Blob b ) const { return pool.alias( a, b ); }

template<>
struct default_hasher< int > {
    template< typename X >
    default_hasher( X& ) { }
    default_hasher() = default;
    hash_t hash( int t ) const { return t; }
    bool valid( int t ) const { return t != 0; }
    bool equal( int a, int b ) const { return a == b; }
    bool alias( int, int ) const { return false; }
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
template< typename _Item, typename _Hasher = divine::default_hasher< _Item > >
struct HashSet
{
    typedef _Item Item;
    typedef Item TableItem;
    typedef _Hasher Hasher;

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

    Hasher hasher;

    size_t usage() {
        return m_used;
    }

    size_t size() const { return m_table.size(); }
    bool empty() const { return !m_used; }

    size_t index( size_t h, size_t i ) const {
        const size_t Q = 1, R = 1, thresh = 16;
        if ( i <= thresh )
            return (h + i) & m_bits;
        else {
            i = i - thresh;
            return (h + thresh + (2 * Q + 1) * i + 2 * R * (i * i)) & m_bits;
        }
    };

    inline std::tuple< Item, bool > insert( Item i ) {
        return insertHinted( i, hasher.hash( i ) );
    }

    inline std::tuple< Item, bool > insertHinted( Item i, hash_t h ) {
        Cell c;
        c.item = i;
        c.hash = h;
        return insertCell( c, m_table, m_used );
    }

    bool has( Item i ) {
        return hasher.valid( std::get< 0 >( get( i ) ) );
    }

    std::tuple< Item, bool > get( Item i ) { return getHinted( i, hasher.hash( i ) ); }

    template< typename T >
    std::tuple< Item, bool > getHinted( T item, hash_t hash ) {
        size_t idx;
        for ( int i = 0; i < maxcollision(); ++i ) {
            idx = index( hash, i );

            if ( !hasher.valid( m_table[ idx ].item ) )
                return std::make_tuple( Item(), false ); // invalid

            if ( m_table[ idx ].hash == hash
                    && hasher.equal( item, m_table[ idx ].item ) )
                return std::make_tuple( m_table[ idx ].item, true );
        }
        // we can be sure that the element is not in the table *because*: we
        // never create chains longer than "mc", and if we haven't found the
        // key in this many steps, it can't be in the table
        return std::make_tuple( Item(), false );
    }

    inline bool cellEq( Cell a, Cell b) {
        return a.hash == b.hash && hasher.equal( a.item, b.item );
    }

    inline std::tuple< Item, bool > insertCell( Cell c, Table &table, int &used )
    {
        assert( hasher.valid( c.item ) ); // ensure key validity

        if ( !growing && size_t( m_used ) > (size() / 100) * 75 )
            grow();

        size_t idx;
        for ( int i = 0; i < maxcollision(); ++i ) {
            idx = index( c.hash, i );

            if ( !hasher.valid( table[ idx ].item ) ) {
                ++ used;
                table[ idx ] = c;
                return std::make_tuple( c.item, true ); // done
            }

            if ( cellEq( c, table[ idx ] ) )
                return std::make_tuple( table[ idx ].item, false );

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
            if ( hasher.valid( m_table[ i ].item ) )
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


    HashSet() : HashSet( Hasher() ) { }
    explicit HashSet( Hasher h ) : HashSet( h, 32 ) { }

    HashSet( Hasher h, int initial )
        : m_used( 0 ), m_maxsize( -1 ), growing( false ), hasher( h )
    {
        setSize( initial );

        // assert that default key is invalid, this is assumed
        // throughout the code
        assert( !hasher.valid( Item() ) );
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
