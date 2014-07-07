// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Fast hash tables.
 */

/*
 * (c) 2010-2014 Petr Ročkai <me@mornfall.net>
 * (c) 2012-2014 Jiří Weiser <xweiser1@fi.muni.cz>
 * (c) 2013-2014 Vladimír Štill <xstill@fi.muni.cz>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-hash.h>
#include <brick-shmem.h>
#include <brick-bitlevel.h>
#include <brick-benchmark.h>

#include <type_traits>

#ifndef BRICK_HASHSET_H
#define BRICK_HASHSET_H

namespace brick {
namespace hashset {

using hash::hash64_t;
using hash::hash128_t;

/*
 * Hash table cell implementations (tables are represented as vectors of
 * cells).
 */

template< typename T, typename _Hasher >
struct CellBase
{
    using value_type = T;
    using Hasher = _Hasher;
};

template< typename T, typename Hasher >
struct FastCell : CellBase< T, Hasher >
{
    T _value;
    hash64_t _hash;

    template< typename Value >
    bool is( Value v, hash64_t hash, Hasher &h ) {
        return _hash == hash && h.equal( _value, v );
    }

    bool empty() { return !_hash; }
    void store( T bn, hash64_t hash ) {
        _hash = hash;
        _value = bn;
    }

    T &fetch() { return _value; }
    T copy() { return _value; }
    hash64_t hash( Hasher & ) { return _hash; }
};

template< typename T, typename Hasher >
struct CompactCell : CellBase< T, Hasher >
{
    T _value;

    template< typename Value >
    bool is( Value v, hash64_t, Hasher &h ) {
        return h.equal( _value, v );
    }

    bool empty() { return !_value; } /* meh */
    void store( T bn, hash64_t ) { _value = bn; }

    T &fetch() { return _value; }
    T copy() { return _value; }
    hash64_t hash( Hasher &h ) { return h.hash( _value ).first; }
};

template< typename T, typename Hasher >
struct FastAtomicCell : CellBase< T, Hasher >
{
    std::atomic< hash64_t > hashLock;
    T value;

    bool empty() { return hashLock == 0; }
    bool invalid() { return hashLock == 3; }

    /* returns old cell value */
    FastAtomicCell invalidate() {
        // wait for write to end
        hash64_t prev = 0;
        while ( !hashLock.compare_exchange_weak( prev, 0x3 ) ) {
            if ( prev == 3 )
                return FastAtomicCell( prev, value );
            prev &= ~(0x3); // clean flags
        }
        return FastAtomicCell( prev, value );
    }

    T &fetch() { return value; }
    T copy() { return value; }

    // TODO: this loses bits and hence doesn't quite work
    // hash64_t hash( Hasher & ) { return hashLock >> 2; }
    hash64_t hash( Hasher &h ) { return h.hash( value ).first; }

    // wait for another write; returns false if cell was invalidated
    bool wait() {
        while( hashLock & 1 )
            if ( invalid() )
                return false;
        return true;
    }

    bool tryStore( T v, hash64_t hash ) {
        hash |= 0x1;
        hash64_t chl = 0;
        if ( hashLock.compare_exchange_strong( chl, (hash << 2) | 1 ) ) {
            value = v;
            hashLock.exchange( hash << 2 );
            return true;
        }
        return false;
    }

    template< typename Value >
    bool is( Value v, hash64_t hash, Hasher &h ) {
        hash |= 0x1;
        if ( ( (hash << 2) | 1) != (hashLock | 1) )
            return false;
        if ( !wait() )
            return false;
        return h.equal( value, v );
    }

    FastAtomicCell() : hashLock( 0 ), value() {}
    FastAtomicCell( const FastAtomicCell & ) : hashLock( 0 ), value() {}
    FastAtomicCell( hash64_t hash, T value ) : hashLock( hash ), value( value ) { }
};

template< typename T, typename = void >
struct Tagged {
    T t;
    uint32_t _tag;

