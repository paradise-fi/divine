/* TAGS: min c++ threads */
#include <sys/divm.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <cassert>

template< typename T >
struct _TestData {
    std::atomic< T > at1;
    std::atomic< T > at2;

    _TestData() :
        at1( 0 ),
        at2( 0 )
    { }
};

template< typename T >
struct _TestThread {
    pthread_t thr;
    _TestData< T > &data;

    _TestThread( _TestData< T > &data ) : data( data ) {
        pthread_create( &thr, NULL, run, this );
    }

    void join() {
        pthread_join( thr, NULL );
    }

    static void *run( void *t ) {
        _TestThread *self = reinterpret_cast< _TestThread * >( t );

        auto &at1 = self->data.at1;
        auto &at2 = self->data.at2;

        assert( at1.is_lock_free() );
        assert( at2.is_lock_free() );

        T tmp1 = 0;
        T tmp2 = 0;

        while( tmp1 == 0 && !at1.compare_exchange_weak( tmp1, 100 ) ) { }
        at2.compare_exchange_strong( tmp2, 100 );

        for ( int i = 0; i < 2; ++i )
            at1++;

        return nullptr;
    }
};


template< typename T >
void runTest() {
    _TestData< T > td;

    assert( td.at1 == 0 );
    assert( td.at1 == 0 );

    _TestThread< T > tr[ 2 ] = { _TestThread< T >( td ), _TestThread< T >( td ) };

    tr[ 0 ].join();
    tr[ 1 ].join();

    assert( td.at1 == 104 );
    assert( td.at2 == 100 );
}

int main( void ) {

    std::atomic< bool > x = { true };

    runTest< uint64_t >();
}
