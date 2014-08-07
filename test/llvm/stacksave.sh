. lib

llvm_verify invalid 'bad dereference' main <<EOF
int main( int argc, char **argv ) {
    int *x;
    {
        int vla[argc];
        x = vla;
    }
    *x = 1;
    return 0;
}
EOF

# Check that the content of the stacksave block is properly canonised.

llvm_verify valid <<EOF
#include <pthread.h>
#include <divine.h>

void *thread( void *zzz ) {
    int y;
    int *x = &y;
    {
        int vla[1 + (int) zzz];
        __divine_interrupt();
    }
    *x = 1;
    return 1;
}

int main( int argc, char **argv ) {
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    free( malloc( 1 ) );
    return 0;
}
EOF
