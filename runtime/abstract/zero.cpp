#include <cassert>
#include <cstdint>
#include <limits>
#include "common.h"
#include "tristate.h"

#ifndef __divine__
#include <cstdlib>
#endif

#define _ROOT __attribute__((__annotate__("brick.llvm.prune.root")))

namespace abstract {

struct Zero {
    enum Domain { ZeroValue, NonzeroValue, Unknown };
    Domain value;
};

using pointer = Zero *;

inline pointer __abstract_zero_construct( Zero::Domain value ) {
    auto obj = allocate< Zero >();
    obj->value = value;
    return obj;
}

inline pointer __abstract_zero_construct() {
    return __abstract_zero_construct( Zero::Domain::Unknown );
}

inline pointer __abstract_zero_construct( int i ) {
    auto value = i == 0 ? Zero::Domain::ZeroValue : Zero::Domain::NonzeroValue;
    return __abstract_zero_construct( value );
}

inline pointer * __abstract_zero_alloca_create() {
    pointer *ptr = allocate< pointer >();
    *ptr = __abstract_zero_construct();
    return ptr;
}

inline pointer __abstract_zero_meet( pointer a, pointer b ) {
    if ( a->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( b->value );
    if ( b->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( a->value );
    return __abstract_zero_construct();
}

inline pointer __abstract_zero_join( pointer a, pointer b ) {
    if (( a->value == Zero::Domain::ZeroValue ) || ( b->value == Zero::Domain::ZeroValue ))
        return __abstract_zero_construct( Zero::Domain::ZeroValue );
    return __abstract_zero_construct();
}

inline pointer __abstract_zero_div( pointer a, pointer b ) {
    assert( b->value != Zero::Domain::ZeroValue || b->value != Zero::Domain::Unknown );
    //TODO DIVINE assert
    if ( a->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( Zero::Domain::ZeroValue );
    if ( a->value == Zero::Domain::NonzeroValue )
        return __abstract_zero_construct( Zero::Domain::NonzeroValue );
    return __abstract_zero_construct( Zero::Domain::Unknown );
}

inline pointer __abstract_zero_rem( pointer a, pointer b ) {
    assert( b->value != Zero::Domain::ZeroValue || b->value != Zero::Domain::Unknown );
    //TODO DIVINE assert
    if ( a->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( Zero::Domain::ZeroValue );
    return __abstract_zero_construct( Zero::Domain::Unknown );
}

inline pointer __abstract_zero_shift( pointer a, pointer b ) {
    if ( a->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( Zero::Domain::ZeroValue );
    if ( b->value == Zero::Domain::ZeroValue )
        return __abstract_zero_construct( b->value );
    return __abstract_zero_construct();
}

#ifdef __divine__

#define __abstract_zero_lower_type( name, type ) \
type __abstract_zero_lower_##name( pointer val ) _ROOT { \
    size_t range = std::numeric_limits<type>::max(); \
    if ( val->value == Zero::Domain::ZeroValue ) \
        return 0; \
    else if ( val->value == Zero::Domain::NonzeroValue ) \
            return __vm_choose( range ) + 1; \
    else \
            return __vm_choose( range + 1 ); \
}

#else

#define __abstract_zero_lower_type( name, type ) \
type __abstract_zero_lower_##name( pointer val ) _ROOT { \
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
    pointer * __abstract_zero_alloca() _ROOT {
        return __abstract_zero_alloca_create();
    }

    pointer __abstract_zero_load( pointer * a ) _ROOT {
        return *a;
    }

    void __abstract_zero_store( pointer val, pointer * ptr ) _ROOT {
        (*ptr)->value = val->value;
    }

    pointer __abstract_zero_lift( int i ) _ROOT {
        return __abstract_zero_construct( i );
    }

    __abstract_zero_lower_type( i1,  bool )
    __abstract_zero_lower_type( i8,  uint8_t )
    __abstract_zero_lower_type( i16, uint16_t )
    __abstract_zero_lower_type( i32, uint32_t )
    __abstract_zero_lower_type( i64, uint64_t )

    pointer __abstract_zero_add( pointer a, pointer b ) _ROOT {
        return __abstract_zero_meet( a, b );
    }

    pointer __abstract_zero_sub( pointer a, pointer b ) _ROOT {
        return __abstract_zero_meet( a, b );
    }

    pointer __abstract_zero_mul( pointer a, pointer b ) _ROOT {
        return __abstract_zero_join( a, b );
    }

    pointer __abstract_zero_sdiv( pointer a, pointer b ) _ROOT {
        return __abstract_zero_div( a, b );
    }

    pointer __abstract_zero_udiv( pointer a, pointer b ) _ROOT {
        return __abstract_zero_div( a, b );
    }

    pointer __abstract_zero_urem( pointer a, pointer b ) _ROOT {
        return __abstract_zero_rem( a, b );
    }

    pointer __abstract_zero_srem( pointer a, pointer b ) _ROOT {
        return __abstract_zero_rem( a, b );
    }

    pointer __abstract_zero_and( pointer a, pointer b ) _ROOT {
        return __abstract_zero_join( a, b );
    }

    pointer __abstract_zero_or( pointer a, pointer b ) _ROOT {
        return __abstract_zero_meet( a, b );
    }

    pointer __abstract_zero_xor( pointer a, pointer b ) _ROOT {
        return __abstract_zero_meet( a, b );
    }

    pointer __abstract_zero_shl( pointer a, pointer b ) _ROOT {
        return __abstract_zero_shift( a, b );
    }

    pointer __abstract_zero_lshr( pointer a, pointer b ) _ROOT {
        return __abstract_zero_shift( a, b );
    }

    // cast operators
    pointer __abstract_zero_trunc( pointer a ) _ROOT {
        if ( a->value == Zero::Domain::NonzeroValue )
            return __abstract_zero_construct( Zero::Domain::Unknown );
        return __abstract_zero_construct( a->value );
    }

    pointer __abstract_zero_zext( pointer a ) _ROOT {
        return __abstract_zero_construct( a->value );
    }

    pointer __abstract_zero_sext( pointer a ) _ROOT {
        return __abstract_zero_construct( a->value );
    }

    pointer __abstract_zero_bitcast( pointer a ) _ROOT {
        return __abstract_zero_construct( a->value );
    }

    pointer * __abstract_zero_bitcast_p( pointer * a ) _ROOT {
        auto ptr = __abstract_zero_alloca();
        (*ptr)->value = (*a)->value;
        return ptr;
    }

    // icmp operators
    Tristate * __abstract_zero_icmp_eq( pointer a, pointer b ) _ROOT {
        if ( a->value == Zero::Domain::ZeroValue && b->value == Zero::Domain::ZeroValue )
            return __abstract_tristate_construct( true );
        return __abstract_tristate_create();
    }

    Tristate * __abstract_zero_icmp_ne( pointer a, pointer b ) _ROOT {
        return __abstract_tristate_negate( __abstract_zero_icmp_eq( a, b ) );
    }

    Tristate * __abstract_zero_icmp_ugt( pointer a, pointer b ) _ROOT {
        if ( a->value == Zero::Domain::Unknown || b->value == Zero::Domain::Unknown )
            return __abstract_tristate_create();
        else if ( a->value == Zero::Domain::NonzeroValue && b->value == Zero::Domain::ZeroValue )
            return __abstract_tristate_construct( true );
        else if ( b->value == Zero::Domain::NonzeroValue && a->value == Zero::Domain::ZeroValue )
            return __abstract_tristate_construct( false );
        else
            return __abstract_tristate_create();
    }

    Tristate * __abstract_zero_icmp_uge( pointer a, pointer b ) _ROOT {
        return __abstract_tristate_or( __abstract_zero_icmp_eq( a, b ),
                                       __abstract_zero_icmp_ugt( a, b ) );
    }

    Tristate * __abstract_zero_icmp_ult( pointer a, pointer b ) _ROOT {
       return __abstract_zero_icmp_uge( b, a );
    }

    Tristate * __abstract_zero_icmp_ule( pointer a, pointer b ) _ROOT {
        return __abstract_tristate_or( __abstract_zero_icmp_eq( a, b ),
                                        __abstract_zero_icmp_ult( a, b ) );
    }

    Tristate * __abstract_zero_icmp_sgt( pointer a, pointer b ) _ROOT {
        if ( a->value == Zero::Domain::ZeroValue && b->value == Zero::Domain::ZeroValue )
            return __abstract_tristate_construct( false );
        return __abstract_tristate_create();
    }

    Tristate * __abstract_zero_icmp_sge( pointer a, pointer b ) _ROOT {
        return __abstract_tristate_or( __abstract_zero_icmp_eq( a, b ),
                                       __abstract_zero_icmp_sgt( a, b ) );
    }

    Tristate * __abstract_zero_icmp_slt( pointer a, pointer b ) _ROOT {
        return __abstract_zero_icmp_sge( b, a );
    }

    Tristate * __abstract_zero_icmp_sle( pointer a, pointer b ) _ROOT {
        return __abstract_tristate_or( __abstract_zero_icmp_eq( a, b ),
                                       __abstract_zero_icmp_slt( a, b ) );
    }
}

} //namespace abstract

