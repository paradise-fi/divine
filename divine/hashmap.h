// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <numeric>
#include <cmath>
#include <wibble/sys/mutex.h>
#include <divine/state.h>

#ifndef DIVINE_HASHMAP_H
#define DIVINE_HASHMAP_H

namespace divine {

// default hash implementation
template< typename T >
hash_t hash( T t ) {
    return t.hash();
}

// default validity implementation
template< typename T >
bool valid( T t ) {
    return t.valid();
}

struct NoopLocker {
    void setSize( int ) {}
    bool lock( size_t s ) { return true; }
    void moveLock( int, int ) {}
    void unlock( int ) {}
    void lockAll() {}
    void unlockAll() {}
};

struct NaiveLocker {
    wibble::sys::Mutex mutex;
    void setSize( int ) {}
    bool lock( size_t s ) { mutex.lock(); return true; }
    void moveLock( int, int ) {}
    void unlock( int ) { mutex.unlock(); }
    void lockAll() { mutex.lock(); }
    void unlockAll() { mutex.unlock(); }
};

struct ConstTranslate {
    int size( int s ) { return s / 1024 + 2; }
    int off( int s ) { return s / 1024; }
};

struct SqrtTranslate {
    int region;
    int size( int s ) { region = int( std::sqrt( s ) ); return region + 2; }
    int off( int s ) { return s / region; }
};

struct LinearTranslate {
    int region;
    int size( int s ) { region = s / 1024; return 1026; }
    int off( int s ) { return s / region; }
};

template< typename Translate >
struct RegionLocker {
    typedef std::vector< wibble::sys::Mutex* > Locks;
    Locks locks, _locks;
    size_t m_size;
    bool all_locked;
    Translate trans;

    // has to be called after lockAll and before unlockAll
    void setSize( size_t s )
    {
        assert( all_locked );
        // do not shrink
        assert( trans.size( s ) >= locks.size() );
        _locks.resize( trans.size( s ), 0 );
        m_size = s;
        for ( int i = 0; i < _locks.size(); ++i ) {
            _locks[ i ] = new wibble::sys::Mutex();
            _locks[ i ]->lock();
        }
        std::swap( locks, _locks );
        assert( all_locked );
    }

    size_t lock( size_t off )
    {
        size_t size = m_size;
        wibble::sys::Mutex *l = locks[ trans.off( off % size ) ];
        l->lock();
        if ( size == m_size && l == locks[ trans.off( off % size ) ] )
            return true;
        l->unlock();
        return false;
    }

    void lockIndex( int idx )
    {
        wibble::sys::Mutex *l = locks[ idx ];
        while ( true ) {
            l->lock();
            if ( l == locks[ idx ] )
                break;
            l->unlock(); // again...
            l = locks[ idx ];
        }
    }

    void unlock( int off )
    {
        locks[ trans.off( off ) ]->unlock();
    }

    void moveLock( int o, int n )
    {
        // may race-deadlock when someone is trying to grow and n < o,
        // ie the merge has overflown
        if ( trans.off( o % m_size ) != trans.off( n % m_size ) ) {
            lock( n );
            unlock( o % m_size );
        }
    }

    void lockAll() {
        int size = locks.size();
        for ( int i = 0; i < size; ++i ) {
            lockIndex( i );
            size = locks.size();
        }
        // at this point
        bool done = false;
        int i = 0;
        while ( !done ) {
            try {
                for ( ; i < _locks.size(); ++i ) {
                    // delete _locks[ i ];
                }
                done = true;
            } catch ( wibble::exception::System x ) {
                // someone still had wrong idea of locks, need to
                // retry; hopefully we aren't leaking here...
            }
        }
        _locks.resize( 0 );
        all_locked = true;
    }

    void unlockAll() {
        all_locked = false;
        for ( int i = 0; i < _locks.size(); ++i )
            _locks[ i ]->unlock();
        for ( int i = 0; i < locks.size(); ++i )
            locks[ i ]->unlock();
    }

    RegionLocker() : all_locked( false ) {}
};

typedef RegionLocker< LinearTranslate > LinearLocker;
typedef RegionLocker< ConstTranslate > ConstLocker;
typedef RegionLocker< SqrtTranslate > SqrtLocker;

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
    };
};

}

namespace divine {

template< typename _Key, typename _Value, typename Locker = NoopLocker >
struct HashMap
{
    Locker m_locker;

    typedef _Key Key;
    typedef _Value Value;

    typedef std::pair< Key, Value > Item;

    int maxcollision() { return 32 + int( std::sqrt( size() ) ) / 16; }
    int growthreshold() { return 75; } // percent

    struct Reference {
        Key key;
        size_t offset;

