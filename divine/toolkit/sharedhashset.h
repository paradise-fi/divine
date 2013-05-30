// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <atomic>
#include <stdint.h>
#include <tuple>
#include <algorithm>
#include <wibble/sys/mutex.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/shmem.h>
#include <divine/toolkit/bitoperations.h>

#ifndef DIVINE_SHAREDHASHSET_H
#define DIVINE_SHAREDHASHSET_H

namespace divine {

template< typename Item >
struct HashCell {
    std::atomic< hash_t > hashLock;
    Item value;

    bool empty() { return hashLock == 0; }
    bool invalid() { return hashLock == 1; }
    void invalidate() { hashLock.exchange( 1 ); }

    Item fetch() { return value; }

    template< typename Hasher >
    hash_t hash( Hasher & ) { return hashLock >> 1; }

    // wait until another writing ends
    // returns false if cell was invalidated
    bool wait() {
        while( hashLock & 1 )
            if ( invalid() )
                return false;
        return true;
    }

    template< typename GrowingGuard >
    bool tryStore( Item v, hash_t hash, GrowingGuard g ) {
        hash_t chl = 0;
        if ( hashLock.compare_exchange_strong( chl, (hash << 1) | 1 ) ) {
            if ( g() ) {
                invalidate();
                return false;
            }
            value = v;
            hashLock.exchange( hash << 1 );
            return true;
        }
        return false;
    }

    // it has to call wait method
    template< typename Value, typename Hasher >
    bool is( Value v, hash_t hash, Hasher &h ) {
        if ( ( (hash << 1) | 1) != (hashLock | 1) )
            return false;
        if ( !wait() )
            return false;
        return h.equal( value, v );
    }

    HashCell() : hashLock( 0 ), value() {}
    HashCell( HashCell & ) : hashLock( 0 ), value() {}
};

template<
    typename _Item,
    typename _Hasher = divine::default_hasher< _Item >,
    typename Cell = HashCell< _Item >
    >
struct SharedHashSetImplementation {

    using Item = _Item;
    using Hasher = _Hasher;
    using TableItem = Item;
    using InsertItem = Item;
    using StoredItem = Item;
    using InputHasher = Hasher;

    enum class InsertResolution {
        Success, // the item has been inserted successfully
        Failed,  // cannot insert value, table growth has been triggered while
                 // we were looking for a free cell
        Found,   // item was already in the table
        NoSpace, // there's is not enough space in the table
        Growing  // table is growing or was already resized, retry
    };
    enum class FindResolution { Found, Growing, NotFound };

    struct ThreadData {
        unsigned currentRow;

        ThreadData() : currentRow( 0 ) {}
    };

    using Row = std::vector< Cell >;

    static const unsigned cacheLine = 64;// Bytes
    static const unsigned thresh = cacheLine / sizeof( Cell );
    static const unsigned threshMSB = bitops::compiletime::MSB( thresh );
    static const unsigned maxCollisions = thresh * 16;
    static const unsigned segmentSize = 1 << 16;// 2^16

    std::vector< Row * > table;
    std::vector< std::atomic< unsigned short > > tableWorkers;
    std::atomic< unsigned > currentRow;
    std::atomic< int > availableSegments;
    std::atomic< unsigned > doneSegments;
    std::atomic< bool > growing;
    Hasher hasher;

    /* can be called while other threads insert, but may give outdated info
     * (less than the current size) */
    size_t size() const {
        return table[ currentRow ]->size();
    }

    explicit SharedHashSetImplementation( Hasher h = Hasher(), unsigned maxGrows = 64 )
        : table( maxGrows, nullptr ), tableWorkers( maxGrows ), currentRow( 1 ),
          availableSegments( 0 ), growing( false ), hasher( h )
    {}

    ~SharedHashSetImplementation() {
        for ( unsigned i = 0; i != table.size(); ++i ) {
            if ( table[ i ] )
                delete table[ i ];
        }
    }

