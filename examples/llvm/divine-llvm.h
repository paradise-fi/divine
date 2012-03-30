#ifdef DIVINE

#ifndef __cplusplus
#define NULL 0L
typedef enum { true, false } bool; // well...
#endif

#ifdef NO_MALLOC_FAILURE
#define malloc __divine_builtin_malloc_guaranteed
#else
#define malloc __divine_builtin_malloc
#endif

#define malloc_guaranteed __divine_builtin_malloc_guaranteed
#define free __divine_builtin_free
#define assert __divine_builtin_assert
#define ap __divine_builtin_ap
#define trace __divine_builtin_trace

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for DiVinE-provided builtins. */
void *__divine_builtin_malloc( unsigned );
void *__divine_builtin_malloc_guaranteed( unsigned );
void __divine_builtin_free( void * );

void __divine_builtin_thread_create( int *,
                                     void (*)(void *(*)(void *), int *tid, void *arg, volatile int *init),
                                     void *(*)(void *), int *, void *, volatile int * );
int __divine_builtin_thread_id();
void __divine_builtin_thread_stop() __attribute__((noreturn));
void __divine_builtin_mutex_lock( int * );
void __divine_builtin_mutex_unlock( int * );

void __divine_builtin_assert( int );
void __divine_builtin_ap( int );
void __divine_builtin_trace( const char *, ... );

#ifdef __cplusplus
}

// TODO: global new & delete
#endif

typedef int pthread_t;
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_mutex_t;

static int pthread_create(pthread_t *ptid, const pthread_attr_t *attr,
                          void *(*entry)(void *), void *arg) __attribute__((noinline));

typedef struct {
    void *result;
    pthread_mutex_t mutex;
} __Tid;

static const int MAXTHREAD = 8;
static __Tid __tid[MAXTHREAD]; // TODO: externalize this so it is shared by all modules
static pthread_mutex_t __tid_mutex;

static void __pthread_entry(void *(*entry)(void *), pthread_t *tid, void *arg, volatile int *init)
{
    int id = *tid;
    if (*tid == MAXTHREAD) {
        trace("FATAL: Thread capacity exceeded.");
        *init = 2;
        __divine_builtin_thread_stop();
    }

    __divine_builtin_mutex_lock( &__tid[id].mutex );
    *init = 1;
    __tid[id].result = entry(arg);
    __divine_builtin_mutex_unlock( &__tid[id].mutex );
    __divine_builtin_thread_stop(); /* die */
}

static int pthread_create(pthread_t *ptid,
                          const pthread_attr_t *attr, /* TODO? */
                          void *(*entry)(void *), void *arg)
{
    volatile int init = 0;
    __divine_builtin_thread_create( ptid, __pthread_entry, entry, ptid, arg, &init );

    while (!init); /* wait */
    return init != 1;
}

static void pthread_exit( void *v )
{
    int id = __divine_builtin_thread_id();
    __tid[id].result = v;
    __divine_builtin_mutex_unlock( &__tid[id].mutex );
    __divine_builtin_thread_stop(); /* die */
}

const int PTHREAD_MUTEX_NORMAL = 0;
const int PTHREAD_MUTEX_RECURSIVE = 1 << 25;
const int PTHREAD_MUTEX_RECURSIVE_NP = PTHREAD_MUTEX_RECURSIVE; /* alias */

static int pthread_join(pthread_t ptid, void **x) {
    __divine_builtin_mutex_lock( &__tid[ptid].mutex );
    *x = __tid[ptid].result;
    __divine_builtin_mutex_unlock( &__tid[ptid].mutex );
    return 0;
}

static int pthread_mutexattr_init( pthread_mutexattr_t *attr ) {
    *attr = PTHREAD_MUTEX_NORMAL;
    return 0;
}

static int pthread_mutexattr_settype( pthread_mutexattr_t *type, int t ) {
    *type = t;
    return 0;
}

static int pthread_mutex_init( pthread_mutex_t *mutex, pthread_mutexattr_t *attr )
{
    if (attr)
        *mutex = *attr;
    else
        *mutex = 0;
    return 0;
}

static int __mutex_adjust_count( pthread_mutex_t *mutex, int adj ) {
    if ( !((*mutex) & PTHREAD_MUTEX_RECURSIVE) )
        return 0;

    trace( "recursive mutex" );
    int count = ((*mutex) & 0xFF0000) >> 16;
    count += adj;
    (*mutex) &= ~0xFF0000;
    (*mutex) |= count << 16;
    return count;
}

static int pthread_mutex_lock( pthread_mutex_t *mutex ) {
    int previous = (*mutex) & 0xFFFF;
    __divine_builtin_mutex_lock( mutex );
    if (!__mutex_adjust_count( mutex, 1 )) /* a non-recursive mutex */
        assert( ((*mutex) & 0xFFFF) != previous );
    return 0;
}

static int pthread_mutex_unlock( pthread_mutex_t *mutex ) {
    if ( !__mutex_adjust_count( mutex, -1 ) )
        __divine_builtin_mutex_unlock( mutex );
    return 0;
}

#define LTL(name, x) const char *__LTL_ ## name = #x

#else // (NOT DIVINE)
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h> 
#include <stdlib.h>
#include "assert.h"

/* compatibility for native execution */

void trace(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void *malloc_guaranteed( unsigned x ) {
    return malloc( x );
}

#define ap(x)
#define LTL(name, x)

#endif
