// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>

/* Includes */
#include "pthread.h"
#include <errno.h>

#ifndef NO_JMP
#include <setjmp.h>
#endif

/* Macros */
#define _WAIT( cond, cancel_point )                                            \
                do {                                                           \
                    while ( ( cond ) && ( !cancel_point || !_canceled() ) ) {  \
                        __divine_interrupt_unmask();                           \
                        /* Control flow cycle forces an intermediate state */  \
                        /* to be generated here.                           */  \
                        __divine_interrupt_mask();                             \
                    }                                                          \
                    if ( cancel_point && _canceled() )                         \
                          _cancel();                                           \
                } while( 0 );

#define WAIT( cond ) _WAIT( cond, false )
#define WAIT_OR_CANCEL( cond ) _WAIT( cond, true )

#define PTHREAD_FUN_BEGIN()         do { ATOMIC_FUN_BEGIN( false ); \
                                         _initialize(); } while( 0 );

#define PTHREAD_VISIBLE_FUN_BEGIN() do { ATOMIC_FUN_BEGIN( true ); \
                                         _initialize(); } while( 0 );

// LTID = local thread ID - unique for thread lifetime (returned by __divine_get_tid)
// GTID = global thread ID - unique during the entire execution
// PTID = pthread_t = GTID (16b) + LTID (16b)
#define GTID( PTID )        ( PTID >> 16 )
#define LTID( PTID )        ( PTID & 0xFFFF )
#define PTID( GTID, LTID )  ( ( GTID << 16 ) | LTID )

// thresholds
#define MILLIARD  1000000000

// bit masks
#define _THREAD_ATTR_DETACH_MASK   0x1

#define _MUTEX_OWNER_MASK          0xFFFF
#define _MUTEX_COUNTER_MASK        0xFF0000
#define _MUTEX_TYPE_MASK           0x3000000
#define _MUTEX_ATTR_TYPE_MASK      0x3

#define _COND_COUNTER_MASK         0xFFFF

#define _WLOCK_WRITER_MASK         0xFFFF
#define _WLOCK_SHARING_MASK        0x10000
#define _RLOCK_READER_MASK         0xFFFF
#define _RLOCK_COUNTER_MASK        0xFF0000
#define _RWLOCK_ATTR_SHARING_MASK  0x1

#define _BARRIER_COUNT_MASK        0xFFFE0000

/* TEMPORARY */
#ifdef NEW_INTERP_BUGS
#define bool int
#endif

/* Internal data types */
struct Entry {
    void *(*entry)(void *);
    void *arg;
    bool initialized;
};

struct CleanupHandler {
    void (*routine)(void *);
    void *arg;
    CleanupHandler *next;
};

struct Thread { // (user-space) information maintained for every (running) thread
    // global thread ID
    int gtid;

    // join & detach
    bool running;
    bool detached;
    void *result;

#ifndef NO_JMP
    // exit -> back to the entry wrapper
    jmp_buf entry_buf;
#endif

    // conditional variables
    bool sleeping;
    pthread_cond_t* condition;

    // cancellation
#ifdef NEW_INTERP_BUGS
    int cancel_state;
    int cancel_type;
#else
    int cancel_state:1;
    int cancel_type:1;
#endif
    bool cancelled;
    CleanupHandler *cleanup_handlers;
};

namespace {

/* Internal globals*/
bool initialized = false;
unsigned alloc_pslots = 0; // num. of pointers (not actuall slots) allocated
unsigned thread_counter = 1;
Thread ** threads = NULL;
pthread_key_t keys = NULL;

}

/* Helper functions */
template< typename T >
T* realloc( T* old_ptr, unsigned old_count, unsigned new_count ) {
    T* new_ptr = static_cast< T* >( __divine_malloc( sizeof( T ) * new_count ) );
    if ( old_ptr ) {
#ifdef NEW_INTERP_BUGS
        for (int i = 0; i < ( old_count < new_count ? old_count : new_count ); i++)
            new_ptr[i] = old_ptr[i]; // only basic types are copied
#else
        memcpy( static_cast< void* >( &new_ptr ), static_cast< void* >( &old_ptr ),
                sizeof( T ) * ( old_count < new_count ? old_count : new_count ));
#endif
        __divine_free( old_ptr );
    }    
    return new_ptr;
}

/* Process */
int pthread_atfork( void (*)(void), void (*)(void), void(*)(void) ) {
    /* TODO */
    return 0;
}

/* Thread */
int _get_gtid( const int ltid ) {
#ifndef NEW_INTERP_BUGS
    DBG_ASSERT( ( ltid >= 0 ) && ( ltid < alloc_pslots ) );
#endif
    if ( threads[ltid] != NULL ) {
        int gtid = threads[ltid]->gtid;
#ifndef NEW_INTERP_BUGS
        DBG_ASSERT( gtid >= 0 && gtid < thread_counter );
#endif
        return gtid;
    } else
        return -1; // invalid gtid
}

