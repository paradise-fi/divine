// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#ifndef __LART_WEAKMEM_H_
#define __LART_WEAKMEM_H_
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/lart.h>

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

struct CasRes { uint64_t value; bool success; };

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth,
                           enum __lart_weakmem_order ord ) _WM_INTERFACE;
uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth,
                              enum __lart_weakmem_order ord ) _WM_INTERFACE;

void __lart_weakmem_fence( enum __lart_weakmem_order ord ) _WM_INTERFACE;
void __lart_weakmem_kernel_fence() _WM_INTERFACE;

CasRes __lart_weakmem_cas( char *addr, uint64_t expected, uint64_t value, uint32_t bitwidth,
                           enum __lart_weakmem_order _ordSucc,
                           enum __lart_weakmem_order _ordFail ) _WM_INTERFACE;
void __lart_weakmem_cleanup( int32_t cnt, ... ) _WM_INTERFACE;
void __lart_weakmem_resize( char *addr, uint32_t newsize ) _WM_INTERFACE;

void __lart_weakmem_dump() _WM_NOTHROW;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __LART_WEAKMEM_H_
