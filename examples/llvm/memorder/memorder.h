// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <divine.h>
#include <divine/problem.h>
#include <algorithm>

#ifndef __STORE_BUFFER_SIZE
#define __STORE_BUFFER_SIZE 4
#endif

#ifdef __cplusplus
extern "C" {
#endif

void __dsb_store( void *addr, uint64_t value, int bitwidth );
void __dsb_flushOne();
void __dsb_flush();
uint64_t __dsb_load( void *addr, int bitwidth );

#ifdef __cplusplus
} // extern "C"

template< typename T >
void __dsb_store( T *var, T val ) {
    __dsb_store( var, uint64_t( val ), sizeof( T ) * 8 );
}
template< typename T >
T __dsb_load( T *var ) { return T( __dsb_load( var, sizeof( T ) * 8 ) ); }

template< typename T >
struct WMO {
    WMO() { }
    explicit WMO( T val ) : val( val ) { }

    T load() { return __dsb_load( &val ); }
    void store( T x ) { __dsb_store( &val, x ); }

    operator T() { return load(); }
    WMO &operator=( T x ) {
        store( x );
        return *this;
    }

  private:
    T val;
};
#endif