void _init_thread( const int gtid, const int ltid, const pthread_attr_t attr ) {
    DBG_ASSERT( ltid >= 0 && gtid >= 0 );
    pthread_key_t key;

    // reallocate thread local storage if neccessary
    if ( ltid >= alloc_pslots ) {
        DBG_ASSERT( ltid == alloc_pslots ); // shouldn't skip unallocated slots
        int new_count = ltid + 1;

        // thread metadata
        threads = realloc< Thread* >( threads, alloc_pslots, new_count );
        threads[ltid] = NULL;

        // TLS
        key = keys;
        while ( key ) {
            key->data = realloc< void* >( key->data, alloc_pslots, new_count );
            key = key->next;
        }
        alloc_pslots = new_count;
    }

    // allocate slot for thread metadata
    DBG_ASSERT( threads[ltid] == NULL );
    threads[ltid] = static_cast< Thread* >( __divine_malloc( sizeof( Thread ) ) );
    DBG_ASSERT( threads[ltid] != NULL );

    // initialize thread metadata
    Thread* thread = threads[ltid];
    DBG_ASSERT( thread != NULL );
    thread->gtid = gtid;
    thread->running = false;
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->sleeping = false;
    thread->result = NULL;
    // thread->condition = NULL; (FIXME: memset (hmm?) is not yet supported)
    thread->cancelled = false;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
    thread->cleanup_handlers = NULL;

    // associate value NULL with all defined keys for this new thread
    key = keys;
    while ( key ) {
        key->data[ltid] = NULL;
        key = key->next;
    }
}

void _initialize( void )  {
    if ( !initialized ) {
        // initialize implicitly created main thread
        DBG_ASSERT( alloc_pslots == 0 );
        _init_thread( 0, 0, PTHREAD_CREATE_DETACHED );
        threads[0]->running = true;

        // etc... more initialization steps might come here

        // check of some assumptions
        __divine_assert( sizeof(int) >= 4 );

        initialized = true;
    }
}

void _cleanup() {
    int ltid = __divine_get_tid();

    CleanupHandler* handler = threads[ltid]->cleanup_handlers;
    threads[ltid]->cleanup_handlers = NULL;
    CleanupHandler* next;

    while ( handler ) {
        next = handler->next;
        handler->routine( handler->arg );
        __divine_free( handler );
        handler = next;
    }
}

void _cancel() {
    int ltid = __divine_get_tid();
    threads[ltid]->sleeping = false;

    // call all cleanup handlers
    _cleanup();

    // return to _pthread_entry
#ifndef NO_JMP
    longjmp( threads[ltid]->entry_buf, 1 );
#endif
}

bool _canceled() {
    int ltid = __divine_get_tid();
    return threads[ltid]->cancelled;
}

void _pthread_entry( void *_args )
{
    PTHREAD_FUN_BEGIN()

    Entry *args = static_cast< Entry* >( _args );
    int ltid = __divine_get_tid();
    DBG_ASSERT( ltid < alloc_pslots );
    Thread* thread = threads[ltid];
    DBG_ASSERT( thread != NULL );
    pthread_key_t key;

    // copy arguments
    void * arg = args->arg;
    void *(*entry)(void *) = args->entry;

    // parent may delete args and leave pthread_create now
    args->initialized = true;

    // call entry function
    thread->running = true;
#ifndef NO_JMP
    if ( !setjmp( thread->entry_buf ) ) {
#endif
        __divine_interrupt_unmask();
        // from now on, args and _args should not be accessed

        thread->result = entry(arg);
#ifndef NO_JMP
    } // else: exited by pthread_exit or cancelled
#endif
    __divine_interrupt_mask();

    DBG_ASSERT( thread->sleeping == false );

    // all thread specific data destructors are run
    key = keys;
    while ( key ) {
        void* data = pthread_getspecific( key );
        if ( data ) {
            pthread_setspecific( key, NULL );
            if ( key->destructor ) {
                int iter = 0;
                while (data && iter < PTHREAD_DESTRUCTOR_ITERATIONS) {
                    (key->destructor)( data );
                    data = pthread_getspecific( key );
                    pthread_setspecific( key, NULL );
                    ++iter;
                }
            }
        }
        key = key->next;
    }

    thread->running = false;

    // wait until detach / join
    WAIT( !thread->detached )

    // cleanup
    __divine_free( thread );
    threads[ltid] = NULL;
}

int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *(*entry)(void *),
                    void *arg ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    DBG_ASSERT( alloc_pslots > 0 );

    // test input arguments
    if ( ptid == NULL || entry == NULL )
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry* args = static_cast< Entry* >( __divine_malloc( sizeof( Entry ) ) );
    args->entry = entry;
    args->arg = arg;
    args->initialized = false;
    int ltid = __divine_new_thread( _pthread_entry, static_cast< void* >( args ) );
    DBG_ASSERT( ltid >= 0 );

    // generate a unique ID
    int gtid = thread_counter++;    
    // 65535 is in fact the maximum number of threads (created during the entire execution)
    // we can handle (capacity of pthread_t, mutex & rwlock types are limiting factors).
    __divine_assert( thread_counter < (1 << 16) );
    *ptid = PTID( gtid, ltid );

    // thread initialization
    _init_thread( gtid, ltid, ( attr == NULL ? PTHREAD_CREATE_JOINABLE : *attr ) );
    DBG_ASSERT( ltid < alloc_pslots );

    WAIT( !args->initialized )  // wait, do not free args yet

    // cleanup and return
    __divine_free( args );
    return 0;
}

