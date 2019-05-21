#pragma once

#include <util/array.hpp>
#include <rst/tristate.hpp>

#include <experimental/type_traits>

#define _NOTHROW __attribute__((__nothrow__))
#define _ROOT __attribute__((__annotate__("divine.link.always")))

#define _UNREACHABLE_F(...) do { \
        __dios_trace_f( __VA_ARGS__ ); \
        __dios_fault( _VM_F_Assert, "unreachable called" ); \
        __builtin_unreachable(); \
    } while ( false )

#define _EXPECT(expr,...) \
    if (!(expr)) \
        _UNREACHABLE_F( __VA_ARGS__ );

#define _LART_INLINE \
    __attribute__((__always_inline__, __flatten__))

#define _LART_NOINLINE \
    __attribute__((__noinline__))

#define _LART_INTERFACE \
    __attribute__((__nothrow__, __noinline__, __flatten__)) \
    __invisible extern "C"

#define _LART_FN \
__attribute__((__nothrow__, __always_inline__, __flatten__)) __invisible \

namespace abstract {

    template < typename T, int PT = _VM_PT_Heap >
    using Array = __dios::Array< T, PT >;

    template< typename T >
    struct Abstracted { };

    using Bitwidth = int8_t;

} // namespace abstract
