// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#if __cplusplus >= 201103L

#include <divine.h>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <type_traits>

#ifndef DIVINE_LLVM_USR_ATOMIC
#define DIVINE_LLVM_USR_ATOMIC

namespace std {

enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};

// lock-free property
#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#define ATOMIC_POINTER_LOCK_FREE 2

namespace __at_impl {

    template< typename T >
    class atomic_base {
    protected:
        T value;
    public:

        atomic_base() : value() {};
        atomic_base( T desired ) : value( desired ) {}
        atomic_base( const atomic_base& ) = delete;
        ~atomic_base() {}

        T operator=( T desired ) {
            store( desired );
            desired;
        }

        bool is_lock_free() const {
            return true;
        }

        void store( T desired, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            memcpy( &value, &desired, sizeof( T ) );
            __divine_interrupt_unmask();
        }

        T load( memory_order = memory_order_seq_cst ) const {
            __divine_interrupt_mask();
            T result;
            memcpy( &result, &value, sizeof( T ) );
            __divine_interrupt_unmask();
            return result;
        }

        operator T() const {
            return load();
        }

        T exchange( T desired, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result;
            memcpy( &result, &value, sizeof( T ) );
            memcpy( &value, &desired, sizeof( T ) );
            __divine_interrupt_unmask();
            return result;
        }

        bool compare_exchange_weak( T &expected, T desired, memory_order, memory_order ) {
            // we don't have to simulate order until we have delayed stores
            return compare_exchange_weak( expected, desired );
        }

        bool compare_exchange_weak( T &expected, T desired, memory_order = memory_order_seq_cst ) {
            bool result = false;
            __divine_interrupt_mask();
            if ( !memcmp( &value, &expected, sizeof( T ) )
                    && __divine_choice( 2 ) )  // simulate spurious fail in weak CAS
            {
                memcpy( &value, &desired, sizeof( T ) );
                result = true;
            }
            else
                memcpy( &expected, &value, sizeof( T ) );
            __divine_interrupt_unmask();
            return result;
        }

        bool compare_exchange_strong( T &expected, T desired, memory_order, memory_order ) {
            return compare_exchange_strong( expected, desired );
        }

        bool compare_exchange_strong( T &expected, T desired, memory_order = memory_order_seq_cst ) {
            bool result = false;
            __divine_interrupt_mask();
            if ( !memcmp( &value, &expected, sizeof( T ) ) ) {
                memcpy( &value, &desired, sizeof( T ) );
                result = true;
            }
            else
                memcpy( &expected, &value, sizeof( T ) );
            __divine_interrupt_unmask();
            return result;
        }
    };

    template< typename T >
    class atomic_integral : public atomic_base< T > {
        static_assert( is_integral< T >::value,
                "Inlvalid usage of atomic_integral< T > with non-integral type" );

    public:

        atomic_integral() {};
        atomic_integral( T desired ) : atomic_base< T >( desired ) {}
        atomic_integral( const atomic_integral& ) = delete;

        T fetch_add( T arg, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result = this->value;
            this->value += arg;
            __divine_interrupt_unmask();
            return result;
        }

        T fetch_sub( T arg, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result = this->value;
            this->value -= arg;
            __divine_interrupt_unmask();
            return result;
        }

        T fetch_and( T arg, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result = this->value;
            this->value &= arg;
            __divine_interrupt_unmask();
            return result;
        }

        T fetch_or( T arg, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result = this->value;
            this->value |= arg;
            __divine_interrupt_unmask();
            return result;
        }
        T fetch_xor( T arg, memory_order = memory_order_seq_cst ) {
            __divine_interrupt_mask();
            T result = this->value;
            this->value ^= arg;
            __divine_interrupt_unmask();
            return result;
        }

        T operator++() {
            return fetch_add( 1 ) + 1;
        }

        T operator++(int) {
            return fetch_add( 1 );
        }

        T operator--() {
            return fetch_sub( 1 ) - 1;
        }

        T operator--(int) {
            return fetch_sub( 1 );
        }

        T operator+=( T arg ) {
            return fetch_add( arg ) + arg;
        }
        T operator-=( T arg ) {
            return fetch_sub( arg ) - arg;
        }
        T operator&=( T arg ) {
            return fetch_and( arg ) & arg;
        }
        T operator|=( T arg ) {
            return fetch_or( arg ) | arg;
        }
        T operator^=( T arg ) {
            return fetch_xor( arg ) ^ arg;
        }
    };

}

template< typename T >
class atomic : public __at_impl::atomic_base< T > {
public:
    atomic() {}
    atomic( T desired ) : __at_impl::atomic_base< T >( desired ) {}
    atomic( const atomic& ) = delete;

