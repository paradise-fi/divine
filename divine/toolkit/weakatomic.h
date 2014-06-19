// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <atomic>
#include <utility>

/* Simple wrapper around atomic with weakened memory orders
 * also includes non-atomic fake wrapper with same interface.
 *
 * The WeakAtomic users consume memory order for reading and release MO for writing,
 * which should assure atomicity and consistency of given variable, however it does
 * not assure consistence of other variables written before given atomic location.
 * Read-modify-write operations use memory_order_acq_rel.
 *
 * To be used in extensions which does not require locking, only atomicity. */

#ifndef _DIVINE_TOOLKIT_WEAK_ATOMIC_H
#define _DIVINE_TOOLKIT_WEAK_ATOMIC_H

namespace divine {

template< typename T >
class WeakAtomic {
    std::atomic< T > _data;
  public:

    WeakAtomic( T x ) : _data( x ) { }
    WeakAtomic() = default;

    operator T() const { return _data.load( std::memory_order_consume ); }
    T operator=( T val ) {
        _data.store( val, std::memory_order_release );
        return val;
    }

    template< typename = decltype( std::declval< std::atomic< T > & >().fetch_or( 0 ) ) >
    T operator |=( T val ) {
        return _data.fetch_or( val, std::memory_order_acq_rel ) | val;
    }

    template< typename = decltype( std::declval< std::atomic< T > & >().fetch_and( 0 ) ) >
    T operator &=( T val ) {
        return _data.fetch_and( val, std::memory_order_acq_rel ) & val;
    }
};

template< typename T >
class NotAtomic {
    T _data;
  public:

    NotAtomic( T x ) : _data( x ) { }

    operator T() const { return _data; }
    T operator=( T val ) { _data = val; }

    template< typename = decltype( std::declval< T & >() |= 0 ) >
    T operator |=( T val ) { return _data |= val; }

    template< typename = decltype( std::declval< T & >() &= 0 ) >
    T operator &=( T val ) { return _data |= val; }
};

}

#endif // _DIVINE_TOOLKIT_WEAK_ATOMIC_H
