#pragma once
#include <dios.h>
#include <utility>
#include <new>

#ifdef __divine__
#include <sys/divm.h>
#endif

#define _NOTHROW __attribute__((__nothrow__))
#define _ROOT __attribute__((__annotate__("divine.link.always")))

#define _UNREACHABLE_F(...) do { \
        __dios_trace_f( __VA_ARGS__ ); \
        __dios_fault( _VM_F_Assert, "unreachable called" ); \
        __builtin_unreachable(); \
    } while ( false )

extern uint32_t __tainted;

namespace abstract {

template< typename T, typename ... Args >
static T *__new( Args &&...args ) {
    void *ptr = __vm_obj_make( sizeof( T ) );
    new ( ptr ) T( std::forward< Args >( args )... );
    return static_cast< T * >( ptr );
}

template< typename T >
static T *mark( T *ptr ) {
    return static_cast< T * >( __dios_pointer_set_type( ptr, _VM_PT_Marked ) );
}

template< typename T >
static T *weaken( T *ptr ) {
    return static_cast< T * >( __dios_pointer_set_type( ptr, _VM_PT_Weak ) );
}

template< typename T >
static T __taint()
{
    static_assert( std::is_integral< T >::value, "Cannot taint a non-integral value." );
    return static_cast< T >( __tainted );
}


} // namespace abstract
