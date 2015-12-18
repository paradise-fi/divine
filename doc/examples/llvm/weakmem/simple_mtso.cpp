// divine-cflags: -std=c++11 -O2 -fno-slp-vectorize -fno-vectorize
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <pthread.h>
#include <assert.h>
#include <weakmem.h>

lart::weakmem::Slot< int > x, y;

void *worker0( void * = nullptr ) {
    x = 1;
    return reinterpret_cast< void * >( size_t( y ) );
}

void *worker1( void * = nullptr ) {
    y = 1;
    return reinterpret_cast< void * >( size_t( x ) );
}

int main() {
    __lart_weakmem_buffer_size = 1;

    pthread_t t0, t1;
    pthread_create( &t0, nullptr, &worker0, nullptr );
    pthread_create( &t1, nullptr, &worker1, nullptr );
    size_t rx, ry;
    pthread_join( t0, reinterpret_cast< void ** >( &ry ) );
    pthread_join( t1, reinterpret_cast< void ** >( &rx ) );
    assert( rx == 1 || ry == 1 );
}