    static const int tagBits = 16;
    void setTag( uint32_t v ) { _tag = v; }
    uint32_t tag() { return _tag; }
    Tagged() noexcept : t(), _tag( 0 ) {}
    Tagged( const T &t ) : t( t ), _tag( 0 ) {}
};

template< typename T >
struct Tagged< T, typename std::enable_if< (T::tagBits > 0) >::type >
{
    T t;

    static const int tagBits = T::tagBits;
    void setTag( uint32_t value ) { t.setTag( value ); }
    uint32_t tag() { return t.tag(); }
    Tagged() noexcept : t() {}
    Tagged( const T &t ) : t( t ) {}
};

template< typename T, typename Hasher >
struct AtomicCell : CellBase< T, Hasher >
{
    std::atomic< Tagged< T > > value;

    static_assert( sizeof( std::atomic< Tagged< T > > ) == sizeof( Tagged< T > ),
                   "std::atomic< Tagged< T > > must be lock-free" );
    static_assert( Tagged< T >::tagBits > 0, "T has at least a one-bit tagspace" );

    bool empty() { return !value.load().t; }
    bool invalid() {
        Tagged< T > v = value.load();
        return (v.tag() == 0 && v.t) || (v.tag() != 0 && !v.t);
    }

    static hash64_t hashToTag( hash64_t hash, int bits = Tagged< T >::tagBits )
    {
        // use different part of hash than used for storing
        return ( hash >> ( sizeof( hash64_t ) * 8 - bits ) ) | 0x1;
    }

    /* returns old cell value */
    AtomicCell invalidate() {
        Tagged< T > v = value;
        v.setTag( v.tag() ? 0 : 1 ); // set tag to 1 if it was empty -> empty != invalid
        return AtomicCell( value.exchange( v ) );
    }

    Tagged< T > &deatomize() {
        value.load(); // fence
        return *reinterpret_cast< Tagged< T > * >( &value );
    }

    T &fetch() { return deatomize().t; }
    T copy() { Tagged< T > v = value; v.setTag( 0 ); return v.t; }
    bool wait() { return !invalid(); }

    void store( T bn, hash64_t hash ) {
        return tryStore( bn, hash );
    }

    bool tryStore( T b, hash64_t hash ) {
        Tagged< T > zero;
        Tagged< T > next( b );
        next.setTag( hashToTag( hash ) );
        auto rv = value.compare_exchange_strong( zero, next );
        return rv;
    }

    template< typename Value >
    bool is( Value v, hash64_t hash, Hasher &h ) {
        return value.load().tag() == hashToTag( hash ) &&
            h.equal( value.load().t, v );
    }

    hash64_t hash( Hasher &h ) { return h.hash( value.load().t ).first; }

    // AtomicCell &operator=( const AtomicCell &cc ) = delete;

    AtomicCell() : value() {}
    AtomicCell( const AtomicCell & ) : value() {}
    AtomicCell( Tagged< T > val ) : value() {
        value.store( val );
    }
};

// default hash implementation
template< typename T >
struct default_hasher {};

#if 0
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
#endif

template< typename Cell >
struct HashSetBase
{
    struct ThreadData {};

    using value_type = typename Cell::value_type;
    using Hasher = typename Cell::Hasher;

    static const unsigned cacheLine = 64; // bytes
    static const unsigned thresh = cacheLine / sizeof( Cell );
    static const unsigned threshMSB = bitlevel::compiletime::MSB( thresh );
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
        ASSERT_EQ( used, _used );

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
using Fast = _HashSet< FastCell< T, Hasher > >;

template< typename T, typename Hasher = default_hasher< T > >
using Compact = _HashSet< CompactCell< T, Hasher > >;

template< typename Cell >
struct _ConcurrentHashSet : HashSetBase< Cell >
{
    using Base = HashSetBase< Cell >;
    using typename Base::Hasher;
    using typename Base::value_type;
    using typename Base::iterator;

    enum class Resolution {
        Success, // the item has been inserted successfully
        Failed,  // cannot insert value, table growth has been triggered while
                 // we were looking for a free cell
        Found,   // item was already in the table
        NotFound,
        NoSpace, // there's is not enough space in the table
        Growing  // table is growing or was already resized, retry
    };