    /* only usable before the first insert */
    void setSize( size_t s ) {
        s = bitops::fill( s - 1 ) + 1;
        if ( !table[ 1 ] )
            table[ 1 ] = new Row( s );
    }

    void setHasher( Hasher h ) {
        hasher = h;
    }

    /* TODO factor this out into a place where HashSet can use it, too */
    size_t index( hash_t h, size_t i, size_t mask ) const {
        h &= ~hash_t( thresh - 1 );
        const unsigned Q = 1, R = 1;
        if ( i < thresh )// for small i use trivial computation
            return (h + i) & mask;
        else {
            size_t j = i & ( thresh - 1 );
            i = i >> threshMSB;
            size_t hop = ( (2 * Q + 1) * i + 2 * R * (i * i) ) << threshMSB;
            return (h + j + hop ) & mask;
        }
    }

    template< typename TD >
    std::tuple< Item, bool > insert( Item x, TD &tld ) {
        return insertHinted( x, hasher.hash( x ), tld );
    }

    template< typename TD >
    std::tuple< Item, bool > insertHinted( Item x, hash_t h, TD &tld ) {
        while ( true ) {
            switch( insertCell( x, h << 1, tld.currentRow ) ) {
                case InsertResolution::Success:
                    return std::make_tuple( x, true );
                case InsertResolution::Found:
                    return std::make_tuple( x, false );
                case InsertResolution::NoSpace:
                    if ( grow( tld.currentRow + 1 ) ) {
                        releaseRow( tld.currentRow );
                        ++tld.currentRow;
                        break;
                    }
                case InsertResolution::Growing:
                    updateIndex( tld.currentRow );
                    break;
                default:
                    __builtin_unreachable();
            }
        }
        __builtin_unreachable();
    }

    template< typename T, typename TD >
    std::tuple< Item, bool > getHinted( T x, hash_t h, TD &tld ) {
        auto pair = std::make_pair( Item(), x );
        while ( true ) {
            switch( getCell( pair, h << 1, tld.currentRow ) ) {
                case FindResolution::Found:
                    return std::make_tuple( pair.first, true );
                case FindResolution::NotFound:
                    return std::make_tuple( pair.first, false );
                case FindResolution::Growing:
                    updateIndex( tld.currentRow );
                    break;
                default:
                    __builtin_unreachable();
            }
        }
        __builtin_unreachable();
    }

    /* multiple threads may use operator[], but not concurrently with insertions! */
    Item operator[]( size_t index ) {
        return (*table[ currentRow ])[ index ].value;
    }

    SharedHashSetImplementation( const SharedHashSetImplementation & ) = delete;
    SharedHashSetImplementation &operator=( const SharedHashSetImplementation & )= delete;

protected:

    template< typename T >
    FindResolution getCell( std::pair< Item, T > &pair, hash_t h, unsigned rowIndex ) {

        while( growing ) {
            // help with moving table
            while( rehashSegment() );
        }
        if ( changed( rowIndex ) )
            return FindResolution::Growing;

        Row &row = *table[ rowIndex ];
        const size_t mask = row.size() - 1;

        for ( size_t i = 0; i < maxCollisions; ++i ) {
            if ( changed( rowIndex ) )
                return FindResolution::Growing;

            Cell &cell = row[ index( h, i, mask ) ];
            if ( cell.empty() )
                return FindResolution::NotFound;
            if ( cell.is( pair.second, h, hasher ) ) {
                pair.first = cell.fetch();
                return FindResolution::Found;
            }
            if ( cell.invalid() )
                return FindResolution::Growing;
        }
        return FindResolution::NotFound;
    }