void pthread_exit( void *result ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = __divine_get_tid();
    int gtid = _get_gtid( ltid );

    threads[ltid]->result = result;

    if (gtid == 0) {
        // join every other thread and exit
        for ( int i = 1; i < alloc_pslots; i++ ) {
            WAIT_OR_CANCEL( threads[i] && threads[i]->running )
        }

        // call all cleanup handlers
        _cleanup();

        // FIXME: how to return from main()?

    } else {
        // call all cleanup handlers
        _cleanup();

        // return to _pthread_entry
#ifndef NO_JMP
        longjmp( threads[ltid]->entry_buf, 1 );
#endif
    }
}

int pthread_join( pthread_t ptid, void **result ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = LTID( ptid );
    int gtid = GTID( ptid );

    if ( ( ltid < 0 ) || ( ltid >= alloc_pslots ) ||
         ( gtid < 0 ) || ( gtid >= thread_counter ))
        return EINVAL;

    if ( gtid != _get_gtid( ltid ) )
        return ESRCH;

    if ( gtid == _get_gtid( __divine_get_tid() ) )
        return EDEADLK;

    if ( threads[ltid]->detached )
        return EINVAL;

    // wait for the thread to finnish
    WAIT_OR_CANCEL( threads[ltid]->running )

    if ( ( gtid != _get_gtid( ltid ) ) ||
         ( threads[ltid]->detached ) ) {
        // meanwhile detached
        return EINVAL;
    }

    // copy result
    if (result) {
        if ( threads[ltid]->cancelled )
            *result = PTHREAD_CANCELED;
        else
            *result = threads[ltid]->result;
    }

    // let the thread to terminate now
    threads[ltid]->detached = true;
    return 0;
}

int pthread_detach( pthread_t ptid ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = LTID( ptid );
    int gtid = GTID( ptid );

    if ( ( ltid < 0 ) || ( ltid >= alloc_pslots ) ||
         ( gtid < 0 ) || ( gtid >= thread_counter ))
        return EINVAL;

    if ( gtid != _get_gtid( ltid ) )
        return ESRCH;

    if ( threads[ltid]->detached )
        return EINVAL;

    threads[ltid]->detached = true;
    return 0;
}

/* Thread attributes */

/*
   pthread_attr_t representation:
     ------------------------------
    | *free* | detach state: 1 bit |
     ------------------------------
  */

int pthread_attr_destroy( pthread_attr_t * ) {
    PTHREAD_FUN_BEGIN()
    return 0;
}

int pthread_attr_init( pthread_attr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    *attr = 0;
    return 0;
}

int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *state) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || state == NULL )
        return EINVAL;

    *state = *attr & _THREAD_ATTR_DETACH_MASK;
    return 0;
}

int pthread_attr_getguardsize( const pthread_attr_t *, size_t *) {
    /* TODO */
    return 0;
}

