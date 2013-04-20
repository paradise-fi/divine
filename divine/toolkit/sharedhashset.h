// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <atomic>
#include <stdint.h>
#include <tuple>
#include <wibble/sys/mutex.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/shmem.h>
#include <divine/toolkit/bitoperations.h>

#ifndef DIVINE_SHAREDHASHSET_H
#define DIVINE_SHAREDHASHSET_H

namespace divine {

template< typename _Item, typename _Hasher = divine::default_hasher< _Item > >
struct SharedHashSet {

    typedef _Item Item;
    typedef Item TableItem;
    typedef _Hasher Hasher;

    struct Cell {
        std::atomic< hash_t > hashLock;
        Item value;

        /* TODO: parallel table initialization */
        Cell() : hashLock( 0 ), value() {}
        Cell( const Cell & ) : hashLock( 0 ), value() {}
    };

    typedef std::vector< Cell > Row;

    static const unsigned rows = 32;
    static const unsigned cacheLine = 64;// Bytes
    static const unsigned thresh = cacheLine / sizeof( Cell );
    static const unsigned threshMSB = bitops::compiletime::MSB( thresh );

    std::vector< Row > table;
    unsigned initialSize;// power of 2
    unsigned initialMask;// filled up to most significant bit
    std::atomic< unsigned > _size;// size = 2^{_size}
    SpinLock growing;
    Hasher hasher;

    size_t size() const {
        return size_t(1) << _size;
    }

    explicit SharedHashSet( Hasher h = Hasher() )
        : table( rows ), initialSize( 0 ), initialMask( 0 ),
          _size( 0 ), hasher( h )
    {}

    void setSize( unsigned s ) {
        assert( s );
        initialMask = bitops::fill( s - 1 );
        _size = initialSize = bitops::MSB( initialMask + 1 );
        table[ 0 ].resize( size_t(1) << initialSize );
    }

    void setHasher( Hasher h ) {
        hasher = h;
    }

    /** TODO: move away so HashSet can use this method */
    unsigned index( hash_t h, unsigned i, unsigned mask ) const {
        h &= ~hash_t( thresh - 1 );
        const unsigned Q = 1, R = 1;
        if ( i < thresh )// for small i use trivial computation
            return (h + i) & mask;
        else {
            unsigned j = i & unsigned( thresh - 1 );
            i = i >> threshMSB;
            unsigned hop = ( (2 * Q + 1) * i + 2 * R * (i * i) ) << threshMSB;
            return (h + j + hop ) & mask;
        }
    }

    unsigned getBorder( unsigned row ) {
        return thresh * ( initialSize + ( row > 1 ? row - 1 : 0 ) );
    }
    /**
     * Insert an element into the set. Returns true if the element was added,
     * false if it was already in the set.
     */
    std::tuple< Item, bool > insert( Item x ) {
        return insertHinted( x, hasher.hash( x ) );
    }
    std::tuple< Item, bool > insertHinted( Item x, hash_t h) {
        h <<= 1;
        unsigned mask = initialMask;

        for ( unsigned row = 0; row < rows; ++row ) {
            if ( !table[ row ].size() )
                grow( row );
            if ( row > 1 )
                mask = (mask << 1) | 1;

            unsigned border = getBorder( row );
            for ( unsigned i = 0; i < border; ++i ) {
                unsigned id = index( h, i, mask );
                Cell &cell = table[ row ][ id ];
                hash_t chl = cell.hashLock;

                if ( chl == 0 ) { /* empty → insert */
                    /* Strong CAS since it won't be called twice. */
                    if ( cell.hashLock.compare_exchange_strong( chl, h | 1 ) ) {
                        cell.value = x;
                        cell.hashLock.exchange( h & ~1 ); /* We need a barrier here. */
                        return std::make_tuple( x, true );
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
                        return std::make_tuple( cell.value, false );
                }

                /* not empty and not equal → reprobe */
            }
        }

        std::cerr << "too many collisions" << std::endl;
        abort();
        __builtin_unreachable();
    }

    inline bool has( Item x ) {
        return has( x, hasher.hash( x ) );
    }

    inline bool has( Item x, hash_t h ) {
        return std::get< 1 >( getHinted( x, h ) );
    }

    std::tuple< Item, bool > get( Item x ) {
        return getHinted( x, hasher.hash( x ) );
    }

    template< typename T >
    std::tuple< Item, bool > getHinted( T x, hash_t h ) {
        h <<= 1;

        unsigned s = _size;// use only one atomic load
        unsigned mask = ( unsigned(1) << s ) - 1;
        unsigned row = s - initialSize + 1;

        do {
            --row;
            mask = ( mask >> 1 ) | initialMask;

            assert( table[ row ].size() - 1 == mask );

            unsigned border = getBorder( row );
            for ( unsigned i = 0; i < border; ++i ) {
                unsigned id = index( h, i, mask );

                assert( id < table[ row ].size() );

                Cell &cell = table[ row ][ id ];
                hash_t chl = cell.hashLock;

                if (chl == 0)
                    continue;

                if ( (chl | 1) == (h | 1) ) {
                    if ( chl & 1 )
                        while ( cell.hashLock & 1 );

                    if ( hasher.equal( cell.value, x ) )
                        return std::make_tuple( cell.value, true );
                }
            }
        } while ( row );
        return std::make_tuple( Item(), false );
    }

    Item operator[]( size_t index ) {
        return table[ rowIndex( index ) ][ subIndex( index ) ].value;
    }

    SharedHashSet( const SharedHashSet & ) = delete;
    SharedHashSet &operator=( const SharedHashSet & )= delete;

protected:

    inline size_t rowIndex( size_t index ) {
        if ( index < ( size_t( 1 ) << initialSize ) )
            return 0;
        return bitops::MSB( index ) - initialSize + 1;
    }

    inline size_t subIndex( size_t index ) {
        if ( index < ( size_t( 1 ) << initialSize ) )
            return index;
        return bitops::withoutMSB( index );
    }

    void grow( unsigned rowIndex ) {
        assert( rowIndex );
        std::lock_guard< SpinLock > lock( growing );

        Row &r = table[ rowIndex ];
        if ( !r.size() ) {
            r.resize( size_t(1) << ( _size ) );
            ++_size;
        }
    }

};

}

#endif