    InsertResolution insertCell( Item &x, hash_t h, unsigned rowIndex, bool force = false ) {
        if ( !force ) {
            while( growing ) // help with rehashing
                while ( rehashSegment() );

            if ( changed( rowIndex ) )
                return InsertResolution::Growing;
        }

        Row &row = *table[ rowIndex ];
        assert( !row.empty() );
        const size_t mask = row.size() - 1;

        const size_t acceptableCollisions = force ? 65536 : maxCollisions;
        for ( size_t i = 0; i < acceptableCollisions; ++i ) {

            Cell &cell = row[ index( h, i, mask ) ];

            if ( cell.empty() ) {
                if ( cell.tryStore( x, h, [&]() -> bool {
                        return !force && (this->changed( rowIndex ) || this->growing );
                    } ) )
                    return InsertResolution::Success;
                if ( !force && ( growing || changed( rowIndex ) ) )
                    return InsertResolution::Growing;
            }
            if ( cell.is( x, h, hasher ) ) {
                x = cell.fetch();
                return InsertResolution::Found;
            }
            if ( !force && ( growing || changed( rowIndex ) ) )
                return InsertResolution::Growing;
        }
        return InsertResolution::NoSpace;
    }

    inline bool changed( unsigned rowIndex ) {
        return rowIndex < currentRow;
    }

    bool grow( unsigned rowIndex ) {
        assert( rowIndex );

        if ( rowIndex >= table.size() ) {
            std::cerr << "no enough memory" << std::endl;
            abort();
            __builtin_unreachable();
        }

        if ( currentRow >= rowIndex )
            return false;

        while ( growing.exchange( true ) ); // acquire growing lock

        if ( currentRow >= rowIndex ) {
            growing.exchange( false ); // release the lock
            return false;
        }

        Row &row = *table[ rowIndex - 1 ];
        table[ rowIndex ] = new Row( row.size() * 2 );
        currentRow.exchange( rowIndex );
        tableWorkers[ rowIndex ] = 1;
        doneSegments.exchange( 0 );
        const unsigned segments = std::max( row.size() / segmentSize, size_t( 1 ) );// if size < segmentSize
        availableSegments.exchange( segments );

        while ( rehashSegment() );

        // release memory after processing all segments
        while( doneSegments != segments );

        return true;
    }

    // thread processing last segment releases growing mutex
    bool rehashSegment() {
        int segment;
        if ( availableSegments <= 0 )
            return false;
        if ( ( segment = --availableSegments ) < 0 )
            return false;

        Row &row = *table[ currentRow - 1 ];
        size_t segments = std::max( row.size() / segmentSize, size_t( 1 ) );
        auto it = row.begin() + segmentSize * segment;
        auto end = it + segmentSize;
        if ( end > row.end() )
            end = row.end();
        assert( it < end );

        // every cell has to be invalitated
        for ( ; it != end; it->invalidate(), ++it ) {
            if ( it->empty() )
                continue;
            if ( !it->wait() )
                continue;
            Item value = it->fetch();
            if ( insertCell( value, it->hash( hasher ), currentRow, true )
                == InsertResolution::NoSpace ) {
                std::cerr << "no enough space during growing" << std::endl;
                abort();
            }
        }

        if ( ++doneSegments == segments )
            growing.exchange( false );
        return segment > 0;
    }

    void updateIndex( unsigned &index ) {
        unsigned row = currentRow;
        if ( row != index ) {
            releaseRow( index );
            acquireRow( row );
            index = row;
        }
    }

    void releaseRow( unsigned index ) {
        // special case - zero index
        if ( !tableWorkers[ index ] )
            return;
        // only last thread releases memory
        if ( !--tableWorkers[ index ] ) {
            delete table[ index ];
            table[ index ] = nullptr;
        }
    }

    void acquireRow( unsigned index ) {
        do {
            if ( 1 < ++tableWorkers[ index ] )
                break;
            if ( table[ index ] && index == 1 )
                break;
            --tableWorkers[ index ];
            index = currentRow;
        } while ( true );
    }
};

template< typename Item, typename Hasher = divine::default_hasher< Item > >
using SharedHashSet = SharedHashSetImplementation< Item, Hasher >;

}

#endif
