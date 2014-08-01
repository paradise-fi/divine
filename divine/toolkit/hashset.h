// -*- C++ -*- (c) 2010-2014 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <cstdint>
#include <type_traits>

#include <wibble/singleton.h>
#include <wibble/sys/mutex.h>

#include <divine/toolkit/bitoperations.h>
#include <divine/toolkit/hashcell.h>

#ifndef DIVINE_HASHSET_H
#define DIVINE_HASHSET_H

namespace divine {

// default hash implementation
template< typename T >
struct default_hasher {};

template< template< typename > class C, typename T, typename F >
using FMap = C< typename std::result_of< F( T ) >::type >;

template< typename T >
struct NewType : wibble::Singleton< T >
{
    template< typename X > using FMap = NewType< X >;
    NewType() noexcept {}
    NewType( const T &t ) noexcept : wibble::Singleton< T >( t ) {}

    T &unwrap() { return *this->begin(); }
    const T &unwrap() const { return *this->begin(); }
};

template< typename T >
struct Wrapper : NewType< T >
{
    Wrapper() = default;
    Wrapper( const T &t ) : NewType< T >( t ) {}
    operator T() { return this->unwrap(); }
    T &value() { return this->unwrap(); }
    T &operator*() { return this->unwrap(); }
    T *operator->() { return &this->unwrap(); }
};

/*
template< typename S, typename F >
FMap< Wrapper, S, F > fmap( F f, Wrapper< S > n ) {
    return FMap< Wrapper, S, F >( f( n.unwrap() ) );
}
*/

template< template< typename > class C, typename S, typename F >
auto fmap( F, C< S > n ) -> decltype( FMap< C, S, F >( n.unwrap() ) ) {
    return FMap< C, S, F >( n.unwrap() );
}

template< typename T >
struct Found : Wrapper< T >
{
    bool _found;

    Found( const T &t, bool found ) : Wrapper< T >( t ), _found( found ) {}
    bool isnew() { return !_found; }
    bool found() { return _found; }

};

template< typename S, typename F >
FMap< Found, S, F > fmap( F f, Found< S > n ) {
    return FMap< Found, S, F >( f( n.unwrap() ), n._found );
}

template< typename T >
Found< T > isNew( const T &x, bool y ) {
    return Found< T >( x, !y );
}

template< typename Cell >
struct HashSetBase
{
    struct ThreadData {};

    using value_type = typename Cell::value_type;
    using Hasher = typename Cell::Hasher;

    static const unsigned cacheLine = 64; // bytes
    static const unsigned thresh = cacheLine / sizeof( Cell );
    static const unsigned threshMSB = bitops::compiletime::MSB( thresh );
    static const unsigned maxcollisions = 1 << 16; // 2^16
    static const unsigned growthreshold = 75; // percent

    Hasher hasher;

    struct iterator {
        Cell *_cell;
        bool _new;
        iterator( Cell *c = nullptr, bool n = false ) : _cell( c ), _new( n ) {}
        value_type *operator->() { return &(_cell->fetch()); }
        value_type &operator*() { return _cell->fetch(); }
        value_type copy() { return _cell->copy(); }
        bool valid() { return _cell; }
        bool isnew() { return _new; }
    };

    iterator end() { return iterator(); }

    static size_t index( hash64_t h, size_t i, size_t mask ) {
        h &= ~hash64_t( thresh - 1 );
        const unsigned Q = 1, R = 1;
        if ( i < thresh )
            return ( h + i ) & mask;
        else {
            size_t j = i & ( thresh - 1 );
            i = i >> threshMSB;
            size_t hop = ( (2 * Q + 1) * i + 2 * R * (i * i) ) << threshMSB;
            return ( h + j + hop ) & mask;
        }
    }

    HashSetBase( const Hasher &h ) : hasher( h ) {}
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
template< typename Cell >
struct _HashSet : HashSetBase< Cell >
{
    using Base = HashSetBase< Cell >;
    typedef std::vector< Cell > Table;
    _HashSet< Cell > &withTD( typename Base::ThreadData & ) { return *this; }

