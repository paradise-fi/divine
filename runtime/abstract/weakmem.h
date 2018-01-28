// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#ifndef __LART_WEAKMEM_H_
#define __LART_WEAKMEM_H_
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if __cplusplus
#if __cplusplus >= 201103L
#include <type_traits>
#define _WM_NOTHROW noexcept
#else
#define _WM_NOTHROW throw()
#endif
#else
#define _WM_NOTHROW __attribute__((__nothrow__))
#endif
#define _WM_INTERFACE _WM_NOTHROW __attribute__((__noinline__, __flatten__))

#ifdef __cplusplus
extern "C" {
#endif

int __lart_weakmem_buffer_size() _WM_INTERFACE;
int __lart_weakmem_min_ordering() _WM_INTERFACE;

enum __lart_weakmem_order {
    __lart_wmo_not_atomic = 0,
    __lart_wmo_unordered  = (1 << 0),
    __lart_wmo_monotonic  = (1 << 1) | __lart_wmo_unordered,
    __lart_wmo_acquire    = (1 << 2) | __lart_wmo_monotonic,
    __lart_wmo_release    = (1 << 3) | __lart_wmo_monotonic,
    __lart_wmo_acq_rel    = __lart_wmo_acquire | __lart_wmo_release,
    __lart_wmo_seq_cst    = (1 << 4) | __lart_wmo_acq_rel,
    __lart_wmo_atomic_op  = (1 << 5),
    __lart_wmo_weak_cas   = (1 << 6),
};

#ifdef __divine__
struct CasRes { uint64_t value; bool success; };

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth, __lart_weakmem_order ord ) _WM_INTERFACE;
uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth, __lart_weakmem_order ord ) _WM_INTERFACE;

void __lart_weakmem_fence( __lart_weakmem_order ord ) _WM_INTERFACE;
void __lart_weakmem_kernel_fence() _WM_INTERFACE;

CasRes __lart_weakmem_cas( char *addr, uint64_t expected, uint64_t value, uint32_t bitwidth,
                           __lart_weakmem_order _ordSucc, __lart_weakmem_order _ordFail ) _WM_INTERFACE;
void __lart_weakmem_cleanup( int32_t cnt, ... ) _WM_INTERFACE;
void __lart_weakmem_resize( char *addr, uint32_t newsize ) _WM_INTERFACE;

void __lart_weakmem_dump() _WM_NOTHROW;
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace lart {
namespace weakmem {

enum class MemoryOrder : uint8_t {
    NotAtomic = __lart_wmo_not_atomic,
    Unordered = __lart_wmo_unordered,
    Monotonic = __lart_wmo_monotonic,
    Acquire = __lart_wmo_acquire,
    Release = __lart_wmo_release,
    AcqRel = __lart_wmo_acq_rel,
    SeqCst = __lart_wmo_seq_cst,
    AtomicOp = __lart_wmo_atomic_op,
    WeakCAS = __lart_wmo_weak_cas,
};

inline MemoryOrder operator|( MemoryOrder a, MemoryOrder b ) {
    using uint = typename std::underlying_type< MemoryOrder >::type;
    return MemoryOrder( uint( a ) | uint( b ) );
}

inline MemoryOrder operator&( MemoryOrder a, MemoryOrder b ) {
    using uint = typename std::underlying_type< MemoryOrder >::type;
    return MemoryOrder( uint( a ) & uint( b ) );
}

inline bool subseteq( MemoryOrder a, MemoryOrder b ) {
    return a == (a & b);
}

}
}

#endif // __cplusplus
#endif // __LART_WEAKMEM_H_