        Reference( Key k, size_t off )
            : key( k ), offset( off )
        {}

        Reference() : offset( 0 )
        {}
    };

    typedef std::vector< Key > Keys;
    // typedef std::vector< Value > Values;
    typedef typename VectorType< Value >::Vector Values;

    Keys m_keys;
    Values m_values;

    int m_factor;
    int m_used;
    std::vector< int > m_usedVec;

    int used() {
        if ( m_used == -1 )
            return std::accumulate( m_usedVec.begin(), m_usedVec.end(), 0 );
        return m_used;
    }

    size_t size() const { return m_keys.size(); }
    bool empty() const { return !m_used; }
    size_t usage() const { return m_used; }

    template< typename Merger >
    std::pair< Reference, int > _mergeInsert( Item item, Merger m,
                                              Keys &keys,
                                              Values &values,
                                              bool lock = true,
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

            if ( lock )
                if ( i ) {
                    m_locker.moveLock( oldoff, off );
                } else {
                    if ( !m_locker.lock( off ) )
                        return std::make_pair( Reference(), -1 );
                }

            assert( keys.size() == values.size() );

            idx = off % keys.size();

            if ( !valid( keys[ idx ] ) || item.first == keys[ idx ] ) {
                if ( !valid( keys[ idx ] ) )
                    ++ used;
                Item use = m( std::make_pair( keys[ idx ], values[ idx ] ), item );
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
            assert( ! (keys[ idx ] == item.first) );
        }
        m_locker.unlock( idx ); // unlock the last index we have tried
        return std::make_pair( Reference(), -2 );
    }

    template< typename Merger >
    Reference mergeInsert( Item item, Merger m, int thread, hash_t hint = 0 ) {
        while ( true ) {
            if ( usage() > (size() * growthreshold()) / 100 )
                grow();
            std::pair< Reference, int > r = _mergeInsert( item, m, m_keys,
                                                          m_values, true, hint );
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
            if ( thread == -1 )
                m_used += r.second;
            else
                m_usedVec[ thread ] += r.second;
            return r.first;
        }
    }

    struct DefaultMerger
    {
        Item operator()( Item a, Item b )
        {
            if ( valid( a.first ) )
                return a;
            return b;
        }
    };

    Reference insert( Key k, int thread = -1, hash_t hint = 0 )
    {
        return mergeInsert( std::make_pair( k, Value() ), DefaultMerger(),
                            thread, hint );
    }

    Reference insert( Item item, int thread = -1, hash_t hint = 0 )
    {
        return mergeInsert( item, DefaultMerger(), thread, hint );
    }

    bool has( Key k, hash_t hint = 0 )
    {
        Reference r = get( k, hint );
        unlock( r );
        return valid( r.key );
    }

    void unlock( Reference r )
    {
        m_locker.unlock( r.offset );
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

            if ( i )
                m_locker.moveLock( oldoff, off );
            else
                m_locker.lock( off );

            if ( !valid( m_keys[ idx ] ) ) {
                return Reference();
            }
            if ( k == m_keys[ idx ] ) {
                return Reference( m_keys[ idx ], idx );
            }
        }
        assert( 0 );
        return Reference();
    }

    Value &value( Reference r ) {
        return m_values[ r.offset ];
    }

    void setSize( size_t s )
    {
        m_keys.resize( s, Key() );
        m_values.resize( s, Value() );
        m_locker.lockAll();
        m_locker.setSize( s );
        m_locker.unlockAll();
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
        m_locker.lockAll();
        if ( size() > _size ) {
            m_locker.unlockAll();
            return;
        }

        Keys keys; Values values;
        size_t _used = used();
        _size = size();

        assert( m_keys.size() == m_values.size() );
        assert( factor > 1 );

        // +1 to keep the table size away from 2^n
        keys.resize( factor * m_keys.size() + 1, Key() );
        values.resize( factor * m_values.size() + 1, Value() );
        assert( keys.size() > size() );
        for ( size_t i = 0; i < m_keys.size(); ++i ) {
            if ( valid( m_keys[ i ] ) )
                _mergeInsert( std::make_pair( m_keys[ i ], m_values[ i ] ),
                              DefaultMerger(), keys, values, false );
        }
        std::swap( keys, m_keys );
        std::swap( values, m_values );
        m_locker.setSize( size() );
        assert( used() == _used );
        assert( size() > _size );
        m_locker.unlockAll();
    }

    Reference operator[]( int off ) {
        return Reference( m_keys[ off ], off );
    }

    void useThreadAccess( int t )
    {
        m_usedVec.resize( t, 0 );
        m_used = -1;
    }

    HashMap( int initial, int factor )
        : m_factor( factor )
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
    swap( a.m_locker, b.m_locker );
}

}

#endif
