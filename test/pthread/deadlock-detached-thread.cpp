/* TAGS: min threads c++ */
#include <pthread.h>
#include <assert.h>

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, []( void * ) -> void * {
            pthread_mutex_lock( &mtx );
            return nullptr;
        }, nullptr );
    pthread_detach( tid );
    pthread_mutex_lock( &mtx ); /* ERROR */
    pthread_mutex_unlock( &mtx );
}