int pthread_attr_getinheritsched( const pthread_attr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_attr_getschedparam( const pthread_attr_t *, struct sched_param * ) {
    /* TODO */
    return 0;
}

int pthread_attr_getschedpolicy( const pthread_attr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_attr_getscope( const pthread_attr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_attr_getstack( const pthread_attr_t *, void **, size_t * ) {
    /* TODO */
    return 0;
}

int pthread_attr_getstackaddr( const pthread_attr_t *, void ** ) {
    /* TODO */
    return 0;
}

int pthread_attr_getstacksize( const pthread_attr_t *, size_t * ) {
    /* TODO */
    return 0;
}

int pthread_attr_setdetachstate( pthread_attr_t *attr, int state) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || ( state & ~_THREAD_ATTR_DETACH_MASK ) )
        return EINVAL;

    *attr &= ~_THREAD_ATTR_DETACH_MASK;
    *attr |= state;
    return 0;
}

int pthread_attr_setguardsize( pthread_attr_t *, size_t ) {
    /* TODO */
    return 0;
}

int pthread_attr_setinheritsched( pthread_attr_t *, int ) {
    /* TODO */
    return 0;
}

int pthread_attr_setschedparam( pthread_attr_t *, const struct sched_param * ) {
    /* TODO */
    return 0;
}

int pthread_attr_setschedpolicy( pthread_attr_t *, int ) {
    /* TODO */
    return 0;
}

int pthread_attr_setscope( pthread_attr_t *, int ) {
    /* TODO */
    return 0;
}

int pthread_attr_setstack( pthread_attr_t *, void *, size_t ) {
    /* TODO */
    return 0;
}

int pthread_attr_setstackaddr( pthread_attr_t *, void * ) {
    /* TODO */
    return 0;
}

int pthread_attr_setstacksize( pthread_attr_t *, size_t ) {
    /* TODO */
    return 0;
}

/* Thread ID */
pthread_t pthread_self( void ) {
    PTHREAD_FUN_BEGIN()
    int ltid = __divine_get_tid();
    return PTID( _get_gtid( ltid ), ltid );
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return GTID( t1 ) == GTID( t2 );
}

/* Scheduler */
int pthread_getconcurrency( void ) {
    /* TODO */
    return 0;
}

int pthread_getcpuclockid( pthread_t, clockid_t * ) {
    /* TODO */
    return 0;
}

int pthread_getschedparam( pthread_t, int *, struct sched_param * ) {
    /* TODO */
    return 0;
}

int pthread_setconcurrency( int ) {
    /* TODO */
    return 0;
}

int pthread_setschedparam( pthread_t, int, const struct sched_param * ) {
    /* TODO */
    return 0;
}

int pthread_setschedprio( pthread_t, int ) {
    /* TODO */
    return 0;
}

/* Mutex */

/*
  pthread_mutex_t representation:
  -----------------------------------------------------------------------------------------------
 | *free* | initialized?: 1 bit | type: 2 bits | lock counter: 8 bits | (thread_id + 1): 16 bits |
  -----------------------------------------------------------------------------------------------
  */

int _mutex_adjust_count( pthread_mutex_t *mutex, int adj ) {
    int count = ( (*mutex) & _MUTEX_COUNTER_MASK ) >> 16;
    count += adj;
    if ( count >= ( 1 << 8 ) )
        return EAGAIN;

    (*mutex) &= ~_MUTEX_COUNTER_MASK;
    (*mutex) |= count << 16;
    return 0;
}

bool _mutex_can_lock( pthread_mutex_t *mutex, int gtid ) {
    return !( (*mutex) & _MUTEX_OWNER_MASK ) || ( ( (*mutex) & _MUTEX_OWNER_MASK ) == ( gtid + 1 ) );
}

int _mutex_lock(pthread_mutex_t *mutex, bool wait) {
    int gtid = _get_gtid( __divine_get_tid() );

    if ( mutex == NULL || !( (*mutex) & _INITIALIZED_MUTEX ) ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( ( (*mutex) & _MUTEX_OWNER_MASK ) == gtid + 1 ) {
        // already locked by this thread
        DBG_ASSERT( (*mutex) & _MUTEX_COUNTER_MASK ); // count should be > 0
        if ( ( *mutex & _MUTEX_TYPE_MASK ) != ( PTHREAD_MUTEX_RECURSIVE << 24 ) ) {
            if ( ( *mutex & _MUTEX_TYPE_MASK ) == ( PTHREAD_MUTEX_ERRORCHECK << 24 ) )
                return EDEADLK;
            else
                __divine_assert( 0 );
        }
    }

    if ( !_mutex_can_lock( mutex, gtid ) ) {
        DBG_ASSERT( (*mutex) & _MUTEX_COUNTER_MASK ); // count should be > 0
        if ( !wait )
            return EBUSY;
    }
    WAIT ( !_mutex_can_lock( mutex, gtid ) )

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if ( err )
        return err;

    // lock the mutex
    (*mutex) &= ~_MUTEX_OWNER_MASK;
    (*mutex) |= gtid + 1;

    return 0;
}

int pthread_mutex_destroy( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( mutex == NULL )
        return EINVAL;

    if ( (*mutex) & _MUTEX_OWNER_MASK ) {
        // mutex is locked
        if ( ( *mutex & _MUTEX_TYPE_MASK ) == ( PTHREAD_MUTEX_ERRORCHECK << 24 ) )
             return EBUSY;
        else
             __divine_assert( 0 );
    }
    *mutex &= ~_INITIALIZED_MUTEX;
    return 0;
}

int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( mutex == NULL )
        return EINVAL;

    if ( (*mutex) & _INITIALIZED_MUTEX ) {
        // already initialized
        if ( ( *mutex & _MUTEX_TYPE_MASK ) == ( PTHREAD_MUTEX_ERRORCHECK << 24 ) )
            return EBUSY;
    }

    if ( attr )
        *mutex = ( *attr & _MUTEX_ATTR_TYPE_MASK ) << 24;
    else
        *mutex = PTHREAD_MUTEX_DEFAULT << 24;
    *mutex |= _INITIALIZED_MUTEX;
    return 0;
}

int pthread_mutex_lock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _mutex_lock( mutex, 1 );
}

int pthread_mutex_trylock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _mutex_lock( mutex, 0 );
}

int pthread_mutex_unlock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    int gtid = _get_gtid( __divine_get_tid() );

    if ( mutex == NULL || !( (*mutex) & _INITIALIZED_MUTEX ) ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( ( (*mutex) & _MUTEX_OWNER_MASK ) != ( gtid + 1 ) ) {
        // mutex is not locked or it is already locked by another thread
        DBG_ASSERT( (*mutex) & _MUTEX_COUNTER_MASK ); // count should be > 0
        if ( ( *mutex & _MUTEX_TYPE_MASK ) == ( PTHREAD_MUTEX_NORMAL << 24 ) )
             __divine_assert( 0 );
        else
             return EPERM; // recursive mutex can also detect
    }

    _mutex_adjust_count( mutex, -1 );
    if ( !( (*mutex) & _MUTEX_COUNTER_MASK ) )
        *mutex &= ~_MUTEX_OWNER_MASK; // unlock if count == 0
    return 0;
}