    atomic &operator=( const atomic& ) = delete;

    T operator=( T desired ) {
        this->store( desired );
        return desired;
    }
};

template< typename T >
class atomic< T * > : public __at_impl::atomic_base< T * > {

public:
    atomic() {};
    atomic( T *desired ) : __at_impl::atomic_base< T * >( desired ) {}
    atomic( const atomic& ) = delete;

    atomic &operator=( const atomic& ) = delete;

    T *operator=( T *desired ) {
        this->store( desired );
        return desired;
    }

    T *fetch_add( ptrdiff_t arg, memory_order = memory_order_seq_cst ) {
        __divine_interrupt_mask();
        T *result = this->value;
        this->value += arg;
        __divine_interrupt_unmask();
        return result;
    }
    T *fetch_sub( ptrdiff_t arg, memory_order = memory_order_seq_cst ) {
        __divine_interrupt_mask();
        T *result = this->value;
        this->value -= arg;
        __divine_interrupt_unmask();
        return result;
    }

    T *operator++() {
        return fetch_add( 1 ) + 1;
    }

    T *operator++(int) {
        return fetch_add( 1 );
    }

    T *operator--() {
        return fetch_sub( 1 ) - 1;
    }
    T *operator--(int) {
        return fetch_sub( 1 );
    }
    T *operator+=( ptrdiff_t arg ) {
        return fetch_add( arg ) + arg;
    }
    T *operator-=( ptrdiff_t arg ) {
        return fetch_sub( arg ) - arg;
    }

};

#define ATOMIC_FLAG_INIT {}
class atomic_flag {
    bool flag;
public:
    atomic_flag() :
        flag( false )
    {}

    atomic_flag &operator=( const atomic_flag& ) = delete;

    void clear( memory_order = memory_order_seq_cst ) {
        __divine_interrupt_mask();
        flag = false;
        __divine_interrupt_unmask();
    }

    bool test_and_set( memory_order = memory_order_seq_cst ) {
        __divine_interrupt_mask();
        bool result = flag;
        flag = true;
        __divine_interrupt_unmask();
        return result;
    }

};

#define ATOMIC_VAR_INIT( value ) { value }

template< typename T >
void atomic_init( std::atomic< T > *obj, T desired ) {
    // TODO: this should not be atomic, should not be called twice
    assert( obj->load() == T() /* must be default constructed */ );
    obj->store( desired );
}

template< typename Atomic >
bool atomic_is_lock_free( const Atomic * ) {
    return false;
}

template< typename T >
bool atomic_is_lock_free( const atomic< T > *obj ) {
    return obj->is_lock_free();
}

template< typename T >
void atomic_store( atomic< T > *obj, T desire ) {
    obj->store( desire );
}

template< typename T >
void atomic_store_explicit( atomic< T > *obj, T desire, memory_order ) {
    obj->store( desire );
}

template< typename T >
T atomic_load( atomic< T > *obj ) {
    return obj->load();
}

template< typename T >
T atomic_load_explicit( atomic< T > *obj, memory_order ) {
    return obj->load();
}

template< typename T >
T atomic_exchange( atomic< T > *obj, T desired ) {
    return obj->exchange( desired );
}

template< typename T >
T atomic_exchange_explicit( atomic< T > *obj, T desired, memory_order ) {
    return obj->exchange( desired );
}

template< typename T >
bool atomic_compare_exchange_weak( atomic< T > *obj, T *expected, T desired ) {
    return obj->compare_exchange_weak( *expected, desired );
}

template< typename T >
bool atomic_compare_exchange_weak_explicit( atomic< T > *obj, T *expected, T desired, memory_order, memory_order ) {
    return obj->compare_exchange_weak( *expected, desired );
}

template< typename T >
bool atomic_compare_exchange_strong( atomic< T > *obj, T *expected, T desired ) {
    return obj->compare_exchange_strong( *expected, desired );
}

template< typename T >
bool atomic_compare_exchange_strong_explicit( atomic< T > *obj, T *expected, T desired, memory_order, memory_order ) {
    return obj->compare_exchange_strong( *expected, desired );
}

template< typename T >
T atomic_fetch_add( atomic< T > *obj, T arg ) {
    return obj->fetch_add( arg );
}

template< typename T >
T atomic_fetch_add_explicit( atomic< T > *obj, T arg, memory_order ) {
    return obj->fetch_add( arg );
}

template< typename T >
T atomic_fetch_sub( atomic< T > *obj, T arg ) {
    return obj->fetch_sub( arg );
}

