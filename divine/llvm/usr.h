#ifndef USR_H
#define USR_H

/* TEMPORARY */
#ifdef OLD_INTERP
#define NO_MALLOC_FAILURE
#define DEBUG
#define NO_BOOL
#define TRACE
#define NO_INTRINSIC_MEMCPY
#else  // new interpreter
#define DEBUG
#define NO_MALLOC_FAILURE
#define NO_JMP
#define NO_BOOL
#define NEW_INTERP_BUGS
#endif

#ifndef __cplusplus
typedef enum { false = 0, true } bool;
#endif

#ifndef NULL
#define NULL 0L
#endif

#define assert __divine_assert
#define ap __divine_ap
#define LTL(name, x) const char *__LTL_ ## name = #x

#ifdef TRACE
#define trace __divine_trace
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for DiVinE-provided builtins. */
int __divine_new_thread(void (*)(void *), void*);
void __divine_interrupt_mask(void);
void __divine_interrupt_unmask(void);
void __divine_interrupt(void);
int __divine_get_tid(void);
int __divine_choice(int);
void __divine_assert( int ); // + some informative string ?
void __divine_ap( int );
void * __divine_malloc( unsigned long );
void __divine_free( void * );
void *memcpy( void *, const void *, unsigned long );

#ifdef TRACE
void __divine_trace( const char *, ... );
#endif

#ifdef __cplusplus
}
#endif

/* (TEMPORARY HERE) Standart library functions provided within user-space */
void * malloc( unsigned long size ) __attribute__((noinline));
void free( void * ) __attribute__((noinline));

#ifndef PROTOTYPES_ONLY

void * malloc( unsigned long size ) {
    __divine_interrupt_mask();
#ifdef TRACE
    trace("malloc(%d) called..",size);
#endif
    __divine_interrupt_mask();
#ifdef NO_MALLOC_FAILURE
    return __divine_malloc(size); // always success
#else
    if ( __divine_choice(2) ) {
        return __divine_malloc(size); // success
    } else {
        return NULL; // failure
    }
#endif
}

void free( void * p) {
    __divine_interrupt_mask();
#ifdef TRACE
    trace("free(%p) called..",p);
#endif
    __divine_free(p);
}

#endif // PROTOTYPES_ONLY
#endif