int pthread_mutex_getprioceiling( const pthread_mutex_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_mutex_setprioceiling( pthread_mutex_t *, int, int * ) {
    /* TODO */
    return 0;
}

int pthread_mutex_timedlock( pthread_mutex_t *mutex, const struct timespec *abstime ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( abstime == NULL || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD ) {
        return EINVAL;
    }

    // Consider both scenarios, one in which the mutex could not be locked
    // before the specified timeout expired, and the other in which
    // pthread_mutex_timedlock behaves basically the same as pthread_mutex_lock.
    if ( __divine_choice( 2 ) ) {
        return pthread_mutex_lock( mutex );
    } else {
        return ETIMEDOUT;
    }
}

/* Mutex attributes */

/*
  pthread_mutexattr_t representation:
  -----------------------------
 | *free* | mutex type: 2 bits |
  -----------------------------
*/


int pthread_mutexattr_destroy( pthread_mutexattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL )
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_mutexattr_init( pthread_mutexattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *value ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || value == NULL )
        return EINVAL;

    *value = *attr & _MUTEX_ATTR_TYPE_MASK;
    return 0;
}

int pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getprotocol( const pthread_mutexattr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getpshared( const pthread_mutexattr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_mutexattr_settype( pthread_mutexattr_t *attr, int value ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || ( value & ~_MUTEX_ATTR_TYPE_MASK ) )
        return EINVAL;

    *attr &= ~_MUTEX_ATTR_TYPE_MASK;
    *attr |= value;
    return 0;
}

int pthread_mutexattr_setprioceiling( pthread_mutexattr_t *, int ) {
    /* TODO */
    return 0;
}

int pthread_mutexattr_setprotocol( pthread_mutexattr_t *, int ) {
    /* TODO */
    return 0;

}

int pthread_mutexattr_setpshared( pthread_mutexattr_t *, int ) {
    /* TODO */
    return 0;
}

/* Spin lock */

int pthread_spin_destroy( pthread_spinlock_t * ) {
    /* TODO */
    return 0;
}

int pthread_spin_init( pthread_spinlock_t *, int ) {
    /* TODO */
    return 0;
}

int pthread_spin_lock( pthread_spinlock_t * ) {
    /* TODO */
    return 0;
}

int pthread_spin_trylock( pthread_spinlock_t * ) {
    /* TODO */
    return 0;
}

int pthread_spin_unlock( pthread_spinlock_t * ) {
    /* TODO */
    return 0;
}

/* Thread specific data */
int pthread_key_create( pthread_key_t *p_key, void (*destructor)(void *) ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    // malloc
    void* _key = __divine_malloc( sizeof( _PerThreadData ) );
    pthread_key_t key = static_cast< pthread_key_t >( _key );
    if ( alloc_pslots ) {
        key->data = static_cast< void ** >
                        ( __divine_malloc( sizeof( void* ) * alloc_pslots ) );
    } else key->data = NULL;

    // initialize
    key->destructor = destructor;
    for ( int i = 0; i < alloc_pslots; ++i ) {
        key->data[i] = NULL;
    }

    // insert into double-linked list
    key->next = keys;
    if ( key->next )
        key->next->prev = key;
    key->prev = NULL;
    keys = key;

    // return
    *p_key = key;
    return 0;
}

int pthread_key_delete( pthread_key_t key ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( key == NULL )
        return EINVAL;

    // remove from double-linked list and free
    if ( keys == key ) {
        keys = key->next;
    }
    if ( key->next ) {
        key->next->prev = key->prev;
    }
    if ( key->prev ) {
        key->prev->next = key->next;
    }

    __divine_free( key->data );
    __divine_free( key );
    return 0;
}

int pthread_setspecific( pthread_key_t key, const void *data ) {
    PTHREAD_FUN_BEGIN()

    if (key == NULL)
        return EINVAL;

    int ltid = __divine_get_tid();
    DBG_ASSERT( ltid < alloc_pslots );

    key->data[ltid] = const_cast< void* >( data );
    return 0;
}

void *pthread_getspecific( pthread_key_t key ) {
    PTHREAD_FUN_BEGIN()
    __divine_assert(key != NULL);

    int ltid = __divine_get_tid();
    DBG_ASSERT( ltid < alloc_pslots );

    return key->data[ltid];
}

/* Conditional variables */

/*
  pthread_cond_t representation:

    { .mutex: address of associated mutex
      .counter:
          -------------------------------------------------------------------
         | *free* | initialized?: 1 bit | number of waiting threads: 16 bits |
          -------------------------------------------------------------------
      }
*/

int _cond_adjust_count( pthread_cond_t *cond, int adj ) {
    int count = cond->counter & _COND_COUNTER_MASK;
    count += adj;
    __divine_assert( count < ( 1 << 16 ) );

    (cond->counter) &= ~_COND_COUNTER_MASK;
    (cond->counter) |= count;
    return count;
}

int pthread_cond_destroy( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( cond == NULL || !( cond->counter & _INITIALIZED_COND ) )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    __divine_assert( ( cond->counter & _COND_COUNTER_MASK ) == 0 );

    cond->mutex = NULL;
    cond->counter = 0;
    return 0;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */ ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( cond == NULL )
        return EINVAL;

    if ( cond->counter & _INITIALIZED_COND )
        return EBUSY; // already initialized

    cond->mutex = NULL;
    cond->counter = _INITIALIZED_COND;
    return 0;
}

int _cond_signal( pthread_cond_t *cond, bool broadcast = false ) {
    if ( cond == NULL || !( cond->counter & _INITIALIZED_COND ) )
        return EINVAL;

    int count = cond->counter & _COND_COUNTER_MASK;
    if ( count )  { // some threads are waiting for condition
        int waiting = 0, wokenup = 0, choice;

        if ( !broadcast ) {
            /* non-determinism */
            choice = __divine_choice( ( 1 << count ) - 1 );
        }

        for ( int i = 0; i < alloc_pslots; ++i ) {
            if ( !threads[i] ) // empty slot
                continue;

            if ( threads[i]->sleeping && threads[i]->condition == cond ) {
                ++waiting;
                if ( ( broadcast ) ||
                     ( ( choice + 1 ) & ( 1 << (waiting - 1) ) ) ) {
                   // wake up the thread
                   threads[i]->sleeping = false;
                   threads[i]->condition = NULL;
                   ++wokenup;
                }
            }
        }

        DBG_ASSERT( count == waiting );

        if ( !_cond_adjust_count( cond,-wokenup ) )
            cond->mutex = NULL; // break binding between cond. variable and mutex
    }
    return 0;
}

int pthread_cond_signal( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _cond_signal( cond );
}

int pthread_cond_broadcast( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _cond_signal( cond, true );
}

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = __divine_get_tid();
    int gtid = _get_gtid( __divine_get_tid() );

    if ( cond == NULL || !( cond->counter & _INITIALIZED_COND ) )
        return EINVAL;

    if ( mutex == NULL || !( (*mutex) & _INITIALIZED_MUTEX ) ) {
        return EINVAL;
    }

    if ( ( (*mutex) & _COND_COUNTER_MASK ) != ( gtid + 1 ) ) {
        // mutex is not locked or it is already locked by another thread
        return EPERM;
    }

    // It is allowed to have one mutex associated with more than one conditional
    // variable. On the other hand, using more than one mutex for one
    // conditional variable results in undefined behaviour.
    __divine_assert( cond->mutex == NULL || cond->mutex == mutex );
    cond->mutex = mutex;

    // fall asleep
    Thread* thread = threads[ltid];
    thread->sleeping = true;
    thread->condition = cond;

    _cond_adjust_count( cond, 1 ); // one more thread is waiting for this condition
    pthread_mutex_unlock( mutex ); // unlock associated mutex

    // sleeping
    WAIT_OR_CANCEL( thread->sleeping )

    // try to lock mutex which was associated to the cond. variable by this thread
    // (not the one from current binding, which may have changed)
    pthread_mutex_lock( mutex );
    return 0;
}

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex,
                            const struct timespec * abstime) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( abstime == NULL || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD ) {
        return EINVAL;
    }

    // Consider both scenarios, one in which the time specified by abstime has passed
    // before the function could finish, and the other in which
    // pthread_cond_timedwait behaves basically the same as pthread_cond_wait.
    if ( __divine_choice( 2 ) ) {
        return pthread_cond_wait( cond, mutex );
    } else {
        return ETIMEDOUT;
    }
}

