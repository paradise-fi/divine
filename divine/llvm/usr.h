#ifndef USR_H
#define USR_H

#ifndef DIVINE
#define DIVINE
#endif

/* ---- TEMPORARY ---- */
#define DEBUG
#define NO_MALLOC_FAILURE
#define NO_JMP
#define NEW_INTERP_BUGS
/* ------------------- */

#define assert __divine_assert
#define ap __divine_ap
#define LTL( name, x ) const char * const __divine_LTL_ ## name = #x

#ifdef TRACE
#define trace __divine_trace
#else
#define trace( x )
#endif

#ifdef DEBUG
#define DBG_ASSERT( x ) __divine_assert( x );
#else
#define DBG_ASSERT( x )
#endif

// Inlining have potential to break desired logic of interruption masking.
// Use NOINLINE for every function in which interruption is (un)masked.
#define NOINLINE __attribute__(( noinline ))

#if __has_attribute( warning )
#define WARNING( message ) __attribute__(( warning( message ) ))
#else
#if __has_attribute( deprecated )
 // silly workaround for unsupported warning attribute
#define WARNING( message ) __attribute__(( deprecated( message ) ))
#else
#define WARNING( message )
#endif
#endif

#if __has_attribute( error )
#define ERROR( message ) __attribute__(( error( message ) ))
#else
#if __has_attribute( unavailable )
 // workaround for unsupported error attribute
#define ERROR( message ) __attribute__(( unavailable( message ) ))
#else
#define ERROR( message )
#endif
#endif

#define UNSUPPORTED_USER     ERROR( "the function is not yet implemented in the user-space." )
#define UNSUPPORTED_SYSTEM   ERROR( "the function is currently unsupported by system-space." )
#define NO_EFFECT            WARNING( "the function is currently ignored during state space generation and" \
                                      " hence doesn't affect overall process of verification." )

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for DiVinE-provided builtins. */
int __divine_new_thread( void (*)(void *), void* );
void __divine_interrupt_mask( void );
void __divine_interrupt_unmask( void );
void __divine_interrupt( void );
int __divine_get_tid( void );
int __divine_choice( int );
void __divine_assert( int ); // + some informative string ?
void __divine_ap( int );
void * __divine_malloc( unsigned long );
void __divine_free( void * );

#ifdef TRACE
void __divine_trace( const char *, ... );
#endif

#ifdef __cplusplus
}
#endif

#endif
