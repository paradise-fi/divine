#ifndef __ABSTRACT_COMMON_H_
#define __ABSTRACT_COMMON_H_
#include <dios.h>
#include <utility>
#include <new>

#ifdef __divine__
#include <sys/divm.h>
#endif

#define _NOTHROW __attribute__((__nothrow__))
#define _ROOT __attribute__((__annotate__("divine.link.always")))

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

} // namespace abstract

#endif //__ABSTRACT_COMMON_H_