template< typename T >
T atomic_fetch_sub_explicit( atomic< T > *obj, T arg, memory_order ) {
    return obj->fetch_sub( arg );
}
template< typename T >
T atomic_fetch_and( atomic< T > *obj, T arg ) {
    return obj->fetch_and( arg );
}

template< typename T >
T atomic_fetch_and_explicit( atomic< T > *obj, T arg, memory_order ) {
    return obj->fetch_and( arg );
}
template< typename T >
T atomic_fetch_or( atomic< T > *obj, T arg ) {
    return obj->fetch_or( arg );
}

template< typename T >
T atomic_fetch_or_explicit( atomic< T > *obj, T arg, memory_order ) {
    return obj->fetch_or( arg );
}
template< typename T >
T atomic_fetch_xor( atomic< T > *obj, T arg ) {
    return obj->fetch_xor( arg );
}

template< typename T >
T atomic_fetch_xor_explicit( atomic< T > *obj, T arg, memory_order ) {
    return obj->fetch_xor( arg );
}

inline static bool atomic_flag_test_and_set( atomic_flag *p ) {
    return p->test_and_set();
}

inline static bool atomic_flag_test_and_set_explicit( atomic_flag *p, memory_order ) {
    return p->test_and_set();
}

inline static void atomic_flag_clear( atomic_flag *p ) {
    p->clear();
}

inline static void atomic_flag_clear_explicit( atomic_flag *p, memory_order ) {
    p->clear();
}

#define INTEGRAL_ATOMIC_TYPEDEF( type, shortcut ) \
    typedef atomic< type > atomic_##shortcut; \
    static_assert( std::is_base_of< __at_impl::atomic_integral< type >, \
            atomic< type > >::value, \
            "Invalid integral atomic typedef" )

#define INTEGRAL_ATOMIC(type, shortcut) \
    template<> class atomic< type > : public __at_impl::atomic_integral< type > { \
    public: \
        atomic() {} \
        atomic( type desired ) : atomic_integral< type >( desired ) {} \
        atomic( const atomic& ) = delete; \
        atomic &operator=( const atomic& ) = delete; \
        \
        type operator=( type desired ) { \
            this->store( desired ); \
            return desired; \
        } \
    }; \
    INTEGRAL_ATOMIC_TYPEDEF( type, shortcut );

    INTEGRAL_ATOMIC( char, char );
    INTEGRAL_ATOMIC( signed char, schar );
    INTEGRAL_ATOMIC( unsigned char, uchar );
    INTEGRAL_ATOMIC( short, short );
    INTEGRAL_ATOMIC( unsigned short, ushort );
    INTEGRAL_ATOMIC( int, int );
    INTEGRAL_ATOMIC( unsigned int, uint );
    INTEGRAL_ATOMIC( long, long );
    INTEGRAL_ATOMIC( unsigned long, ulong );
    INTEGRAL_ATOMIC( long long, llong );
    INTEGRAL_ATOMIC( unsigned long long, ullong );
    INTEGRAL_ATOMIC( char16_t, char16_t );
    INTEGRAL_ATOMIC( char32_t, char32_t );
    INTEGRAL_ATOMIC( wchar_t, wchar_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_least8_t, int_least8_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_least8_t, uint_least8_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_least16_t, int_least16_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_least16_t, uint_least16_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_least32_t, int_least32_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_least32_t, uint_least32_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_least64_t, int_least64_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_least64_t, uint_least64_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_fast8_t, int_fast8_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_fast8_t, uint_fast8_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_fast16_t, int_fast16_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_fast16_t, uint_fast16_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_fast32_t, int_fast32_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_fast32_t, uint_fast32_t );
    INTEGRAL_ATOMIC_TYPEDEF( int_fast64_t, int_fast64_t );
    INTEGRAL_ATOMIC_TYPEDEF( uint_fast64_t, uint_fast64_t );
    INTEGRAL_ATOMIC_TYPEDEF( intptr_t, intptr_t );
    INTEGRAL_ATOMIC_TYPEDEF( uintptr_t, uintptr_t );
    INTEGRAL_ATOMIC_TYPEDEF( std::size_t, size_t );
    INTEGRAL_ATOMIC_TYPEDEF( std::ptrdiff_t, ptrdiff_t );
    INTEGRAL_ATOMIC_TYPEDEF( intmax_t, intmax_t );
    INTEGRAL_ATOMIC_TYPEDEF( uintmax_t, uintmax_t );

    typedef atomic< bool > atomic_bool;

#undef INTEGRAL_ATOMIC
#undef INTEGRAL_ATOMIC_TYPEDEF

}

#endif // DIVINE_LLVM_USR_ATOMIC

#else // C++11

#error "atomic is only supported for C++11 or newer, use -std=c++11"

#endif // C++11
