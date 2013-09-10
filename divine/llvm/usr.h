#ifndef __DIVINE_USR_H
#define __DIVINE_USR_H

#ifndef DIVINE
#define DIVINE
#endif

/* ---- TEMPORARY ---- */
#define DEBUG
#define NO_MALLOC_FAILURE
#define NO_JMP
#define NEW_INTERP_BUGS
/* ------------------- */

#undef assert
#if __cplusplus
#define assert( x ) __divine_assert( static_cast< int >( x ) )
#else
#define assert( x ) __divine_assert( x )
#endif
#define ap( x ) __divine_ap( x )
#define LTL( name, x ) extern const char * const __divine_LTL_ ## name = #x

#ifdef TRACE
#define trace __divine_trace
#else
#define trace( x )
#endif

#ifdef DEBUG
#if __cplusplus
#define DBG_ASSERT( x ) __divine_assert( static_cast< int >( x ) )
#else
#define DBG_ASSERT( x ) __divine_assert( x )
#endif
#else
#define DBG_ASSERT( x ) ((void)0)
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
/* Some handy macros. */
#define BREAK_MASK( command )                    \
                do {                             \
                    __divine_interrupt_unmask(); \
                    command;                     \
                    __divine_interrupt_mask();   \
                } while( 0 );

#define ATOMIC_FUN_BEGIN( visible_effect )                        \
                do {                                              \
                    __divine_interrupt_mask();                    \
                    if ( visible_effect )                         \
                        /* FIXME: this should be automatically */ \
                        /*        detected in the system-space */ \
                        BREAK_MASK( __divine_interrupt() )        \
                } while( 0 );

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
#define NOTHROW throw()
extern "C" {
#else
#define NOTHROW
#endif

/* Prototypes for DiVinE-provided builtins. */
int __divine_new_thread( void (*entry)(void *), void *arg ) NOTHROW;
int __divine_get_tid( void ) NOTHROW;

void __divine_interrupt_mask( void ) NOTHROW;
void __divine_interrupt_unmask( void ) NOTHROW;
void __divine_interrupt( void ) NOTHROW;

void __divine_assert( int value ) NOTHROW; // + some informative string ?
void __divine_ap( int id ) NOTHROW;

/*
 * Non-deterministic choice: when __divine_choice is encountered, the
 * "universe" of the program splits into n copies. Each copy of the "universe"
 * will see a different return value from __divine_choice, starting from 0.
 *
 * When more than one parameter is given, the choice becomes probabilistic: for
 * N choices, you need to provide N additional integers, giving probability
 * weight of a given alternative (numbering from 0). E.g.
 * `__divine_choice( 2, 1, 9 )` will return 0 with probability 1/10 and 1 with
 * probability 9/10.
 */
int __divine_choice( int n, ... ) NOTHROW;

/*
 * Heap access. To obtain a new heap block, call __divine_malloc with a
 * size. The size will not be rounded in any way: you get exactly what you ask
 * for. Does not fail. Calling free will mark a block as invalid and the
 * program is no longer allowed to touch it.
 *
 * TODO: Any references to freed memory blocks should be turned into null
 * pointers immediately, to avoid the possibility of incidentally creating
 * references to subsequently allocated memory. Currently this null-ing only
 * happens „sometimes“ (when the heap is canonized).
 */

void *__divine_malloc( unsigned long size ) NOTHROW;
void __divine_free( void *ptr ) NOTHROW;

/*
 * Copy memory. Doing a per-byte copy would destroy pointer maps, hence you are
 * required to copy memory blocks using this builtin. The llvm memcpy
 * intrinsics are automatically mapped memcpy, which is provided by pdclib and
 * calls __divine_memcpy. Unlike libc memcpy, the memory areas are allowed to
 * overlap (i.e. __divine_memcpy behaves like memmove).
 */
void *__divine_memcpy( void *dest, void *src, size_t count ) NOTHROW;

/*
 * Variable argument handling. Calling __divine_va_start gives you a pointer to
 * a monolithic block of memory that contains all the varargs, successively
 * assigned higher addresses going from left to right. All the rest of vararg
 * support can be implemented in the userspace then.
 */
void *__divine_va_start( void ) NOTHROW;

/*
 * Exception handling and stack unwinding.
 *
 * To unwind some frames (in the current execution stack, i.e. the current
 * thread), call __divine_unwind. The argument "frameid" gives the id of the
 * frame to unwind to (see also __divine_landingpad). If more than 1 frame is
 * unwound, the intervening frames will NOT run their landing pads.
 *
 * If a landing pad exists for the active call in the frame that we unwind
 * into, the landing pad personality routine gets exactly the arguments passed
 * as varargs to __divine_unwind.
 */
void __divine_unwind( int frameid, ... ) NOTHROW __attribute__((noreturn));

struct _DivineLP_Clause {
    int32_t type_id; // -1 for a filter
    void *tag; /* either a pointer to an array constant for a filter, or a
                  typeinfo pointer for catch */
} __attribute__((packed));

struct _DivineLP_Info {
    int32_t cleanup; /* whether the cleanup flag is present on the landingpad */
    int32_t clause_count;
    void *personality;
    struct _DivineLP_Clause clause[];
} __attribute__((packed));

/*
 * The LPInfo data will be garbage-collected when it is no longer
 * referenced. The info returned reflects the landingpad LLVM instruction: each
 * LPClause is either a "catch" or a "filter". The frameid is either an
 * absolute frame number counted from 1 upwards through the call stack, or a
 * relative frame number counted from 0 downwards (0 being the caller of
 * __divine_landingpad, -1 its caller, etc.). Returns NULL if the active call
 * in the given frame has no landing pad.
 */
struct _DivineLP_Info *__divine_landingpad( int frameid ) NOTHROW;

#ifdef TRACE
void __divine_trace( const char *, ... ) NOTHROW;
#endif

#ifdef __cplusplus
}
#endif

#undef NOTHROW

#endif
