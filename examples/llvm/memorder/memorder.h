// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <divine.h>
#include <divine/problem.h>

#ifndef __STORE_BUFFER_SIZE
#define __STORE_BUFFER_SIZE 2
#endif

#define __LART_WM_DIRECT__ __attribute__((annotate("LART::WM::direct")))

#ifdef __cplusplus
extern "C" {
#endif

void __dsb_store( void *addr, uint64_t value, int bitwidth ) __attribute__((annotate("LART::WM::store, LART::WM::direct")));
void __dsb_flush() __attribute__((annotate("LART::WM::flush, LART::WM::direct")));
uint64_t __dsb_load( void *addr, int bitwidth ) __attribute__((annotate("LART::WM::load, LART::WM::direct")));

#ifdef __cplusplus
} // extern "C"

template< typename T >
void __dsb_store( T *var, T val ) __LART_WM_DIRECT__ {
    __dsb_store( var, uint64_t( val ), sizeof( T ) * 8 );
}
template< typename T >
T __dsb_load( T *var ) __LART_WM_DIRECT__ {
    return T( __dsb_load( var, sizeof( T ) * 8 ) );
}

template< typename T >
struct WMO {
    WMO() __LART_WM_DIRECT__ { }
    explicit WMO( T val ) __LART_WM_DIRECT__ : val( val ) { }

    T load() __LART_WM_DIRECT__ { return __dsb_load( &val ); }
    void store( T x ) __LART_WM_DIRECT__ { __dsb_store( &val, x ); }

    operator T() __LART_WM_DIRECT__ { return load(); }
    WMO &operator=( T x ) __LART_WM_DIRECT__ {
        store( x );
        return *this;
    }

  private:
    T val;
};
#endif