/* Attributes of conditional variables */
int pthread_condattr_destroy( pthread_condattr_t * ) {
    /* TODO */
    return 0;
}

int pthread_condattr_getclock( const pthread_condattr_t *, clockid_t * ) {
    /* TODO */
    return 0;
}

int pthread_condattr_getpshared( const pthread_condattr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_condattr_init( pthread_condattr_t * ) {
    /* TODO */
    return 0;
}

int pthread_condattr_setclock( pthread_condattr_t *, clockid_t ) {
    /* TODO */
    return 0;
}

int pthread_condattr_setpshared( pthread_condattr_t *, int ) {
    /* TODO */
    return 0;
}

/* Once-only execution */
int pthread_once( pthread_once_t *once_control, void (*init_routine)(void) ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if (*once_control) {
        *once_control = 0;
        init_routine();
    }
    return 0;
}

/* Thread cancellation */
int pthread_setcancelstate( int state, int *oldstate ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if (state & ~0x1)
        return EINVAL;

    int ltid = __divine_get_tid();
    *oldstate = threads[ltid]->cancel_state;
    threads[ltid]->cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if (type & ~0x1)
        return EINVAL;

    int ltid = __divine_get_tid();
    *oldtype = threads[ltid]->cancel_type;
    threads[ltid]->cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t ptid ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = LTID( ptid );
    int gtid = GTID( ptid );

    if ( ( ltid < 0 ) || ( ltid >= alloc_pslots ) ||
         ( gtid < 0 ) || ( gtid >= thread_counter ))
        return ESRCH;

    if ( gtid != _get_gtid( ltid ) )
        return ESRCH;

    if ( threads[ltid]->cancel_state == PTHREAD_CANCEL_ENABLE )
        threads[ltid]->cancelled = true;

    return 0;
}

void pthread_testcancel( void ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if (_canceled())
        _cancel();
}

void pthread_cleanup_push( void (*routine)(void *), void *arg ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    __divine_assert( routine != NULL );

    int ltid = __divine_get_tid();
    CleanupHandler* handler = reinterpret_cast< CleanupHandler* >( __divine_malloc( sizeof(CleanupHandler) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = threads[ltid]->cleanup_handlers;
    threads[ltid]->cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int ltid = __divine_get_tid();
    CleanupHandler* handler = threads[ltid]->cleanup_handlers;
    if ( handler != NULL ) {
        threads[ltid]->cleanup_handlers = handler->next;
        if ( execute ) {
            handler->routine( handler->arg );
        }
        __divine_free( handler );
    }
}

/* Readers-Writer lock */

/*
  pthread_rwlock_t representation:

    { .wlock: information about writer and lock itself
          ---------------------------------------------------------------------------
         | *free* | initialized?: 1 bit | sharing: 1 bit | (writer gid + 1): 16 bits |
          ---------------------------------------------------------------------------
      .rlocks: linked list of nodes, each one representing a thread holding the read lock
         _ReadLock representation:
           {
             .rlock: information about reader
                 -----------------------------------------------------------
                | *free* | lock counter: 8 bits | (reader gid + 1): 16 bits |
                 -----------------------------------------------------------
             .next: pointer to the next node, if there is any (representing another thread)
            }
     }
 */

int _rlock_adjust_count( _ReadLock *rlock, int adj ) {
    int count = ( rlock->rlock & _RLOCK_COUNTER_MASK ) >> 16;
    count += adj;
    if ( count >= ( 1 << 8 ) )
        return EAGAIN;

    rlock->rlock &= ~_RLOCK_COUNTER_MASK;
    rlock->rlock |= count << 16;
    return 0;
}

_ReadLock *_get_rlock( pthread_rwlock_t *rwlock, int gtid, _ReadLock **prev = NULL ) {
    _ReadLock *rlock = rwlock->rlocks;
    _ReadLock *_prev = NULL;

    while ( rlock && ( ( rlock->rlock & _RLOCK_READER_MASK ) != gtid + 1 ) ) {
        _prev = rlock;
        rlock = rlock->next;
    }

    if ( rlock && prev )
        *prev = _prev;

    return rlock;
}

_ReadLock *_create_rlock( pthread_rwlock_t *rwlock, int gtid ) {
    _ReadLock *rlock = reinterpret_cast< _ReadLock* >( __divine_malloc( sizeof(_ReadLock) ) );
    rlock->rlock = gtid + 1;
    rlock->rlock |= 1 << 16;
    rlock->next = rwlock->rlocks;
    rwlock->rlocks = rlock;
    return rlock;
}

bool _rwlock_can_lock( pthread_rwlock_t *rwlock, bool writer ) {
    return !( rwlock->wlock & _WLOCK_WRITER_MASK ) && !( writer && rwlock->rlocks );
}

int _rwlock_lock( pthread_rwlock_t *rwlock, bool wait, bool writer ) {
    int gtid = _get_gtid( __divine_get_tid() );

    if ( rwlock == NULL || !( rwlock->wlock & _INITIALIZED_RWLOCK ) ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    if ( ( rwlock->wlock & _WLOCK_WRITER_MASK ) == gtid + 1 )
        // thread already owns write lock
        return EDEADLK;

    _ReadLock *rlock = _get_rlock( rwlock, gtid );
    // existing read lock implies read lock counter having value more than zero
    DBG_ASSERT( !rlock || ( rlock->rlock & _RLOCK_COUNTER_MASK ) );

    if ( writer && rlock )
        // thread already owns read lock
        return EDEADLK;

    if ( !_rwlock_can_lock( rwlock, writer ) ) {
        if ( !wait )
            return EBUSY;
    }
    WAIT ( !_rwlock_can_lock( rwlock, writer ) )

    if ( writer ) {
        rwlock->wlock &= ~_WLOCK_WRITER_MASK;
        rwlock->wlock |= gtid + 1;
    } else {
        if ( !rlock )
            rlock = _create_rlock( rwlock, gtid );
        else {
            int err = _rlock_adjust_count( rlock, 1 );
            if ( err )
                return err;
        }
    }

    return 0;
}

int pthread_rwlock_destroy( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( rwlock == NULL )
        return EINVAL;

    if ( ( rwlock->wlock & _WLOCK_WRITER_MASK ) || ( rwlock->rlocks ) ) {
        // rwlock is locked
        return EBUSY;
    }

    rwlock->wlock &= ~_INITIALIZED_RWLOCK;
    return 0;
}

int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( rwlock == NULL )
        return EINVAL;

    if ( rwlock->wlock & _INITIALIZED_RWLOCK ) {
        // already initialized
        return EBUSY;
    }

    if ( attr )
        rwlock->wlock = ( *attr & _RWLOCK_ATTR_SHARING_MASK ) << 16;
    else
        rwlock->wlock = PTHREAD_PROCESS_PRIVATE << 16;
    rwlock->wlock |= _INITIALIZED_RWLOCK;
    rwlock->rlocks = NULL;
    return 0;
}

int pthread_rwlock_rdlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _rwlock_lock( rwlock, true, false );
}

int pthread_rwlock_wrlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _rwlock_lock( rwlock, true, true );
}

int pthread_rwlock_tryrdlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _rwlock_lock( rwlock, false, false );
}

