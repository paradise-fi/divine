// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <atomic>
#include <stdint.h>
#include <tuple>
#include <algorithm>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/bitoperations.h>

#ifndef DIVINE_SHAREDHASHSET_H
#define DIVINE_SHAREDHASHSET_H

namespace divine {

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

    using Row = std::vector< Cell >;

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
            : hasher( h ), table( maxGrows ), tableWorkers( maxGrows ), currentRow( 1 ),
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
        bool changed( unsigned row ) { return row < _d.currentRow; }

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
                        assert_unreachable("impossible result from insertCell");
                }
            }
            assert_unreachable("broken loop");
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
                        assert_unreachable("impossible result from findCell");
                }
            }
            assert_unreachable("broken loop");
        }

        template< typename T >
        Find findCell( T v, hash64_t h, unsigned rowIndex )
        {
            helpWithRehashing();
            if ( changed( rowIndex ) )
                return Find( Resolution::Growing );

            Row &row = current( rowIndex );
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
            if ( !force ) {
                // read usage first to guarantee usage <= size
                size_t u = _d.used.load();
                size_t s = size();
                // usage >= 75% of table size
                // usage is never greater than size
                if ( ( s >> 2 ) >= s - u )
                    return Insert( Resolution::NoSpace );
                helpWithRehashing();
                if ( changed( _td.currentRow ) )
                    return Insert( Resolution::Growing );
            }

            Row &row = current( _td.currentRow );
            assert( !row.empty() );
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
            assert( rowIndex );

            if ( rowIndex >= _d.table.size() )
                assert_unreachable( "out of growth space" );

            if ( _d.currentRow >= rowIndex )
                return false;

            while ( _d.growing.exchange( true ) ); // acquire growing lock

            if ( _d.currentRow >= rowIndex ) {
                _d.growing.exchange( false ); // release the lock
                return false;
            }

            Row &row = current( rowIndex - 1 );
            _d.table[ rowIndex ].resize( row.size() * 2 );
            _d.currentRow.exchange( rowIndex );
            _d.tableWorkers[ rowIndex ] = 1;
            _d.doneSegments.exchange( 0 );
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
            assert( it < end );

            ThreadData td;
            td.currentRow = _d.currentRow;

            // every cell has to be invalidated
            for ( ; it != end; it->invalidate(), ++it ) {
                if ( it->empty() )
                    continue;
                if ( !it->wait() )
                    continue;
                value_type value = it->fetch();
                if ( WithTD( _d, td ).insertCell< true >( value, it->hash( _d.hasher ) ).r ==
                     Resolution::NoSpace )
                    assert_unreachable( "ran out of space during growth" );
            }

            if ( ++_d.doneSegments == segments ) {
                _d.growing.exchange( false ); /* done */
                releaseRow( td.currentRow - 1 );
            }

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
            if ( !--_d.tableWorkers[ index ] ) {
                _d.table[ index ] = Row();
            }
        }

        void acquireRow( unsigned index ) {
            do {
                ++_d.tableWorkers[ index ];
                if ( index == _d.currentRow )
                    break;
                --_d.tableWorkers[ index ];
                index = _d.currentRow;
            } while ( true );
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
        s = bitops::fill( s - 1 ) + 1;
        _d.table[ 1 ].resize( s );
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
using FastConcurrentSet = _ConcurrentHashSet< FastAtomicCell< T, Hasher > >;

template< typename T, typename Hasher = default_hasher< T > >
using ConcurrentSet = _ConcurrentHashSet< AtomicCell< T, Hasher > >;

}

#endif