    struct _Resolution {
        Resolution r;
        Cell *c;

        _Resolution( Resolution r, Cell *c = nullptr ) : r( r ), c( c ) {}
    };

    using Insert = _Resolution;
    using Find = _Resolution;

    struct ThreadData {
        unsigned inserts;
        unsigned currentRow;

        ThreadData() : inserts( 0 ), currentRow( 0 ) {}
    };

    struct Row {
        std::atomic< Cell * > _data;
        size_t _size;

        size_t size() const { return _size; }

        void size( size_t s ) {
            ASSERT( empty() );
            _size = s;
        }

        bool empty() const { return begin() == nullptr; }

        void resize( size_t n ) {
            Cell *old = _data.exchange( new Cell[ n ] );
            _size = n;
            delete[] old;
        }

        void free() {
            Cell *old = _data.exchange( nullptr );
            _size = 0;
            delete[] old;
        }

        Cell &operator[]( size_t i ) {
            return _data.load( std::memory_order_relaxed )[ i ];
        }

        Cell *begin() {
            return _data.load( std::memory_order_relaxed );
        }
        Cell *begin() const {
            return _data.load( std::memory_order_relaxed );
        }

        Cell *end() {
            return begin() + size();
        }
        Cell *end() const {
            return begin() + size();
        }

        Row() : _data( nullptr ), _size( 0 ) {}
        ~Row() { free(); }
    };

    static const unsigned segmentSize = 1 << 16;// 2^16 = 65536
    static const unsigned syncPoint = 1 << 10;// 2^10 = 1024

    struct Data
    {
        Hasher hasher;
        std::vector< Row > table;
        std::vector< std::atomic< unsigned short > > tableWorkers;
        std::atomic< unsigned > currentRow;
        std::atomic< int > availableSegments;
        std::atomic< unsigned > doneSegments;
        std::atomic< size_t > used;
        std::atomic< bool > growing;

        Data( const Hasher &h, unsigned maxGrows )
            : hasher( h ), table( maxGrows ), tableWorkers( maxGrows ), currentRow( 0 ),
              availableSegments( 0 ), used( 0 ), growing( false )
        {}
    };

    Data _d;
    ThreadData _global; /* for single-thread access */

    struct WithTD
    {
        using iterator = typename Base::iterator;
        using value_type = typename Base::value_type;

        Data &_d;
        ThreadData &_td;
        WithTD( Data &d, ThreadData &td ) : _d( d ), _td( td ) {}

        size_t size() { return current().size(); }
        Row &current() { return _d.table[ _d.currentRow ]; }
        Row &current( unsigned index ) { return _d.table[ index ]; }
        bool changed( unsigned row ) { return row < _d.currentRow || _d.growing; }

        iterator insert( value_type x ) {
            return insertHinted( x, _d.hasher.hash( x ).first );
        }

        template< typename T >
        iterator find( T x ) {
            return findHinted( x, _d.hasher.hash( x ).first );
        }

        int count( value_type x ) {
            return find( x ).valid() ? 1 : 0;
        }

        size_t nextSize( size_t s ) {
            if ( s < 512 * 1024 )
                return s * 16;
            if ( s < 16 * 1024 * 1024 )
                return s * 8;
            if ( s < 32 * 1024 * 1024 )
                return s * 4;
            return s * 2;
        }

        iterator insertHinted( value_type x, hash64_t h )
        {
            while ( true ) {
                Insert ir = insertCell< false >( x, h );
                switch ( ir.r ) {
                    case Resolution::Success:
                        increaseUsage();
                        return iterator( ir.c, true );
                    case Resolution::Found:
                        return iterator( ir.c, false );
                    case Resolution::NoSpace:
                        if ( grow( _td.currentRow + 1 ) ) {
                            ++_td.currentRow;
                            break;
                        }
                    case Resolution::Growing:
                        helpWithRehashing();
                        updateIndex( _td.currentRow );
                        break;
                    default:
                        ASSERT_UNREACHABLE("impossible result from insertCell");
                }
            }
            ASSERT_UNREACHABLE("broken loop");
        }

