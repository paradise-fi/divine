// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include "memorder.h"
#include <pthread.h>

enum APs { r0t0, r0t1 };
LTL( sc, G( r0t0 -> G( !r0t1 ) ) && G( r0t1 -> G( !r0t0 ) ) );

int x, y;

template< int &rx, int &ry, APs ap >
void *worker( void * = nullptr ) {
    rx = 1;
    if ( ry == 0 )
        AP( ap );
    __dsb_flush();
    return nullptr;
}

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, &worker< x, y, r0t0 >, nullptr );
    worker< y, x, r0t1 >();
    pthread_join( tid, nullptr );
}
