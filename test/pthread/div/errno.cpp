/* TAGS: min threads c++ */
#include <errno.h>
#include <dios.h>
#include <assert.h>
#include <pthread.h>

int *mainErrno;

int main() {
    pthread_t tid;

    assert( errno == 0 );
    errno = 42;
    assert( errno == 42 );
    assert( *__dios_get_errno() == 42 );
    assert( &errno == __dios_get_errno() );
    mainErrno = __dios_get_errno();

    pthread_create( &tid, nullptr, []( void * ) -> void * {
            assert( errno == 0 );
            errno = 16;
            assert( errno == 16 );
            assert( *__dios_get_errno() == 16 );
            assert( &errno == __dios_get_errno() );
            assert( __dios_get_errno() != mainErrno );
            return nullptr;
        }, nullptr );

    assert( errno == 42 );
    assert( &errno == __dios_get_errno() );

    pthread_join( tid, nullptr );

    assert( errno == 42 );
    assert( &errno == __dios_get_errno() );
}

