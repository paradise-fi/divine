// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <cmath>
#include <stdint.h>
#include <wibble/sys/mutex.h>
#include <wibble/sfinae.h> // for Unit

#ifndef DIVINE_HASHMAP_H
#define DIVINE_HASHMAP_H

namespace divine {

using wibble::Unit;
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

template< typename T >
struct VectorType {
    typedef std::vector< T > Vector;
};

template<>
struct VectorType< Unit > {
    struct Vector {
        size_t m_size;
        size_t size() { return m_size; }
        void resize( size_t s, Unit = Unit() ) { m_size = s; }
        Unit operator[]( size_t ) { return Unit(); }
        Vector() : m_size( 0 ) {}
    };
};

}

namespace divine {

/**
 * An implementation of high-performance hash table. May be used both as an
 * associative array and as an unordered set. To use a set, use Unit for the
 * _Value type parameter (in this case, the value storage is completely
 * optimised away, only memory for the keys is ever allocated).
 *
 * The implementation is based on a dynamically growing hashtable with
 * quadratic probing for collision resolution. The initial size may be provided
 * to improve performance in cases where it is known there will be many
 * elements. Table growth is exponential, the default base is 2 and is
 * controlled by both collision rate and load ratio (see maxcollision and
 * growthreshold).
 */
template< typename _Key, typename _Value,
          typename _Hash = divine::hash< _Key >,
          typename _Valid = divine::valid< _Key >,
          typename _Equal = divine::equal< _Key > >
struct HashMap
{
    typedef _Key Key;
    typedef _Value Value;
    typedef _Hash Hash;
    typedef _Valid Valid;
    typedef _Equal Equal;

    typedef std::pair< Key, Value > Item;

    int maxcollision() { return 32 + int( std::sqrt( size() ) ) / 16; }
    int growthreshold() { return 75; } // percent

    struct Reference {
        Key key;
        size_t offset;

        Reference( Key k, size_t off )
            : key( k ), offset( off )
        {}

        Reference() : key( Key() ), offset( 0 )
        {}
    };

    typedef std::vector< Key > Keys;
    // typedef std::vector< Value > Values;
    typedef typename VectorType< Value >::Vector Values;

    Keys m_keys;
    Values m_values;

    int m_factor;
    int m_used;

    Hash hash;
    Valid valid;
    Equal equal;

    size_t usage() {
        return m_used;
    }

    size_t size() const { return m_keys.size(); }
    bool empty() const { return !m_used; }

    std::pair< Reference, int > _mergeInsert( Item item,
                                              Keys &keys,
                                              Values &values,
                                              hash_t hint = 0 )
    {
        assert( valid( item.first ) ); // ensure key validity
        int used = 0;
        hash_t _hash = hint ? hint : hash( item.first );
        hash_t off = 0, oldoff = 0, idx = 0;
        int mc = maxcollision();
        for ( int i = 0; i < mc; ++i ) {
            oldoff = off;
            off = _hash + i*i;

            assert( keys.size() == values.size() );

            idx = off % keys.size();

            if ( !valid( keys[ idx ] ) || equal( item.first, keys[ idx ] ) ) {
                if ( !valid( keys[ idx ] ) )
                    ++ used;
                Item use = valid( keys[ idx ] ) ?
                             std::make_pair( keys[ idx ], values[ idx ] ) :
                             item;
                keys[ idx ] = use.first;
                values[ idx ] = use.second;
                if ( !valid( keys[ idx ] ) )
                    -- used;
                return std::make_pair( Reference( keys[ idx ], idx ), used );
            }
        }
        for ( int i = 0; i < mc; ++i ) {
            size_t idx = ((_hash + i*i)%keys.size());
            assert( valid( keys[ idx ] ) );
            assert( ! equal( keys[ idx ], item.first ) );
        }
        return std::make_pair( Reference(), -2 );
    }

