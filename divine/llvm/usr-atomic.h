#pragma once

#include <stddef.h>
#include <string.h>

#ifdef DIVINE
#define __DIVINE_ATOMIC_BLOCK_BEGIN __divine_interrupt_mask()
#define __DIVINE_ATOMIC_BLOCK_END __divine_interrupt_unmask()
#else
#define __DIVINE_ATOMIC_BLOCK_BEGIN do {} while ( false )
#define __DIVINE_ATOMIC_BLOCK_END do {} while ( false )
#endif

namespace std {

    enum memory_order {
        memory_order_relaxed,
        memory_order_consume,
        memory_order_acquire,
        memory_order_release,
        memory_order_acq_rel,
        memory_order_seq_cst
    };


    template< typename T >
    class atomic_base {
    protected:
        T value;
    public:

        atomic_base() : value() {};
        atomic_base( T desired ) : value( desired ) {}
        atomic_base( const atomic_base& ) = delete;
        ~atomic_base() {}

        atomic_base& operator=( T desired ) {
            store( desired );
            return *this;
        }

        //atomic_base& operator=(atomic) = delete;

        bool is_lock_free() const {
            return true;
        }

        void store( T desired, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            memcpy( &value, &desired, sizeof( T ) );
            __DIVINE_ATOMIC_BLOCK_END;
        }

        T load( memory_order = memory_order_seq_cst ) const {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result;
            memcpy( &result, &value, sizeof( T ) );
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        } 

        operator T() const {
            return load();
        }

