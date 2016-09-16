/*
 * Emulation of IPC example
 * ========================
 *
 *  Small program which uses unnamed pipe to communicate between
 *  two instances (here simulated by independent thread). The input
 *  to be passed into the pipe is loaded from standard input.
 *
 * Verification
 * ------------
 *
 *      $ divine compile --llvm --stdin=ipc_emulation_input.txt ipc_emulation.c
 *      $ divine verify -p assert ipc_emulation.bc
 *
 */
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

struct DataPack {
    int pfd;
    int workDone;
};

void *consumer( void *_data ) {
    struct DataPack *data = (struct DataPack*)_data;

    char cmd;
    while ( 1 ) {
        assert( read( data->pfd, &cmd, 1 ) == 1 );
        if ( cmd == '-' )
            break;

        ++data->workDone;
    }

    assert( close( data->pfd ) == 0 );
    return NULL;
}

int main() {

    int pfds[ 2 ];
    int input;
    int workDone = 0;
    struct DataPack data = {0, 0};
    assert( pipe( pfds ) == 0 );

    data.pfd = pfds[ 0 ];

    pthread_t child;
    assert( pthread_create( &child, NULL, consumer, &data ) == 0 );

    while ( (input = getchar() ) != EOF ) {
        assert( write( pfds[ 1 ], &input, 1 ) == 1 );
        if ( input == '-' )
            break;
        ++workDone;
    }
    if ( input != '-' ) {
        input = '-';
        assert( write( pfds[ 1 ], &input, 1 ) == 1 );
    }
    assert( close( pfds[ 1 ] ) == 0 );
    assert( pthread_join( child, NULL ) == 0 );
    assert( workDone == data.workDone );

    return 0;
}
