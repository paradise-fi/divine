// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <atomic>
#include <utility>
#include <type_traits>

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

namespace _impl {

template< typename Self, typename T >
struct WeakAtomicIntegral {

    T operator |=( T val ) {
        return self()._data.fetch_or( val, std::memory_order_acq_rel ) | val;
    }

    T operator &=( T val ) {
        return self()._data.fetch_and( val, std::memory_order_acq_rel ) & val;
    }

    Self &self() { return *static_cast< Self * >( this ); }
};

struct Empty { };
}

template< typename T >
struct WeakAtomic : std::conditional< std::is_integral< T >::value && !std::is_same< T, bool >::value,
                      _impl::WeakAtomicIntegral< WeakAtomic< T >, T >,
                      _impl::Empty >::type
{
    WeakAtomic( T x ) : _data( x ) { }
    WeakAtomic() = default;

    operator T() const { return _data.load( std::memory_order_consume ); }
    T operator=( T val ) {
        _data.store( val, std::memory_order_release );
        return val;
    }

  private:
    std::atomic< T > _data;
    friend struct _impl::WeakAtomicIntegral< WeakAtomic< T >, T >;
};

}

#endif // _DIVINE_TOOLKIT_WEAK_ATOMIC_H
