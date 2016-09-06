. lib
. flavour vanilla

llvm_verify invalid "PROBLEM.*Uncaught signal: SIGSEGV" pthread.cpp <<EOF
#include <signal.h>
#include <divine.h>

int main() {
    int sig = __divine_choice( 31 ) + 1;
    if ( sig != SIGSEGV )
        signal( sig, SIG_IGN );
    raise( SIGSEGV );
    return 0;
}
EOF

llvm_verify invalid "PROBLEM.*Uncaught signal: SIGSEGV" pthread.cpp <<EOF
#include <signal.h>

int main() {
    signal( SIGSEGV, SIG_DFL );
    raise( SIGSEGV );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <signal.h>

typedef void (*sighandler_t)( int );

int main() {
    sighandler_t old = signal( SIGSEGV, SIG_IGN );
    assert( old == SIG_DFL );
    raise( SIGSEGV );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <signal.h>

typedef void (*sighandler_t)( int );
volatile sig_atomic_t signalled = 0;

void handler( int sig ) {
    assert( sig == SIGSEGV );
    signalled = 1;
}

int main() {
    sighandler_t old = signal( SIGSEGV, handler );
    assert( old == SIG_DFL );
    int sig = __divine_choice( 31 ) + 1;
    if ( sig != SIGSEGV )
        signal( sig, SIG_IGN );
    raise( SIGSEGV );
    assert( signalled );
    old = signal( SIGSEGV, SIG_DFL );
    assert( old == handler );
    return 0;
}
EOF

llvm_verify invalid "PROBLEM.*Uncaught signal: SIGSEGV" pthread.cpp <<EOF
#include <assert.h>
#include <signal.h>

typedef void (*sighandler_t)( int );
volatile sig_atomic_t signalled = 0;

void handler( int sig ) {
    assert( sig == SIGSEGV );
    signalled = 1;
}

int main() {
    signal( SIGSEGV, handler );
    raise( SIGSEGV );
    assert( signalled == 1 );
    signal( SIGSEGV, SIG_DFL );
    raise( SIGSEGV );
    return 0;
}
EOF

llvm_verify ltl_valid ltl <<EOF
#include <signal.h>
#include <divine.h>

enum APs { ignored, in_handler, post_raise };
LTL( ltl, F( ignored && F( in_handler && F( post_raise ) ) ) );

void handler( int sig ) {
    if ( sig == SIGSEGV )
        AP( in_handler );
}

int main() {
    signal( SIGSEGV, SIG_IGN );
    raise( SIGSEGV );
    AP( ignored );
    signal( SIGSEGV, handler );
    raise( SIGSEGV );
    AP( post_raise );
    return 0;
}
EOF
