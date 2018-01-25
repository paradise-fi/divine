/* TAGS: min threads c++ */
#include <pthread.h>
#include <cassert>
#include <cstdint>

pthread_key_t key;

int main() {
    using ptr = void *;

    int r = pthread_key_create( &key, nullptr );
    pthread_t tid;

    uintptr_t val = uintptr_t( pthread_getspecific( key ) );
    assert( val == 0 );

    pthread_setspecific( key, ptr( 16 ) );
    val = uintptr_t( pthread_getspecific( key ) );
    assert( val == 16 );

    pthread_create( &tid, nullptr, []( void * ) -> void * {
            uintptr_t val = uintptr_t( pthread_getspecific( key ) );
            assert( val == 0 );
            pthread_setspecific( key, ptr( 42 ) );
            val = uintptr_t( pthread_getspecific( key ) );
            assert( val == 42 );
            return nullptr;
        }, nullptr );

    val = uintptr_t( pthread_getspecific( key ) );
    assert( val == 16 );

    pthread_join( tid, nullptr );

    val = uintptr_t( pthread_getspecific( key ) );
    assert( val == 16 );
}
