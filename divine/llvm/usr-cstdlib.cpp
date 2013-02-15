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