int pthread_rwlock_trywrlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()
    return _rwlock_lock( rwlock, false, true );
}

int pthread_rwlock_unlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    int gtid = _get_gtid( __divine_get_tid() );

    if ( rwlock == NULL || !( rwlock->wlock & _INITIALIZED_RWLOCK ) ) {
          return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    _ReadLock *rlock, *prev;
    rlock = _get_rlock( rwlock, gtid, &prev );
    // existing read lock implies read lock counter having value more than zero
    DBG_ASSERT( !rlock || ( rlock->rlock & _RLOCK_COUNTER_MASK ) );

    if ( ( ( rwlock->wlock & _WLOCK_WRITER_MASK ) != gtid + 1 ) && !rlock ) {
        // the current thread does not own the read-write lock
        return EPERM;
    }

    if ( ( rwlock->wlock & _WLOCK_WRITER_MASK ) == gtid + 1 ) {
        DBG_ASSERT( !rlock );
        // release write lock
        rwlock->wlock &= ~_WLOCK_WRITER_MASK;
    } else {
        // release read lock
        _rlock_adjust_count( rlock, -1 );
        if ( !( rlock->rlock & _RLOCK_COUNTER_MASK ) ) {
            // no more read locks are owned by this thread
            if ( prev )
                prev->next = rlock->next;
            else
                rwlock->rlocks = rlock->next;
            __divine_free( rlock );
        }
    }
    return 0;
}