        template< typename T >
        iterator findHinted( T x, hash64_t h ) {
            while ( true ) {
                Find fr = findCell( x, h, _td.currentRow );
                switch ( fr.r ) {
                    case Resolution::Found:
                        return iterator( fr.c );
                    case Resolution::NotFound:
                        return iterator();
                    case Resolution::Growing:
                        helpWithRehashing();
                        updateIndex( _td.currentRow );
                        break;
                    default:
                        ASSERT_UNREACHABLE("impossible result from findCell");
                }
            }
            ASSERT_UNREACHABLE("broken loop");
        }

        template< typename T >
        Find findCell( T v, hash64_t h, unsigned rowIndex )
        {
            if ( changed( rowIndex ) )
                return Find( Resolution::Growing );

            Row &row = current( rowIndex );

            if ( row.empty() )
                return Find( Resolution::NotFound );

            const size_t mask = row.size() - 1;

            for ( size_t i = 0; i < Base::maxcollisions; ++i ) {
                if ( changed( rowIndex ) )
                    return Find( Resolution::Growing );

                Cell &cell = row[ Base::index( h, i, mask ) ];
                if ( cell.empty() )
                    return Find( Resolution::NotFound );
                if ( cell.is( v, h, _d.hasher ) )
                    return Find( Resolution::Found, &cell );
                if ( cell.invalid() )
                    return Find( Resolution::Growing );
            }
            return Find( Resolution::NotFound );
        }

        template< bool force >
        Insert insertCell( value_type x, hash64_t h )
        {
            Row &row = current( _td.currentRow );
            if ( !force ) {
                // read usage first to guarantee usage <= size
                size_t u = _d.used.load();
                // usage >= 75% of table size
                // usage is never greater than size
                if ( row.empty() || double( row.size() ) <= double( 4 * u ) / 3 )
                    return Insert( Resolution::NoSpace );
                if ( changed( _td.currentRow ) )
                    return Insert( Resolution::Growing );
            }

            ASSERT( !row.empty() );
            const size_t mask = row.size() - 1;

            for ( size_t i = 0; i < Base::maxcollisions; ++i )
            {
                Cell &cell = row[ Base::index( h, i, mask ) ];

                if ( cell.empty() ) {
                    if ( cell.tryStore( x, h ) )
                        return Insert( Resolution::Success, &cell );
                    if ( !force && changed( _td.currentRow ) )
                        return Insert( Resolution::Growing );
                }
                if ( cell.is( x, h, _d.hasher ) )
                    return Insert( Resolution::Found, &cell );

                if ( !force && changed( _td.currentRow ) )
                    return Insert( Resolution::Growing );
            }
            return Insert( Resolution::NoSpace );
        }

        bool grow( unsigned rowIndex )
        {
            ASSERT( rowIndex );

            if ( rowIndex >= _d.table.size() )
                ASSERT_UNREACHABLE( "out of growth space" );

            if ( _d.currentRow >= rowIndex )
                return false;

            while ( _d.growing.exchange( true ) ); // acquire growing lock

            if ( _d.currentRow >= rowIndex ) {
                _d.growing.exchange( false ); // release the lock
                return false;
            }

            Row &row = current( rowIndex - 1 );
            _d.table[ rowIndex ].resize( nextSize( row.size() ) );
            _d.currentRow.exchange( rowIndex );
            _d.tableWorkers[ rowIndex ] = 1;
            _d.doneSegments.exchange( 0 );

            // current row is fake, so skip the rehashing
            if ( row.empty() ) {
                rehashingDone();
                return true;
            }

            const unsigned segments = std::max( row.size() / segmentSize, size_t( 1 ) );
            _d.availableSegments.exchange( segments );

            while ( rehashSegment() );

            return true;
        }

        void helpWithRehashing() {
            if ( _d.growing )
                while( rehashSegment() );
            while( _d.growing );
        }

