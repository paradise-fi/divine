#ifndef __ABSTRACT_COMMON_H_
#define __ABSTRACT_COMMON_H_

#ifdef __divine__
#include <divine.h>
#endif

namespace abstract {

template< typename T >
T * allocate() {
#ifdef __divine__
        return static_cast< T * >( __vm_obj_make( sizeof( T ) ) );
#else
        return static_cast< T * >( std::malloc( sizeof( T ) ) );
#endif
}

} //namespace abstract

#endif //__ABSTRACT_COMMON_H_
