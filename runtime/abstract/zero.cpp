#include <abstract/zero.h>
#include <cassert>
#include <cstdint>
#include <limits>

using namespace abstract;
inline Zero * __abstract_zero_construct( int i ) {
    auto value = i == 0 ? Zero::Domain::ZeroValue : Zero::Domain::NonzeroValue;
    return __new< Zero >( value );
}

inline Zero * __abstract_zero_meet( Zero * a, Zero * b ) {
    if ( a->value == Zero::Domain::ZeroValue )
        return __new< Zero >( b->value );
    if ( b->value == Zero::Domain::ZeroValue )
        return __new< Zero >( a->value );
    return __new< Zero >();
}

inline Zero * __abstract_zero_join( Zero * a, Zero * b ) {
    if (( a->value == Zero::Domain::ZeroValue ) || ( b->value == Zero::Domain::ZeroValue ))
        return __new< Zero >( Zero::Domain::ZeroValue );
    return __new< Zero >();
}

inline Zero * __abstract_zero_div( Zero * a, Zero * b ) {
    assert( b->value != Zero::Domain::ZeroValue || b->value != Zero::Domain::Unknown );
    if ( a->value == Zero::Domain::ZeroValue )
        return __new< Zero >( Zero::Domain::ZeroValue );
    if ( a->value == Zero::Domain::NonzeroValue )
        return __new< Zero >( Zero::Domain::NonzeroValue );
    return __new< Zero >();
}

inline Zero * __abstract_zero_rem( Zero * a, Zero * b ) {
    assert( b->value != Zero::Domain::ZeroValue || b->value != Zero::Domain::Unknown );
    if ( a->value == Zero::Domain::ZeroValue )
        return __new< Zero >( Zero::Domain::ZeroValue );
    return __new< Zero >();
}

inline Zero * __abstract_zero_shift( Zero * a, Zero * b ) {
    if ( a->value == Zero::Domain::ZeroValue )
        return __new< Zero >( Zero::Domain::ZeroValue );
    if ( b->value == Zero::Domain::ZeroValue )
        return __new< Zero >( Zero::Domain::ZeroValue );
    return __new< Zero >();
}

Zero * __abstract_zero_value_negate( Zero * a ) {
    if ( a->value == Zero::Domain::ZeroValue )
        a->value = Zero::Domain::NonzeroValue;
    else if ( a->value == Zero::Domain::NonzeroValue )
        a->value = Zero::Domain::ZeroValue;
    //if unknonwn do nothing
    return a;
}

Zero * __abstract_zero_value_and( Zero * a, Zero * b ) {
    if ( a->value == Zero::Domain::Unknown )
        return a;
    if ( b->value == Zero::Domain::Unknown )
        return b;
    if ( a->value == Zero::Domain::ZeroValue )
        return a;
    if ( b->value == Zero::Domain::ZeroValue )
        return b;
    return a;
}

Zero * __abstract_zero_value_or( Zero * a, Zero * b ) {
    if ( a->value == Zero::Domain::NonzeroValue )
        return a;
    if ( b->value == Zero::Domain::NonzeroValue )
        return b;
    if ( a->value == Zero::Domain::Unknown )
        return a;
    if ( b->value == Zero::Domain::Unknown )
        return b;
    return a;
}

#define __abstract_zero_lift_impl( name, type ) \
Zero *  __abstract_zero_lift_##name( type i )  { \
    auto val = i == 0 ? Zero::Domain::ZeroValue : Zero::Domain::NonzeroValue; \
    return __new< Zero >( val ); \
}

#ifdef __divine__

#define __abstract_zero_lower_impl( name, type ) \
type __abstract_zero_lower_##name( Zero * val ) { \
    size_t range = std::numeric_limits<type>::max(); \
    if ( val->value == Zero::Domain::ZeroValue ) \
        return 0; \
    else if ( val->value == Zero::Domain::NonzeroValue ) \
            return __vm_choose( range ) + 1; \
    else \
            return __vm_choose( range + 1 ); \
}

#else

#define __abstract_zero_lower_impl( name, type ) \
type __abstract_zero_lower_##name( Zero * val ) { \
    size_t range = std::numeric_limits<type>::max(); \
    if ( val->value == Zero::Domain::ZeroValue ) \
        return 0; \
    else if ( val->value == Zero::Domain::NonzeroValue ) \
        return std::rand() % range + 1; \
    else \
        return std::rand() % ( range + 1 ); \
}

#endif

