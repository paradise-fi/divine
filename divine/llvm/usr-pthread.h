#ifndef USR_PTHREAD_H
#define USR_PTHREAD_H

/* TEMPORARY
   (until std-lib function and NEW_INTERP_BUGS will be removed and corrected respectively)
*/
#include "usr.h"

/* Macros */

// internal macros
#define _INITIALIZED_MUTEX (1 << 26)
#define _INITIALIZED_COND  (1 << 16)

// pthread-specified macros
#define PTHREAD_CREATE_JOINABLE        0
#define PTHREAD_CREATE_DETACHED        1
#define PTHREAD_MUTEX_NORMAL           0
#define PTHREAD_MUTEX_RECURSIVE        (1 << 24)
#define PTHREAD_MUTEX_ERRORCHECK       (1 << 25)
#define PTHREAD_MUTEX_DEFAULT          PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_INITIALIZER      (PTHREAD_MUTEX_DEFAULT | _INITIALIZED_MUTEX)
#define PTHREAD_COND_INITIALIZER       {.mutex = NULL, .counter = _INITIALIZED_COND}
#define PTHREAD_ONCE_INIT              1
#define PTHREAD_DESTRUCTOR_ITERATIONS  8

 // Some pthread functions call some other pthread functions
 // and inlining could break the desired logic of interruption masking.
 // Use NOINLINE for every function in which interruption is (un)masked.
#define NOINLINE __attribute__((noinline))

/* Data types */

typedef int pthread_attr_t;
typedef int pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef struct {pthread_mutex_t * mutex; int counter;} pthread_cond_t;
typedef int pthread_condattr_t;
typedef int pthread_once_t;

typedef struct _PerThreadData {
    void ** data;
    void (*destructor)(void*);
    struct _PerThreadData *next, *prev;
} _PerThreadData;

typedef _PerThreadData* pthread_key_t;

/* Function prototypes */

#ifdef __cplusplus
extern "C" {
#endif

int pthread_create(pthread_t *, const pthread_attr_t *,
                   void *(*)(void *), void *) NOINLINE;
void pthread_exit(void *) NOINLINE;
int pthread_join(pthread_t, void **) NOINLINE;
int pthread_detach(pthread_t thread) NOINLINE;
pthread_t pthread_self(void);

int pthread_attr_destroy(pthread_attr_t *);
int pthread_attr_init(pthread_attr_t *);

int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *) NOINLINE;
int pthread_mutex_lock(pthread_mutex_t *) NOINLINE;
int pthread_mutex_trylock(pthread_mutex_t *) NOINLINE;
int pthread_mutex_unlock(pthread_mutex_t *) NOINLINE;

int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);

int pthread_key_create(pthread_key_t *, void (*)(void *)) NOINLINE;
int pthread_key_delete(pthread_key_t) NOINLINE;
int pthread_setspecific(pthread_key_t, const void *) NOINLINE;
void *pthread_getspecific(pthread_key_t) NOINLINE;

int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_signal(pthread_cond_t *) NOINLINE;
int pthread_cond_broadcast(pthread_cond_t *) NOINLINE;
int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *) NOINLINE;

int pthread_once(pthread_once_t *, void (*)(void)) NOINLINE;

#ifdef NEW_INTERP_BUGS
void _ZSt9terminatev( void );
#endif

#ifdef __cplusplus
}
#endif
#endif
