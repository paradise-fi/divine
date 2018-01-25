/* TAGS: min threads c++ */
#include <pthread.h>
#include <cassert>
#include <cstdint>

pthread_key_t key;

int main() {
    int r = pthread_key_create( &key, nullptr );
    pthread_t tid;

    pthread_create( &tid, nullptr, []( void * ) -> void * {
            return nullptr;
        }, nullptr );
    pthread_join( tid, nullptr );
}

