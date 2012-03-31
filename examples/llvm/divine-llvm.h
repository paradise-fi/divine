#ifdef DIVINE

#ifndef __cplusplus__
#define NULL 0L
typedef enum { true, false } bool; // well...
#endif

#ifdef NO_MALLOC_FAILURE
#define malloc malloc_guaranteed
#endif

#ifdef __cplusplus__
extern "C" {
#endif

/* Prototypes for DiVinE-provided builtins. */
void *malloc( unsigned );
void *malloc_guaranteed( unsigned );
void free( void * );
int thread_create();
void thread_stop() __attribute__((noreturn));
void assert( int );
void trace( const char *, ... );

#ifdef __cplusplus__
}

// TODO: global new & delete
#endif

typedef int pthread_t;
typedef void pthread_attr_t;

typedef struct {
    void *result;
    volatile bool done;
} __Tid;

static const int MAXTHREAD = 8;
static __Tid __tid[MAXTHREAD]; // TODO: externalize this so it is shared by all modules
static int __tid_last = 0;

static int pthread_create(pthread_t *ptid,
                          const pthread_attr_t *attr, /* TODO? */
                          void *(*entry)(void *), void *arg)
{
    if (__tid_last == MAXTHREAD) {
        trace("FATAL: Thread capacity exceeded.");
        return 1;
    }

    int tid = *ptid = __tid_last++;

    int id = thread_create();
    if ( id == 0 ) { // this will be the parent
        return 0;
    } else {
        __tid[tid].done = false;
        __tid[tid].result = entry(arg);
        __tid[tid].done = true;
        thread_stop(); /* die */
    }
}

int pthread_join(pthread_t ptid, void **x) {
    while ( !__tid[ptid].done ) ;
    *x = __tid[ptid].result;
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
