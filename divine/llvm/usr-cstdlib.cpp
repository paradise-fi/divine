/* Includes */
#include "cstdlib"

/* Memory allocation */
void * malloc( size_t size ) {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "malloc( %d ) called..", size );
#endif
    __divine_interrupt_mask();
#ifdef NO_MALLOC_FAILURE
    return __divine_malloc( size ); // always success
#else
    if ( __divine_choice( 2 ) ) {
        return __divine_malloc( size ); // success
    } else {
        return NULL; // failure
    }
#endif
}

void free( void * p) {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "free( %p ) called..", p );
#endif
    __divine_free( p );
}

/* Termination */
void _ZSt9terminatev( void ) {
    // TODO?
}


/* Operators new & delete */
void _Znwm( void ) {
    // TODO?
}

void _Znam( void ) {
    // TODO?
}


/* IOStream */

void _ZNSt8ios_base4InitC1Ev( void ) { // std::ios_base::Init
    // TODO?
}

/* Exit */
void exit( int exit_code ) {
    // TODO
    ( void ) exit_code;
}

int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

int atexit( void ( *func )( void ) ) {
    // TODO
    ( void ) func;
    return 0;
}