        void rehashingDone() {
            releaseRow( _d.currentRow - 1 );
            _d.growing.exchange( false ); /* done */
        }

        bool rehashSegment() {
            int segment;
            if ( _d.availableSegments <= 0 )
                return false;
            if ( ( segment = --_d.availableSegments ) < 0 )
                return false;

            Row &row = current( _d.currentRow - 1 );
            size_t segments = std::max( row.size() / segmentSize, size_t( 1 ) );
            auto it = row.begin() + segmentSize * segment;
            auto end = it + segmentSize;
            if ( end > row.end() )
                end = row.end();
            ASSERT( it < end );

            ThreadData td;
            td.currentRow = _d.currentRow;

            // every cell has to be invalidated
            for ( ; it != end; ++it ) {
                Cell old = it->invalidate();
                if ( old.empty() || old.invalid() )
                    continue;

                value_type value = old.fetch();
                Resolution r = WithTD( _d, td ).insertCell< true >( value, old.hash( _d.hasher ) ).r;
                switch( r ) {
                    case Resolution::Success:
                        break;
                    case Resolution::NoSpace:
                        ASSERT_UNREACHABLE( "ran out of space during growth" );
                    default:
                        ASSERT_UNREACHABLE( "internal error" );
                }
            }

            if ( ++_d.doneSegments == segments )
                rehashingDone();

            return segment > 0;
        }

        void updateIndex( unsigned &index ) {
            unsigned row = _d.currentRow;
            if ( row != index ) {
                releaseRow( index );
                acquireRow( row );
                index = row;
            }
        }

        void releaseRow( unsigned index ) {
            // special case - zero index
            if ( !_d.tableWorkers[ index ] )
                return;
            // only last thread releases memory
            if ( !--_d.tableWorkers[ index ] )
                _d.table[ index ].free();
        }

        void acquireRow( unsigned &index ) {
            unsigned short refCount	= _d.tableWorkers[ index ];

            do {
                if ( !refCount ) {
                    index = _d.currentRow;
                    refCount = _d.tableWorkers[ index ];
                    continue;
                }

                if (_d.tableWorkers[ index ].compare_exchange_weak( refCount, refCount + 1 ))
                    break;
            } while( true );
        }

        void increaseUsage() {
            if ( ++_td.inserts == syncPoint ) {
                _d.used += syncPoint;
                _td.inserts = 0;
            }
        }

    };

    WithTD withTD( ThreadData &td ) { return WithTD( _d, td ); }

    explicit _ConcurrentHashSet( Hasher h = Hasher(), unsigned maxGrows = 64 )
        : Base( h ), _d( h, maxGrows )
    {
        setSize( 16 ); // by default
    }

    /* XXX only usable before the first insert; rename? */
    void setSize( size_t s ) {
        s = bitlevel::fill( s - 1 ) + 1;
        _d.table[ 0 ].size( s / 2 );
    }

    hash64_t hash( const value_type &t ) { return hash128( t ).first; }
    hash128_t hash128( const value_type &t ) { return _d.hasher.hash( t ); }
    iterator insert( const value_type &t ) { return withTD( _global ).insert( t ); }
    int count( const value_type &t ) { return withTD( _global ).count( t ); }
    size_t size() { return withTD( _global ).size(); }

    _ConcurrentHashSet( const _ConcurrentHashSet & ) = delete;
    _ConcurrentHashSet &operator=( const _ConcurrentHashSet & )= delete;

    /* multiple threads may use operator[], but not concurrently with insertions */
    value_type operator[]( size_t index ) { // XXX return a reference
        return _d.table[ _d.currentRow ][ index ].fetch();
    }

    bool valid( size_t index ) {
        return !_d.table[ _d.currentRow ][ index ].empty();
    }
};

template< typename T, typename Hasher = default_hasher< T > >
using FastConcurrent = _ConcurrentHashSet< FastAtomicCell< T, Hasher > >;

template< typename T, typename Hasher = default_hasher< T > >
using Concurrent = _ConcurrentHashSet< AtomicCell< T, Hasher > >;

}
}

#include <brick-hlist.h>
#include <unordered_set>