    using typename Base::iterator;
    using typename Base::value_type;
    using typename Base::Hasher;

    Table _table;
    int _used;
    int _bits;
    size_t _maxsize;
    bool _growing;

    size_t size() const { return _table.size(); }
    bool empty() const { return !_used; }

    int count( const value_type &i ) { return find( i ).valid(); }
    hash64_t hash( const value_type &i ) { return hash128( i ).first; }
    hash128_t hash128( const value_type &i ) { return this->hasher.hash( i ); }
    iterator insert( value_type i ) { return insertHinted( i, hash( i ) ); }

    template< typename T >
    iterator find( const T &i ) {
        return findHinted( i, hash( i ) );
    }

    template< typename T >
    iterator findHinted( const T &item, hash64_t hash )
    {
        size_t idx;
        for ( size_t i = 0; i < this->maxcollisions; ++i ) {
            idx = this->index( hash, i, _bits );

            if ( _table[ idx ].empty() )
                return this->end();

            if ( _table[ idx ].is( item, hash, this->hasher ) )
                return iterator( &_table[ idx ] );
        }
        // we can be sure that the element is not in the table *because*: we
        // never create chains longer than "mc", and if we haven't found the
        // key in this many steps, it can't be in the table
        return this->end();
    }

    iterator insertHinted( const value_type &i, hash64_t h ) {
        return insertHinted( i, h, _table, _used );
    }

    iterator insertHinted( const value_type &item, hash64_t h, Table &table, int &used )
    {
        if ( !_growing && size_t( _used ) > (size() / 100) * 75 )
            grow();

        size_t idx;
        for ( size_t i = 0; i < this->maxcollisions; ++i ) {
            idx = this->index( h, i, _bits );

            if ( table[ idx ].empty() ) {
                ++ used;
                table[ idx ].store( item, h );
                return iterator( &table[ idx ], true );
            }

            if ( table[ idx ].is( item, h, this->hasher ) )
                return iterator( &table[ idx ], false );
        }

        grow();

        return insertHinted( item, h, table, used );
    }

    void grow() {
        if ( 2 * size() >= _maxsize ) {
            std::cerr << "Sorry, ran out of space in the hash table!" << std::endl;
            abort();
        }

        if( _growing ) {
            std::cerr << "Oops, too many collisions during table growth." << std::endl;
            abort();
        }

        _growing = true;

        int used = 0;

        Table table;

        table.resize( 2 * size(), Cell() );
        _bits |= (_bits << 1); // unmask more

        for ( auto cell : _table ) {
            if ( cell.empty() )
                continue;
            insertHinted( cell.fetch(), cell.hash( this->hasher ),
                          table, used );
        }

        std::swap( table, _table );
        assert_eq( used, _used );

        _growing = false;
    }

    void setSize( size_t s )
    {
        _bits = 0;
        while ((s = s >> 1))
            _bits |= s;
        _table.resize( _bits + 1, Cell() );
    }

    void clear() {
        _used = 0;
        std::fill( _table.begin(), _table.end(), value_type() );
    }

    bool valid( int off ) {
        return !_table[ off ].empty();
    }

    value_type &operator[]( int off ) {
        return _table[ off ].fetch();
    }


    _HashSet() : _HashSet( Hasher() ) {}
    explicit _HashSet( Hasher h ) : _HashSet( h, 32 ) {}

    _HashSet( Hasher h, int initial )
        : Base( h ), _used( 0 ), _maxsize( -1 ), _growing( false )
    {
        setSize( initial );
    }
};

template< typename T, typename Hasher = default_hasher< T > >
using FastSet = _HashSet< FastCell< T, Hasher > >;

template< typename T, typename Hasher = default_hasher< T > >
using CompactSet = _HashSet< CompactCell< T, Hasher > >;

}

namespace std {

template< typename X >
void swap( divine::_HashSet< X > &a, divine::_HashSet< X > &b )
{
    swap( a._table, b._table );
    swap( a._used, b._used );
}

}

#endif