    Reference mergeInsert( Item item, hash_t hint = 0 ) {
        while ( true ) {
            std::pair< Reference, int > r = _mergeInsert( item, m_keys,
                                                          m_values, hint );
            if ( r.second == -1 )
                continue;
            if ( r.second == -2 ) {
                if ( usage() < size() / 10 ) {
                    std::cerr << "WARNING: suspiciously high collision rate, "
                              << "table growth triggered with usage < size/10"
                              << std::endl
                              << "size() = " << size()
                              << ", usage() = " << usage()
                              << ", maxcollision() = " << maxcollision()
                              << std::endl;
                }
                grow();
                continue;
            }

            m_used += r.second;

            if ( r.second > 0 && usage() > (size() * growthreshold()) / 100 )
                grow();

            return r.first;
        }
    }

    Reference insert( Key k, hash_t hint = 0 )
    {
        return mergeInsert( std::make_pair( k, Value() ), hint );
    }

    Reference insert( Item item, hash_t hint = 0 )
    {
        return mergeInsert( item, hint );
    }

    bool has( Key k, hash_t hint = 0 )
    {
        Reference r = get( k, hint );
        return valid( r.key );
    }

    Reference get( Key k, hash_t hint = 0 ) // const (bleh)
    {
        hash_t _hash = hint? hint : hash( k );
        size_t oldoff, off, idx;
        int mc = maxcollision();
        for ( int i = 0; i < mc; ++i ) {
            oldoff = off;
            off = _hash + i*i;
            idx = off % size();

            if ( !valid( m_keys[ idx ] ) ) {
                return Reference();
            }
            if ( equal( k, m_keys[ idx ] ) ) {
                return Reference( m_keys[ idx ], idx );
            }
        }
        // we can be sure that the element is not in the table *because*: we
        // never create chains longer than "mc", and if we haven't found the
        // key in this many steps, it can't be in the table
        return Reference();
    }

    Value &value( Reference r ) {
        return m_values[ r.offset ];
    }

    void setSize( size_t s )
    {
        m_keys.resize( s, Key() );
        m_values.resize( s, Value() );
    }

    template< typename F >
    void for_each( F f )
    {
        std::for_each( m_keys.begin(), m_keys.end(), f );
    }

    void clear() {
        m_used = 0;
        std::fill( m_keys.begin(), m_keys.end(), Key() );
        std::fill( m_values.begin(), m_values.end(), Value() );
    }

    void grow( int factor = 0 )
    {
        if ( factor == 0 )
            factor = m_factor;

        size_t _size = size();

        Keys keys; Values values;
        size_t _used = usage();

        assert_eq( m_keys.size(), m_values.size() );
        assert( factor > 1 );

        // +1 to keep the table size away from 2^n
        keys.resize( factor * m_keys.size() + 1, Key() );
        values.resize( factor * m_values.size() + 1, Value() );
        assert( keys.size() > size() );
        for ( size_t i = 0; i < m_keys.size(); ++i ) {
            if ( valid( m_keys[ i ] ) )
                _mergeInsert( std::make_pair( m_keys[ i ], m_values[ i ] ),
                              keys, values );
        }
        std::swap( keys, m_keys );
        std::swap( values, m_values );
        assert( usage() == _used );
        assert( size() > _size );
    }

    Reference operator[]( int off ) {
        return Reference( m_keys[ off ], off );
    }

    HashMap( Hash h = Hash(), Valid v = Valid(), Equal eq = Equal(),
             int initial = 32, int factor = 2 )
        : m_factor( factor ), hash( h ), valid( v ), equal( eq )
    {
        m_used = 0;
        setSize( initial );

        // assert that default key is invalid, this is assumed
        // throughout the code
        assert( !valid( Key() ) );
        assert( factor > 1 );
    }
};

}

namespace std {

template< typename Key, typename Value >
void swap( divine::HashMap< Key, Value > &a, divine::HashMap< Key, Value > &b )
{
    swap( a.m_keys, b.m_keys);
    swap( a.m_values, b.m_values );
    swap( a.m_used, b.m_used );
}

}

#endif