extern "C" {
    Zero ** __abstract_zero_alloca() {
        return __new< Zero * >( mark( __new< Zero >( Zero::Domain::Unknown ) ) );
    }

    Zero * __abstract_zero_load( Zero ** a ) {
        return mark( *a );
    }

    void __abstract_zero_store( Zero * val, Zero ** ptr ) {
        *ptr = weaken( val );
    }

    __abstract_zero_lift_impl( i1, bool )
    __abstract_zero_lift_impl( i8, uint8_t )
    __abstract_zero_lift_impl( i16, uint16_t )
    __abstract_zero_lift_impl( i32, uint32_t )
    __abstract_zero_lift_impl( i64, uint64_t )

    __abstract_zero_lower_impl( i1,  bool )
    __abstract_zero_lower_impl( i8,  uint8_t )
    __abstract_zero_lower_impl( i16, uint16_t )
    __abstract_zero_lower_impl( i32, uint32_t )
    __abstract_zero_lower_impl( i64, uint64_t )

    Zero * __abstract_zero_add( Zero * a, Zero * b ) {
        return __abstract_zero_meet( a, b );
    }

    Zero * __abstract_zero_sub( Zero * a, Zero * b ) {
        return __abstract_zero_meet( a, b );
    }

    Zero * __abstract_zero_mul( Zero * a, Zero * b ) {
        return __abstract_zero_join( a, b );
    }

    Zero * __abstract_zero_sdiv( Zero * a, Zero * b ) {
        return __abstract_zero_div( a, b );
    }

    Zero * __abstract_zero_udiv( Zero * a, Zero * b ) {
        return __abstract_zero_div( a, b );
    }

    Zero * __abstract_zero_urem( Zero * a, Zero * b ) {
        return __abstract_zero_rem( a, b );
    }

    Zero * __abstract_zero_srem( Zero * a, Zero * b ) {
        return __abstract_zero_rem( a, b );
    }

    Zero * __abstract_zero_and( Zero * a, Zero * b ) {
        return __abstract_zero_join( a, b );
    }

    Zero * __abstract_zero_or( Zero * a, Zero * b ) {
        return __abstract_zero_meet( a, b );
    }

    Zero * __abstract_zero_xor( Zero * a, Zero * b ) {
        return __abstract_zero_meet( a, b );
    }

    Zero * __abstract_zero_shl( Zero * a, Zero * b ) {
        return __abstract_zero_shift( a, b );
    }

    Zero * __abstract_zero_lshr( Zero * a, Zero * b ) {
        return __abstract_zero_shift( a, b );
    }

    Zero * __abstract_zero_ashr( Zero * a, Zero * b ) {
        return __abstract_zero_shift( a, b );
    }

    // cast operators
    Zero * __abstract_zero_trunc( Zero * a ) {
        if ( a->value == Zero::Domain::NonzeroValue )
            return __new< Zero >( Zero::Domain::Unknown );
        return __new< Zero >( a->value );
    }

    Zero * __abstract_zero_zext( Zero * a ) {
        return __new< Zero >( a->value );
    }

    Zero * __abstract_zero_sext( Zero * a ) {
        return __new< Zero >( a->value );
    }

    Zero * __abstract_zero_bitcast( Zero * a ) {
        return __new< Zero >( a->value );
    }

    // icmp operators
    Zero * __abstract_zero_icmp_eq( Zero * a, Zero * b ) {
        if ( a->value == Zero::Domain::ZeroValue && b->value == Zero::Domain::ZeroValue )
            return __new< Zero >( Zero::Domain::NonzeroValue );
        return __new< Zero >();
    }

    Zero * __abstract_zero_icmp_ne( Zero * a, Zero * b ) {
        return __abstract_zero_value_negate( __abstract_zero_icmp_eq( a, b ) );
    }

    Zero * __abstract_zero_icmp_ugt( Zero * a, Zero * b ) {
        if ( a->value == Zero::Domain::Unknown || b->value == Zero::Domain::Unknown )
            return __new< Zero >();
        else if ( a->value == Zero::Domain::NonzeroValue && b->value == Zero::Domain::ZeroValue )
            return __new< Zero >( Zero::Domain::NonzeroValue );
        else if ( b->value == Zero::Domain::NonzeroValue && a->value == Zero::Domain::ZeroValue )
            return __new< Zero >( Zero::Domain::ZeroValue );
        else
            return __new< Zero >();
    }

    Zero * __abstract_zero_icmp_uge( Zero * a, Zero * b ) {
        return __abstract_zero_value_or( __abstract_zero_icmp_eq( a, b ),
                                         __abstract_zero_icmp_ugt( a, b ) );
    }

    Zero * __abstract_zero_icmp_ult( Zero * a, Zero * b ) {
       return __abstract_zero_icmp_uge( b, a );
    }

    Zero * __abstract_zero_icmp_ule( Zero * a, Zero * b ) {
        return __abstract_zero_value_or( __abstract_zero_icmp_eq( a, b ),
                                         __abstract_zero_icmp_ult( a, b ) );
    }

    Zero * __abstract_zero_icmp_sgt( Zero * a, Zero * b ) {
        if ( a->value == Zero::Domain::ZeroValue && b->value == Zero::Domain::ZeroValue )
            return __new< Zero >( Zero::Domain::ZeroValue );
        return __new< Zero >();
    }

    Zero * __abstract_zero_icmp_sge( Zero * a, Zero * b ) {
        return __abstract_zero_value_or( __abstract_zero_icmp_eq( a, b ),
                                         __abstract_zero_icmp_sgt( a, b ) );
    }

    Zero * __abstract_zero_icmp_slt( Zero * a, Zero * b ) {
        return __abstract_zero_icmp_sge( b, a );
    }

    Zero * __abstract_zero_icmp_sle( Zero * a, Zero * b ) {
        return __abstract_zero_value_or( __abstract_zero_icmp_eq( a, b ),
                                         __abstract_zero_icmp_slt( a, b ) );
    }

    Tristate * __abstract_zero_bool_to_tristate( Zero * a ) {
        if ( a->value == Zero::Domain::ZeroValue )
            return __new< Tristate >( Tristate::Domain::False );
        if ( a->value == Zero::Domain::NonzeroValue )
            return __new< Tristate >( Tristate::Domain::True );
        return __new< Tristate >( Tristate::Domain::Unknown );
    }

    Zero *__abstract_zero_assume( Zero * value, Zero * /*constraint*/, bool /*assume*/ ) {
        // TODO implement
        return value;
    }
} // extern C

