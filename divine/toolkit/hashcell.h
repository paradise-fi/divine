// -*- C++ -*- (c) 2013 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimir Still <xstill@fi.muni.cz>

#ifndef DIVINE_HASHCELL_H
#define DIVINE_HASHCELL_H

#include <type_traits>

namespace divine {

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
    void invalidate() { hashLock.exchange( 3 ); }

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

    template< typename Guard >
    bool tryStore( T v, hash64_t hash, Guard guard ) {
        hash64_t chl = 0;
        if ( hashLock.compare_exchange_strong( chl, (hash << 2) | 1 ) ) {
            if ( !guard() ) {
                invalidate();
                return false;
            }
            value = v;
            hashLock.exchange( hash << 2 );
            return true;
        }
        return false;
    }

    template< typename Value >
    bool is( Value v, hash64_t hash, Hasher &h ) {
        if ( ( (hash << 2) | 1) != (hashLock | 1) )
            return false;
        if ( !wait() )
            return false;
        return h.equal( value, v );
    }

    FastAtomicCell() : hashLock( 0 ), value() {}
    FastAtomicCell( const FastAtomicCell & ) : hashLock( 0 ), value() {}
};

template< typename T, typename = void >
struct Tagged {
    T t;
    uint32_t _tag;

    static const int tagBits = 16;
    void setTag( int v ) { _tag = v; }
    int tag() { return _tag; }
    Tagged() noexcept : t(), _tag( 0 ) {}
    Tagged( const T &t ) : t( t ), _tag( 0 ) {}
};

template< typename T >
struct Tagged< T, typename std::enable_if< (T::tagBits > 0) >::type >
{
    T t;

    static const int tagBits = T::tagBits;
    void setTag( int value ) { t.setTag( value ); }
    int tag() { return t.tag(); }
    Tagged() noexcept : t() {}
    Tagged( const T &t ) : t( t ) {}
};

template< typename T, typename Hasher >
struct AtomicCell : CellBase< T, Hasher >
{
    std::atomic< Tagged< T > > value;

    static_assert( sizeof( std::atomic< T > ) == sizeof( T ),
                   "std::atomic< T > is lock-free" );
    static_assert( Tagged< T >::tagBits > 0, "T has at least a one-bit tagspace" );

    bool empty() { return !value.load().t; }
    bool invalid() { return false; }

    static hash64_t hashToTag( hash64_t hash, int bits = Tagged< T >::tagBits )
    {
        // use different part of hash than used for storing
        return hash >> ( sizeof( hash64_t ) * 8 - bits );
    }

    void invalidate() {
        Tagged< T > v = value;
        v.setTag( 0 );
        value.exchange( v );
    }

    Tagged< T > &deatomize() {
        value.load(); // fence
        return *reinterpret_cast< Tagged< T > * >( &value );
    }

    T &fetch() { return deatomize().t; }
    T copy() { Tagged< T > v = value; v.setTag( 0 ); return v.t; }
    bool wait() { return !invalid(); }

    void store( T bn, hash64_t hash ) {
        return tryStore( bn, hash, []() -> bool { return true; } );
    }

    template< typename Guard >
    bool tryStore( T b, hash64_t hash, Guard ) {
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
};

}

#endif