        T exchange( T desired, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result;
            memcpy( &result, &value, sizeof( T ) );
            memcpy( &value, &desired, sizeof( T ) );
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

        bool compare_exchange_weak( T& expected, T desired, memory_order = memory_order_seq_cst ) {
            bool result = false;
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            if ( !memcmp( &value, &expected, sizeof( T ) ) ) {
                memcpy( &value, &desired, sizeof( T ) );
                result = true;
            }
            else
                memcpy( &expected, &value, sizeof( T ) );
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

        bool compare_exchange_strong( T& expected, T desired, memory_order = memory_order_seq_cst ) {
            bool result = false;
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            if ( !memcmp( &value, &expected, sizeof( T ) ) ) {
                memcpy( &value, &desired, sizeof( T ) );
                result = true;
            }
            else
                memcpy( &expected, &value, sizeof( T ) );
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }
    };

    template< typename T >
    class atomic_integral : public atomic_base< T > {
    public:

        atomic_integral() {};
        atomic_integral( T desired ) : atomic_base< T >( desired ) {}
        atomic_integral( const atomic_integral& ) = delete;
        T fetch_add( T arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value += arg;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

        T fetch_sub( T arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value -= arg;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

        T fetch_and( T arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value &= arg;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

        T fetch_or( T arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value |= arg;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }
        T fetch_xor( T arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value ^= arg;
            __DIVINE_ATOMIC_BLOCK_END;
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
            return fetch_add( arg );
        }
        T operator-=( T arg ) {
            return fetch_sub( arg );
        }
        T operator&=( T arg ) {
            return fetch_and( arg );
        }
        T operator|=( T arg ) {
            return fetch_or( arg );
        }
        T operator^=( T arg ) {
            return fetch_xor( arg );
        }
    };

    template< typename T >
    class atomic : public atomic_base< T > {
    public:
        atomic() {}
        atomic( T desired ) : atomic_base< T >( desired ) {}
        atomic( const atomic& ) = delete;
    };

    template< typename T >
    class atomic< T* > : public atomic_base< T* > {
    public:
        atomic() {};
        atomic( T desired ) : atomic_base< T* >( desired ) {}
        atomic( const atomic& ) = delete;

        T* fetch_add( ptrdiff_t arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value += arg;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }
        T* fetch_sub( ptrdiff_t arg, memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            T result = this->value;
            this->value -= arg;
            __DIVINE_ATOMIC_BLOCK_END;
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
        T operator+=( ptrdiff_t arg ) {
            return fetch_add( arg );
        }
        T operator-=( ptrdiff_t arg ) {
            return fetch_sub( arg );
        }

    };

#define ATOMIC_FLAG_INIT { flag = false }
    class atomic_flag {
        bool flag;
    public:
        atomic_flag() :
            flag( false )
        {}

        void clear( memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            flag = false;
            __DIVINE_ATOMIC_BLOCK_END;
        }

        bool test_and_set( memory_order = memory_order_seq_cst ) {
            __DIVINE_ATOMIC_BLOCK_BEGIN;
            bool result = flag;
            flag = true;
            __DIVINE_ATOMIC_BLOCK_END;
            return result;
        }

    };

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
    T atomic_compare_exchange_weak( atomic< T > *obj, T *expected, T desired ) {
        return obj->compare_exchange_weak( *expected, desired );
    }

    template< typename T >
    T atomic_compare_exchange_weak_explicit( atomic< T > *obj, T *expected, T desired, memory_order, memory_order ) {
        return obj->compare_exchange_weak( *expected, desired );
    }

    template< typename T >
    T atomic_compare_exchange_strong( atomic< T > *obj, T *expected, T desired ) {
        return obj->compare_exchange_strong( *expected, desired );
    }

    template< typename T >
    T atomic_compare_exchange_strong_explicit( atomic< T > *obj, T *expected, T desired, memory_order, memory_order ) {
        return obj->compare_exchange_strong( *expected, desired );
    }

    template< typename T, typename U >
    T atomic_fetch_add( atomic< T > *obj, U arg ) {
        return obj->fetch_add( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_add_explicit( atomic< T > *obj, U arg, memory_order ) {
        return obj->fetch_add( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_sub( atomic< T > *obj, U arg ) {
        return obj->fetch_sub( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_sub_explicit( atomic< T > *obj, U arg, memory_order ) {
        return obj->fetch_sub( arg );
    }
    template< typename T, typename U >
    T atomic_fetch_and( atomic< T > *obj, U arg ) {
        return obj->fetch_and( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_and_explicit( atomic< T > *obj, U arg, memory_order ) {
        return obj->fetch_and( arg );
    }
    template< typename T, typename U >
    T atomic_fetch_or( atomic< T > *obj, U arg ) {
        return obj->fetch_or( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_or_explicit( atomic< T > *obj, U arg, memory_order ) {
        return obj->fetch_or( arg );
    }
    template< typename T, typename U >
    T atomic_fetch_xor( atomic< T > *obj, U arg ) {
        return obj->fetch_xor( arg );
    }

    template< typename T, typename U >
    T atomic_fetch_xor_explicit( atomic< T > *obj, U arg, memory_order ) {
        return obj->fetch_xor( arg );
    }

    bool atomic_flag_test_and_set( atomic_flag *p ) {
        return p->test_and_set();
    }

    bool atomic_flag_test_and_set_explicit( atomic_flag *p, memory_order ) {
        return p->test_and_set();
    }

    void atomic_flag_clear( atomic_flag *p ) {
        p->clear();
    }

    void atomic_flag_clear_explicit( atomic_flag *p, memory_order ) {
        p->clear();
    }

#ifdef _MSC_VER
#define INTEGRAL_ATOMIC(type, shortcut) \
    template<> class atomic< type > : public atomic_integral< type > { \
    public: \
        atomic() {} \
        atomic( type desired ) : atomic_integral< type >( desired ) {} \
    }; \
    typedef atomic< type > atomic_##shortcut
#else
#define INTEGRAL_ATOMIC(type, shortcut) \
    template<> class atomic< type > : public atomic_integral< type > { \
    public: \
        atomic() {} \
        atomic( type desired ) : atomic_integral< type >( desired ) {} \
        atomic( const atomic& ) = delete; \
    }; \
    typedef atomic< type > atomic_##shortcut

#endif

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
//    INTEGRAL_ATOMIC( char16_t, char16_t );
//    INTEGRAL_ATOMIC( char32_t, char32_t );
    INTEGRAL_ATOMIC( wchar_t, wchar_t );
//    INTEGRAL_ATOMIC( int_least8_t, int_least8_t );
//    INTEGRAL_ATOMIC( uint_least8_t, uint_least8_t );
//    INTEGRAL_ATOMIC( int_least16_t, int_least16_t );
//    INTEGRAL_ATOMIC( uint_least16_t, uint_least16_t );
//    INTEGRAL_ATOMIC( int_least32_t, int_least32_t );
//    INTEGRAL_ATOMIC( uint_least32_t, uint_least32_t );
//    INTEGRAL_ATOMIC( int_least64_t, int_least64_t );
//    INTEGRAL_ATOMIC( uint_least64_t, uint_least64_t );
//    INTEGRAL_ATOMIC( int_fast8_t, int_fast8_t );
//    INTEGRAL_ATOMIC( uint_fast8_t, uint_fast8_t );
//    INTEGRAL_ATOMIC( int_fast16_t, int_fast16_t );
//    INTEGRAL_ATOMIC( uint_fast16_t, uint_fast16_t );
//    INTEGRAL_ATOMIC( int_fast32_t, int_fast32_t );
//    INTEGRAL_ATOMIC( uint_fast32_t, uint_fast32_t );
//    INTEGRAL_ATOMIC( int_fast64_t, int_fast64_t );
//    INTEGRAL_ATOMIC( uint_fast64_t, uint_fast64_t );
//    INTEGRAL_ATOMIC( intptr_t, intptr_t );
//    INTEGRAL_ATOMIC( uintptr_t, uintptr_t );
//    INTEGRAL_ATOMIC( std::size_t, size_t );
//    INTEGRAL_ATOMIC( std::ptrdiff_t, ptrdiff_t );
//    INTEGRAL_ATOMIC( intmax_t, intmax_t );
//    INTEGRAL_ATOMIC( uintmax_t, uintmax_t );

#undef INTEGRAL_ATOMIC

}

#undef __DIVINE_ATOMIC_BLOCK_BEGIN
#undef __DIVINE_ATOMIC_BLOCK_END