namespace brick_test {
namespace hashset {

using namespace ::brick::hashset;

template< template< typename > class HS >
struct Sequential
{
    TEST(basic) {
        HS< int > set;

        ASSERT( !set.count( 1 ) );
        ASSERT( set.insert( 1 ).isnew() );
        ASSERT( set.count( 1 ) );

        unsigned count = 0;
        for ( unsigned i = 0; i != set.size(); ++i )
            if ( set[ i ] )
                ++count;

        ASSERT_EQ( count, 1u );
    }

    TEST(stress) {
        HS< int > set;
        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            ASSERT( set.count( i ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            ASSERT( set.count( i ) );
        }
    }

    TEST(set) {
        HS< int > set;

        for ( int i = 1; i < 32*1024; ++i ) {
            ASSERT( !set.count( i ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            ASSERT( set.count( i ) );
            ASSERT( !set.count( i + 1 ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            ASSERT( set.count( i ) );
        }

        for ( int i = 32*1024; i < 64 * 1024; ++i ) {
            ASSERT( !set.count( i ) );
        }
    }
};

template< template< typename > class HS >
struct Parallel
{
    struct Insert : shmem::Thread {
        HS< int > *_set;
        typename HS< int >::ThreadData td;
        int from, to;
        bool overlap;

        void main() {
            auto set = _set->withTD( td );
            for ( int i = from; i < to; ++i ) {
                set.insert( i );
                ASSERT( !set.insert( i ).isnew() );
                if ( !overlap && i < to - 1 )
                    ASSERT( !set.count( i + 1 ) );
            }
        }
    };

    TEST(insert) {
        HS< int > set;
        set.setSize( 4 * 1024 );
        Insert a;
        a._set = &set;
        a.from = 1;
        a.to = 32 * 1024;
        a.overlap = false;
        a.main();
        for ( int i = 1; i < 32*1024; ++i )
            ASSERT( set.count( i ) );
    }

    static void _par( HS< int > *set, int f1, int t1, int f2, int t2 )
    {
        Insert a, b;

        a.from = f1;
        a.to = t1;
        b.from = f2;
        b.to = t2;
        a._set = set;
        b._set = set;
        a.overlap = b.overlap = (t1 > f2);

        a.start();
        b.start();
        a.join();
        b.join();
    }

    static void _multi( HS< int > *set, std::size_t count, int from, int to )
    {
        Insert *arr = new Insert[ count ];

        for ( std::size_t i = 0; i < count; ++i ) {
            arr[ i ].from = from;
            arr[ i ].to = to;
            arr[ i ]._set = set;
            arr[ i ].overlap = true;
        }

        for ( std::size_t i = 0; i < count; ++i )
            arr[ i ].start();

        for ( std::size_t i = 0; i < count; ++i )
            arr[ i ].join();

        delete[] arr;
    }

    TEST(multi)
    {
        HS< int > set;
        set.setSize( 4 * 1024 );
        _multi( &set, 10, 1, 32 * 1024 );

        for  ( int i = 1; i < 32 * 1024; ++i )
            ASSERT( set.count( i ) );

        int count = 0;
        std::set< int > s;
        for ( size_t i = 0; i != set.size(); ++i ) {
            if ( set[ i ] ) {
                if ( s.find( set[ i ] ) != s.end() )
                    std::cout << set[ i ] << std::endl;
                else
                    s.insert( set[ i ] );
                ++count;
            }
        }
        ASSERT_EQ( count, 32 * 1024 - 1 );
    }

    TEST(stress)
    {
        HS< int > set;

        set.setSize( 4 * 1024 );
        _par( &set, 1, 16*1024, 8*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            ASSERT( set.count( i ) );
    }

    TEST(set) {
        HS< int > set;
        set.setSize( 4 * 1024 );
        for ( int i = 1; i < 32*1024; ++i )
            ASSERT( !set.count( i ) );

        _par( &set, 1, 16*1024, 16*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            ASSERT_EQ( i, i * set.count( i ) );

        for ( int i = 32*1024; i < 64 * 1024; ++i )
            ASSERT( !set.count( i ) );
    }
};

struct test_hasher {
    template< typename X >
    test_hasher( X& ) { }
    test_hasher() = default;
    hash128_t hash( int t ) const { return std::make_pair( t, t ); }
    bool valid( int t ) const { return t != 0; }
    bool equal( int a, int b ) const { return a == b; }
};

template< typename T > using CS = Compact< T, test_hasher >;
template< typename T > using FS = Fast< T, test_hasher >;
template< typename T > using ConCS = Concurrent< T, test_hasher >;
template< typename T > using ConFS = FastConcurrent< T, test_hasher >;

/* instantiate the testcases */
template struct Sequential< CS >;
template struct Sequential< FS >;
template struct Sequential< ConCS >;
template struct Sequential< ConFS >;
template struct Parallel< ConCS >;
template struct Parallel< ConFS >;

/* Benchmarks */

template< typename HS >
struct RandomInsert : shmem::Thread {
    HS *_set;
    typename HS::ThreadData td;
    int count;
    struct random_data rand;
    char statebuf[256];

    RandomInsert() {
        memset(&rand, 0, sizeof(rand));
        initstate_r( 0, statebuf, 128, &rand );
    }

    void main() {
        auto set = _set->withTD( td );
        int v;
        for ( int i = 0; i < count; ++i ) {
            random_r( &rand, &v );
            set.insert( v );
        }
    };
};

Axis axis_items( int min = 16, int max = 16 * 1024 ) {
    Axis a;
    a.active = true;
    a.name = "items";
    a.log = true;
    a.step = sqrt(sqrt(2));
    a.normalize = true;
    a.unit = "k";
    a.unit_div =    1000;
    a.min = min * 1000;
    a.max = max * 1000;
    return a;
}

Axis axis_threads( int max = 16 ) {
    Axis a;
    a.active = true;
    a.name = "threads";
    a.unit = "";
    a.min = 1;
    a.max = max;
    a.step = 1;
    return a;
}

Axis axis_reserve( int max = 200, int step = 50 )
{
    Axis a;
    a.active = true;
    a.name = "reserve";
    a.unit = "%";
    a.min = 0;
    a.max = max;
    a.step = step;
    return a;
}

Axis axis_types( int count )
{
    Axis a;
    a.active = true;
    a.name = "type";
    a.unit = "";
    a.min = 0;
    a.max = count - 1;
    a.step = 1;
    return a;
}

template< typename T > struct TN {};

template< typename Ts >
struct Run : BenchmarkGroup
{
    template< typename >
    std::string describe( hlist::not_preferred ) { return ""; }

    template< typename Tss = Ts, typename = typename Tss::Head >
    std::string describe( hlist::preferred = hlist::preferred() )
    {
        std::string t = describe< typename Tss::Tail >( hlist::preferred() );
        return std::string( "type=" ) + TN< typename Tss::Head >::n() +
               (t.empty() ? "" : (" " + t));
    }

    template< template< typename > class, typename Self, int, typename >
    static void run( Self *, hlist::not_preferred ) {
        ASSERT_UNREACHABLE( "brick_test::hashset::Run fell off the cliff" );
    }

    template< template< typename > class RI, typename Self, int id = 0,
              typename Tss = Ts, typename = typename Tss::Head >
    static void run( Self *self, hlist::preferred = hlist::preferred() )
    {
        if ( self->type() == id ) {
            /* std::cerr << "running " << id << " with " << self->items() << " items, " << self->threads()
                      << " threads and " << self->reserve() << " reserve" <<
                      std::endl; */
            RI< typename Tss::Head >()( self->items(), self->threads(), self->reserve() );
        } else
            run< RI, Self, id + 1, typename Tss::Tail >( self, hlist::preferred() );
    }

    int type() { return 0; } // default
};

template< int _threads, typename T >
struct ItemsVsReserve : Run< hlist::TypeList< T > >
{
    ItemsVsReserve() {
        this->x = axis_items();
        this->y = axis_reserve();
    }

    std::string fixed() {
        std::stringstream s;
        s << "threads=" << _threads;
        return s.str();
    }

    int threads() { return _threads; }
    int items() { return this->p; }
    double reserve() { return this->q / 100; }
};

template< int _reserve, typename T >
struct ItemsVsThreads : Run< hlist::TypeList< T > >
{
    ItemsVsThreads() {
        this->x = axis_items();
        this->y = axis_threads();
    }

    std::string fixed() {
        std::stringstream s;
        s << "reserve=" << _reserve;
        return s.str();
    }

    int threads() { return this->q; }
    int items() { return this->p; }
    double reserve() { return _reserve / 100.0; }
};

template< int _items, typename T >
struct ThreadsVsReserve : Run< hlist::TypeList< T > >
{
    ThreadsVsReserve() {
        this->x = axis_threads();
        this->y = axis_reserve();
    }

    std::string fixed() {
        std::stringstream s;
        s << "items=" << _items << "k";
        return s.str();
    }

    int threads() { return this->p; }
    int reserve() { return this->q; }
    int items() { return _items * 1000; }
};

template< int _items, int _reserve, int _threads, typename... Ts >
struct ThreadsVsTypes : Run< hlist::TypeList< Ts... > >
{
    ThreadsVsTypes() {
        this->x = axis_threads( _threads );
        this->y = axis_types( sizeof...( Ts ) );
    }

    std::string fixed() {
        std::stringstream s;
        s << "items=" << _items << "k reserve=" << _reserve;
        return s.str();
    }

    int threads() { return this->p; }
    double reserve() { return _reserve / 100.0; }
    int items() { return _items * 1000; }
    int type() { return this->q; }
};

template< typename Param >
struct Bench : Param
{
    std::string describe() {
        return "category=hashset " + Param::describe() + " " +
               Param::fixed() + " " + this->describe_axes();
    }

    template< typename T >
    struct _RandomInsert {
        void operator()( int items, int threads, double reserve )
        {
            T t;

            if ( reserve > 0 )
                t.setSize( items * reserve );

            RandomInsert< T > *ri = new RandomInsert< T >[ threads ];

            for ( int i = 0; i < threads; ++i ) {
                ri[i].count = items / threads;
                ri[i]._set = &t;
            }

            for ( int i = 0; i < threads; ++i )
                ri[i].start();
            for ( int i = 0; i < threads; ++i )
                ri[i].join();
        }
    };

    BENCHMARK(random_insert)
    {
        this->template run< _RandomInsert >( this );
    }
};

template< typename T >
struct wrap : T {
    struct ThreadData {};
    wrap< T > &withTD( ThreadData & ) { return *this; }
    void setSize( int s ) { T::rehash( s ); }
};

using A = wrap< std::unordered_set< int > >;
using B = Compact< int, test_hasher >;
using C = Fast< int, test_hasher >;
using D = Concurrent< int, test_hasher >;
using E = FastConcurrent< int, test_hasher >;

template<> struct TN< A > { static const char *n() { return "unordered_set"; } };
template<> struct TN< B > { static const char *n() { return "CompactSet"; } };
template<> struct TN< C > { static const char *n() { return "FastSet"; } };
template<> struct TN< D > { static const char *n() { return "ConcurrentSet"; } };
template<> struct TN< E > { static const char *n() { return "FastConcurrentSet"; } };

template struct Bench< ThreadsVsTypes< 1024, 50, 16, D, E > >;

template struct Bench< ItemsVsReserve< 1, A > >;
template struct Bench< ItemsVsReserve< 1, B > >;
template struct Bench< ItemsVsReserve< 1, C > >;
template struct Bench< ItemsVsReserve< 1, D > >;
template struct Bench< ItemsVsReserve< 2, D > >;
template struct Bench< ItemsVsReserve< 4, D > >;
template struct Bench< ItemsVsReserve< 1, E > >;
template struct Bench< ItemsVsReserve< 2, E > >;
template struct Bench< ItemsVsReserve< 4, E > >;

}
}

#endif

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