/* Readers-Write lock attributes */

/*
  pthread_rwlockattr_t representation:
     -------------------------
    | *free* | sharing: 1 bit |
     -------------------------
*/

int pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL )
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || pshared == NULL )
        return EINVAL;

    *pshared = *attr & _RWLOCK_ATTR_SHARING_MASK;
    return 0;
}

int pthread_rwlockattr_init( pthread_rwlockattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( attr == NULL || ( pshared & ~_RWLOCK_ATTR_SHARING_MASK ) )
        return EINVAL;

    *attr &= ~_RWLOCK_ATTR_SHARING_MASK;
    *attr |= pshared;
    return 0;
}

/* Barrier */

/*
  pthread_barrier_t representation:

   { .counter:
        -----------------------------------------------------------------------------------------------
       | the number of threads to synchronize: 15 bits | < the rest is the same as in pthread_cond_t > |
        -----------------------------------------------------------------------------------------------
     .mutex: NOT USED
   }
*/

int pthread_barrier_destroy( pthread_barrier_t *barrier ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( barrier == NULL )
        return EINVAL;

    pthread_cond_destroy( barrier );
    return 0;
}

int pthread_barrier_init( pthread_barrier_t *barrier, const pthread_barrierattr_t * attr /* TODO: barrier attributes */,
                          unsigned count ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( count == 0 || barrier == NULL )
        return EINVAL;

    // make sure that no thread is blocked on the barrier
    // (probably better alternative when compared to: return EBUSY)
    __divine_assert( ( barrier->counter & _COND_COUNTER_MASK ) == 0 );

    // Set the number of threads that must call pthread_barrier_wait() before
    // any of them successfully return from the call.
    barrier->counter = ( count << 17 );
    barrier->counter |= _INITIALIZED_COND;
    return 0;
}

int pthread_barrier_wait( pthread_barrier_t *barrier ) {
    PTHREAD_VISIBLE_FUN_BEGIN()

    if ( barrier == NULL || !( barrier->counter & _INITIALIZED_COND ) )
        return EINVAL;

    int ltid = __divine_get_tid();
    int ret = 0;
    int counter = barrier->counter & _COND_COUNTER_MASK;
    int release_count = barrier->counter >> 17;

    if ( (counter + 1) == release_count ) {
        pthread_cond_broadcast( barrier );
        ret = PTHREAD_BARRIER_SERIAL_THREAD;
    } else {
        // WARNING: this is not immune from spurious wakeup.
        // As of this writing, spurious wakeup is not implemented (and probably never will).

        // fall asleep
        Thread* thread = threads[ltid];
        thread->sleeping = true;
        thread->condition = barrier;

        _cond_adjust_count( barrier, 1 ); // one more thread is blocked on this barrier

        // sleeping
        WAIT_OR_CANCEL( thread->sleeping )
    }

    return ret;
}

/* Barrier attributes */
int pthread_barrierattr_destroy( pthread_barrierattr_t * ) {
    /* TODO */
    return 0;
}

int pthread_barrierattr_getpshared( const pthread_barrierattr_t *, int * ) {
    /* TODO */
    return 0;
}

int pthread_barrierattr_init( pthread_barrierattr_t * ) {
    /* TODO */
    return 0;
}

int pthread_barrierattr_setpshared( pthread_barrierattr_t *, int ) {
    /* TODO */
    return 0;
}

/* POSIX Realtime Extension - sched.h */
int sched_get_priority_max(int) {
    /* TODO */
    return 0;
}

int sched_get_priority_min(int) {
    /* TODO */
    return 0;
}

int sched_getparam(pid_t, struct sched_param *) {
    /* TODO */
    return 0;
}

int sched_getscheduler(pid_t) {
    /* TODO */
    return 0;
}

int sched_rr_get_interval(pid_t, struct timespec *) {
    /* TODO */
    return 0;
}

int sched_setparam(pid_t, const struct sched_param *) {
    /* TODO */
    return 0;
}

int sched_setscheduler(pid_t, int, const struct sched_param *) {
    /* TODO */
    return 0;
}

int sched_yield(void) {
    /* TODO */
    return 0;
}
