// -*- C++ -*- (c) 2013 Petr Ročkai <me@mornfall.net>

#include <asm/byteorder.h>
#include <byteswap.h>
#include <atomic>
#include <divine/toolkit/pool.h> // for align

#ifndef DIVINE_BITTUPLE_H
#define DIVINE_BITTUPLE_H

namespace divine {

static inline uint64_t bitshift( uint64_t t, int shift ) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64( shift < 0 ? bswap_64( t << -shift ) : bswap_64( t >> shift ) );
#else
    return shift < 0 ? ( t << -shift ) : ( t >> shift );
#endif
}

struct BitPointer {
    BitPointer() : base( nullptr ), _bitoffset( 0 ) {}
    template< typename T > BitPointer( T *t, int offset = 0 )
        : base( static_cast< void * >( t ) ), _bitoffset( offset )
    {
        normalize();
    }
    uint32_t &word() { return *static_cast< uint32_t * >( base ); }
    uint64_t &dword() { return *static_cast< uint64_t * >( base ); }
    void normalize() {
        base = &word() + _bitoffset / 32;
        _bitoffset = _bitoffset % 32;
    }
    void shift( int bits ) { _bitoffset += bits; normalize(); }
    void fromReference( BitPointer r ) { *this = r; }
    int bitoffset() { return _bitoffset; }
private:
    void *base;
    int _bitoffset;
};

static inline uint64_t mask( int first, int count ) {
    return bitshift(uint64_t(-1), -first) & bitshift(uint64_t(-1), (64 - first - count));
}

/*
 * NB. This function will alias whatever "to" points to with an uint64_t. With
 * aggressive optimisations, this might break code that passes an address of a
 * variable of different type. When "to" points to a stack variable, take
 * precautions to avoid breaking strict aliasing rules (the violation is not
 * detected by GCC as of 4.7.3).
 */
static inline void bitcopy( BitPointer from, BitPointer to, int bitcount )
{
    while ( bitcount ) {
        int w = std::min( 32 - from.bitoffset(), bitcount );
        uint32_t fmask = mask( from.bitoffset(), w );
        uint64_t tmask = mask( to.bitoffset(), w );
        uint64_t bits = bitshift( from.word() & fmask, from.bitoffset() - to.bitoffset() );
        assert_eq( bits & ~tmask, 0u );
        assert_eq( bits & tmask, bits );
        to.dword() = (to.dword() & ~tmask) | bits;
        from.shift( w ); to.shift( w ); bitcount -= w; // slide
    }
}

template< typename T, int width = sizeof( T ) * 8 >
struct BitField
{
    static const int bitwidth = width;
    struct Virtual : BitPointer {
        void set( T t ) { bitcopy( BitPointer( &t ), *this, bitwidth ); }
        Virtual operator=( T t ) {
            set( t );
            return *this;
        }
        Virtual operator=( Virtual v ) {
            set( v.get() );
            return *this;
        }

        operator T() const { return get(); }
        T get() const {
            union U {
                uint64_t x;
                T t;
                U() : t() { }
            } u;
            bitcopy( *this, BitPointer( &u.x ), bitwidth );
            return u.t;
        }

        Virtual &operator++() {
            T value( get() );
            set( ++value );
            return *this;
        }
        T operator++(int) {
            T value( get() );
            T result( value++ );
            set( value );
            return result;
        }

        Virtual &operator--() {
            T value( get() );
            set( --value );
            return *this;
        }
        T operator--(int) {
            T value( get() );
            T result( value-- );
            set( value );
            return result;
        }
        template< typename U >
        Virtual operator+=( U value ) {
            T t( get() );
            t += value;
            set( t );
            return *this;
        }
        template< typename U >
        Virtual operator-=( U value ) {
            T t( get() );
            t -= value;
            set( t );
            return *this;
        }
        template< typename U >
        Virtual operator*=( U value ) {
            T t( get() );
            t *= value;
            set( t );
            return *this;
        }
        template< typename U >
        Virtual operator/=( U value ) {
            T t( get() );
            t /= value;
            set( t );
            return *this;
        }
        template< typename U >
        Virtual operator%=( U value ) {
            T t( get() );
            t %= value;
            set( t );
            return *this;
        }
    };
};

struct BitLock
{
    static const int bitwidth = 1;
    struct Virtual : BitPointer {
        using Atomic = std::atomic< uint32_t >;
        Atomic &atomic() { return *reinterpret_cast< Atomic * >( &word() ); }
        uint32_t bit() {
            assert_leq( bitoffset(), 31 );
            return uint32_t( 1 ) << bitoffset();
        }
        void lock() {
            uint32_t l = word();
            do { l &= ~bit(); } while ( !atomic().compare_exchange_weak( l, l | bit() ) );
        }
        void unlock() { atomic().exchange( word() & ~bit() ); }
        bool locked() { return atomic().load() & bit(); }
    };
};

template< int O, typename... Args > struct BitAccess;

template< int O >
struct BitAccess< O > { static const int total = 0; };

template< int O, typename T, typename... Args >
struct BitAccess< O, T, Args... > {
    static const int offset = O;
    static const int width = T::bitwidth;
    typedef typename T::Virtual Head;
    typedef BitAccess< offset + T::bitwidth, Args... > Tail;
    static const int total = width + Tail::total;
};

template< typename BA, int I >
struct _AccessAt : _AccessAt< typename BA::Tail, I - 1 > {};

template< typename BA >
struct _AccessAt< BA, 0 > { using T = BA; };

template< typename... Args >
struct _BitTuple
{
    using Access = BitAccess< 0, Args... >;
    static const int bitwidth = Access::total;
    template< int I > using AccessAt = _AccessAt< Access, I >;
    template< int I > static int offset() { return AccessAt< I >::T::offset; }
};

template< typename... Args > struct BitTuple : _BitTuple< Args... >
{
    struct Virtual : BitPointer, _BitTuple< Args... > {};
    char storage[ align( Virtual::bitwidth, 32 ) / 8 ];
    BitTuple() { std::fill( storage, storage + sizeof( storage ), 0 ); }
    operator BitPointer() { return BitPointer( storage ); }
};

template< int I, typename BT >
typename BT::template AccessAt< I >::T::Head get( BT &bt )
{
    typename BT::template AccessAt< I >::T::Head t;
    t.fromReference( bt );
    t.shift( BT::template offset< I >() );
    return t;
}

}

#endif
