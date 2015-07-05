// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014, 2015 Vladimír Štill <xstill@fi.muni.cz>

/* Includes */
#include <pthread.h>
#include <divine.h>
#include <errno.h>
#include <limits.h>
#include <divine/problem.h>
#include <csignal>
#include <cstdlib>
#include <algorithm>

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
                } while( 0 )

#define WAIT( cond ) _WAIT( cond, false )
#define WAIT_OR_CANCEL( cond ) _WAIT( cond, true )

#define PTHREAD_FUN_BEGIN()         do { ATOMIC_FUN_BEGIN( false ); \
                                         _initialize(); } while( 0 )

#define PTHREAD_VISIBLE_FUN_BEGIN() do { ATOMIC_FUN_BEGIN( true ); \
                                         _initialize(); } while( 0 )

// LTID = local thread ID - unique for thread lifetime (returned by __divine_get_tid)
// GTID = global thread ID - unique during the entire execution

// thresholds
#define MILLIARD  1000000000

#define _real_pt( ptid ) ((real_pthread_t){ .asint = (ptid) })

// bit masks
#define _THREAD_ATTR_DETACH_MASK   0x1

#define _MUTEX_ATTR_TYPE_MASK      0x3

#define _COND_COUNTER_MASK         0xFFFF

#define _WLOCK_WRITER_MASK         0xFFFF
#define _WLOCK_SHARING_MASK        0x10000
#define _RLOCK_READER_MASK         0xFFFF
#define _RLOCK_COUNTER_MASK        0xFF0000
#define _RWLOCK_ATTR_SHARING_MASK  0x1

#define _BARRIER_COUNT_MASK        0xFFFE0000

#define DBG_ASSERT( x ) __divine_assert( static_cast< int >( x ) )

#define ATOMIC_FUN_BEGIN( visible_effect )                        \
                do {                                              \
                    __divine_interrupt_mask();                    \
                    if ( visible_effect )                         \
                        /* FIXME: this should be automatically */ \
                        /*        detected in the system-space */ \
                        BREAK_MASK( __divine_interrupt() );       \
                } while ( 0 );

/* Some handy macros. */
#define BREAK_MASK( command )                    \
                do {                             \
                    __divine_interrupt_unmask(); \
                    command;                     \
                    __divine_interrupt_mask();   \
                } while( 0 );


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

enum SleepingOn { NotSleeping = 0, Condition, Barrier };

typedef void (*sighandler_t)( int );
typedef unsigned short ushort;

struct Thread { // (user-space) information maintained for every (running) thread
    void *result;
    pthread_mutex_t *waiting_mutex;
    CleanupHandler *cleanup_handlers;
    union {
        pthread_cond_t *condition;
        pthread_barrier_t *barrier;
    };
    sighandler_t *sighandlers;

    // global thread ID
    int gtid;

    bool running:1;
    bool detached:1;
    SleepingOn sleeping:2;
    bool cancelled:1;

    int cancel_state:1;
    int cancel_type:1;
    unsigned sigmaxused:5; // at most 32 signals

    void setSleeping( pthread_cond_t *cond ) {
        sleeping = Condition;
        condition = cond;
    }

    void setSleeping( pthread_barrier_t *bar ) {
        sleeping = Barrier;
        barrier = bar;
    }
};

namespace {

/* Internal globals*/
bool initialized = false;
unsigned alloc_pslots = 0; // num. of pointers (not actuall slots) allocated
unsigned thread_counter = 1;
Thread **threads = NULL;
pthread_key_t keys = NULL;
}

/* Helper functions */
template< typename T >
T* realloc( T* old_ptr, unsigned old_count, unsigned new_count ) {
    T* new_ptr = static_cast< T* >( __divine_malloc( sizeof( T ) * new_count ) );
    if ( old_ptr ) {
        memcpy( static_cast< void* >( new_ptr ), static_cast< void* >( old_ptr ),
                sizeof( T ) * ( old_count < new_count ? old_count : new_count ));
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
    DBG_ASSERT( ( ltid >= 0 ) && ( ltid < alloc_pslots ) );
    if ( threads[ltid] != NULL ) {
        int gtid = threads[ltid]->gtid;
        DBG_ASSERT( gtid >= 0 && gtid < thread_counter );
        return gtid;
    } else
        return -1; // invalid gtid
}

void _init_thread( const int gtid, const int ltid, const pthread_attr_t attr ) {
    DBG_ASSERT( ltid >= 0 && gtid >= 0 );
    pthread_key_t key;

    // reallocate thread local storage if neccessary
    if ( ltid >= alloc_pslots ) {
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
    memset( thread, 0, sizeof( Thread ) );

    thread->gtid = gtid;
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->condition = NULL;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
    thread->sighandlers = NULL;
    thread->sigmaxused = 0;

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
    threads[ ltid ]->sleeping = NotSleeping;

    // call all cleanup handlers
    _cleanup();
    __divine_unwind( 1, NULL );
}

bool _canceled() {
    int ltid = __divine_get_tid();
    return threads[ltid]->cancelled;
}

void _pthread_entry( void *_args )
{
    PTHREAD_FUN_BEGIN();

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
    __sync_synchronize();
    __divine_interrupt_unmask();
    // from now on, args and _args should not be accessed

    thread->result = entry(arg);
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
    __sync_synchronize();

    // wait until detach / join
    WAIT( !thread->detached );

    // cleanup
    __divine_free( thread );
    threads[ltid] = NULL;
    __sync_synchronize();
}

int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *(*entry)(void *),
                    void *arg ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
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
    // 2^16 - 1 is in fact the maximum number of threads (created during the entire execution)
    // we can handle (capacity of pthread_t, mutex & rwlock types are limiting factors).
    // at most 2^15 - 1 threads may exist at any moment
    if ( gtid >= (1 << 16) || ltid >= (1 << 15) )
        return EAGAIN;

    real_pthread_t rptid; /* bitfields must be explicitly zeroed */
    memset( &rptid, 0, sizeof( rptid ) );
    rptid.gtid = gtid;
    rptid.ltid = ltid;
    rptid.initialized = 1;
    *ptid = rptid.asint;

    // thread initialization
    _init_thread( gtid, ltid, ( attr == NULL ? PTHREAD_CREATE_JOINABLE : *attr ) );
    DBG_ASSERT( ltid < alloc_pslots );

    __sync_synchronize();
    WAIT( !args->initialized );  // wait, do not free args yet

    // cleanup and return
    __divine_free( args );
    return 0;
}

void pthread_exit( void *result ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    int ltid = __divine_get_tid();
    int gtid = _get_gtid( ltid );

    if (gtid == 0) {
        // join every other thread and exit
        for ( int i = 1; i < alloc_pslots; i++ ) {
            WAIT_OR_CANCEL( threads[i] && threads[i]->running );
        }

        _cleanup();
        __divine_unwind(INT_MIN);
    } else {
        _cleanup();
        __divine_unwind(1, result);
    }
}

int pthread_join( pthread_t _ptid, void **result ) {
    real_pthread_t ptid = _real_pt( _ptid );
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( !ptid.initialized )
        return ESRCH;

    if ( ptid.ltid >= alloc_pslots || ptid.gtid >= thread_counter )
        return EINVAL;

    if ( ptid.gtid != _get_gtid( ptid.ltid ) )
        return ESRCH;

    if ( ptid.gtid == _get_gtid( __divine_get_tid() ) )
        return EDEADLK;

    if ( threads[ ptid.ltid ]->detached )
        return EINVAL;

    // wait for the thread to finnish
    WAIT_OR_CANCEL( threads[ ptid.ltid ]->running );

    if ( ( ptid.gtid != _get_gtid( ptid.ltid ) ) ||
         ( threads[ ptid.ltid ]->detached ) ) {
        // meanwhile detached
        return EINVAL;
    }

    // copy result
    if (result) {
        if ( threads[ ptid.ltid ]->cancelled )
            *result = PTHREAD_CANCELED;
        else
            *result = threads[ ptid.ltid ]->result;
    }

    // let the thread to terminate now
    threads[ ptid.ltid ]->detached = true;

    // force us to synchrozie with the thread on its end, to avoid it from
    // ending nondeterministically in all subsequent states
    __sync_synchronize();
    WAIT( threads[ ptid.ltid ] != NULL );
    return 0;
}

int pthread_detach( pthread_t _ptid ) {
    real_pthread_t ptid = _real_pt( _ptid );
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( !ptid.initialized )
        return ESRCH;

    if ( ptid.ltid >= alloc_pslots || ptid.gtid >= thread_counter )
        return EINVAL;

    if ( ptid.gtid != _get_gtid( ptid.ltid ) )
        return ESRCH;

    if ( threads[ ptid.ltid ]->detached )
        return EINVAL;

    bool ended = !threads[ ptid.ltid ]->running;
    threads[ ptid.ltid ]->detached = true;

    if ( ended ) {
        // force us to synchrozie with the thread on its end, to avoid it from
        // ending nondeterministically in all subsequent states
        __sync_synchronize();
        WAIT( threads[ ptid.ltid ] != NULL );
    }
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
    PTHREAD_FUN_BEGIN();
    return 0;
}

int pthread_attr_init( pthread_attr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    *attr = 0;
    return 0;
}

int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *state) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_FUN_BEGIN();
    unsigned short ltid = __divine_get_tid();
    return ((real_pthread_t){ .gtid = ushort( _get_gtid( ltid ) ), .ltid = ltid, .initialized = 1 }).asint;
}

int pthread_equal( pthread_t t1, pthread_t t2 ) {
    return _real_pt( t1 ).gtid == _real_pt( t2 ).gtid;
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
    int count = mutex->lockCounter;
    count += adj;
    if ( count >= (1 << 12) || count < 0 )
        return EAGAIN;

    mutex->lockCounter = count;
    return 0;
}

inline Thread *_get_thread_by_gtid( int gtid ) {
    for ( int i = 0; i < alloc_pslots; ++i )
        if ( threads[ i ] != NULL && threads[ i ]->gtid == gtid )
            return threads[ i ];
    return NULL;
}

void _check_deadlock( pthread_mutex_t *mutex, int gtid ) {
    int holdertid = 0;
    int lastthread = 0;
    for ( int i = 0; mutex != NULL && i < alloc_pslots /* num of threads */; ++i ) {
        holdertid = mutex->owner - 1;
        if ( holdertid < 0 || holdertid == lastthread )
            return;
        if ( holdertid == gtid ) {
            __divine_problem( Deadlock, "Deadlock: Mutex cycle closed" );
            return;
        }
        Thread *holder = _get_thread_by_gtid( holdertid );
        if ( holder == NULL || !holder->running ) {
            __divine_problem( Deadlock, "Deadlock: Mutex locked by dead thread" );
            return;
        }
        mutex = holder->waiting_mutex;
        lastthread = holdertid;
    }
}

bool _mutex_can_lock( pthread_mutex_t *mutex, int gtid ) {
    return !mutex->owner || ( mutex->owner == ( gtid + 1 ) );
}

int _mutex_lock(pthread_mutex_t *mutex, bool wait) {

    Thread *thr = threads[ __divine_get_tid() ];
    int gtid = thr->gtid;

    if ( mutex == NULL || !mutex->initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->owner == gtid + 1 ) {
        // already locked by this thread
        DBG_ASSERT( mutex->lockCounter ); // count should be > 0
        if ( mutex->type != PTHREAD_MUTEX_RECURSIVE ) {
            if ( mutex->type == PTHREAD_MUTEX_ERRORCHECK )
                return EDEADLK;
            else
                __divine_problem( Deadlock, "Deadlock: Nonrecursive mutex locked again" );
        }
    }

    if ( !_mutex_can_lock( mutex, gtid ) ) {
        DBG_ASSERT( mutex->lockCounter ); // count should be > 0
        if ( !wait )
            return EBUSY;
    }

    // mark waiting now for wait cycle detection
    // note: waiting should not ne set in case of unsuccessfull try-lock
    // (cycle in try-lock is not deadlock, although it might be livelock)
    // so it must be here after return EBUSY
    thr->waiting_mutex = mutex;
    while ( !_mutex_can_lock( mutex, gtid ) ) {
        _check_deadlock( mutex, gtid );
        __divine_interrupt_unmask();
        __divine_interrupt_mask();
    }
    thr->waiting_mutex = NULL;

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if ( err )
        return err;

    // lock the mutex
    mutex->owner = gtid + 1;
    __sync_synchronize();

    return 0;
}

int pthread_mutex_destroy( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( mutex == NULL )
        return EINVAL;

    if ( mutex->owner ) {
        // mutex is locked
        if ( mutex->type == PTHREAD_MUTEX_ERRORCHECK )
             return EBUSY;
        else
             __divine_assert( 0 );
    }
    mutex->initialized = 0;
    return 0;
}

int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( mutex == NULL )
        return EINVAL;

    /* bitfield initializers can be only used as such, that is, initializers,
     * *not* as a right-hand side of an assignment to uninitialised memory */
    memset( mutex, 0, sizeof( pthread_mutex_t ) );
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
    *mutex = init;

    if ( attr )
        mutex->type = attr->type;
    else
        mutex->type = PTHREAD_MUTEX_DEFAULT;
    __divine_interrupt_unmask();
    return 0;
}

int pthread_mutex_lock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _mutex_lock( mutex, 1 );
}

int pthread_mutex_trylock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _mutex_lock( mutex, 0 );
}

int pthread_mutex_unlock( pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    int gtid = _get_gtid( __divine_get_tid() );

    if ( mutex == NULL || !mutex->initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->owner != gtid + 1 ) {
        // mutex is not locked or it is already locked by another thread
        DBG_ASSERT( mutex->lockCounter ); // count should be > 0
        if ( mutex->type == PTHREAD_MUTEX_NORMAL )
             __divine_problem( Other, "Mutex has to be released by the same thread which acquired the mutex." );
        else
             return EPERM; // recursive mutex can also detect
    }

    int r = _mutex_adjust_count( mutex, -1 );
    __divine_assert( r == 0 );
    if ( !mutex->lockCounter ) {
        mutex->owner = 0; // unlock if count == 0
        __sync_synchronize();
    }
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
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL )
        return EINVAL;

    attr->type = 0;
    return 0;
}

int pthread_mutexattr_init( pthread_mutexattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL )
        return EINVAL;

    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *value ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL || value == NULL )
        return EINVAL;

    *value = attr->type & _MUTEX_ATTR_TYPE_MASK;
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
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL || ( value & ~_MUTEX_ATTR_TYPE_MASK ) )
        return EINVAL;

    attr->type = value;
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
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_FUN_BEGIN();

    if (key == NULL)
        return EINVAL;

    int ltid = __divine_get_tid();
    DBG_ASSERT( ltid < alloc_pslots );

    key->data[ltid] = const_cast< void* >( data );
    return 0;
}

void *pthread_getspecific( pthread_key_t key ) {
    PTHREAD_FUN_BEGIN();
    __divine_assert(key != NULL);

    int ltid = __divine_get_tid();
    DBG_ASSERT( ltid < alloc_pslots );

    return key->data[ltid];
}

/* Conditional variables */

/*
  pthread_cond_t representation:

    { .mutex: address of associated mutex
      .counter: number of waiting threads: 16 bits
      .initialized: 1 bit
      ._pad: 15 bits
    }
*/

template< typename CondOrBarrier >
int _cond_adjust_count( CondOrBarrier *cond, int adj ) {
    int count = cond->counter;
    count += adj;
    __divine_assert( count < ( 1 << 16 ) );
    __divine_assert( count >= 0 );

    cond->counter = count;
    return count;
}

template< typename CondOrBarrier >
int _destroy_cond_or_barrier(CondOrBarrier *cond ) {

    if ( cond == NULL || !cond->initialized )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    __divine_assert( cond->counter == 0 );

    cond->counter = 0;
    cond->initialized = 0;
    return 0;
}

int pthread_cond_destroy( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    int r = _destroy_cond_or_barrier( cond );
    if ( r == 0 )
        cond->mutex = NULL;
    return r;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */ ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( cond == NULL )
        return EINVAL;

    if ( cond->initialized )
        return EBUSY; // already initialized

    *cond = (pthread_cond_t){ .mutex = NULL, .initialized = 1, .counter = 0, ._pad = 0 };
    return 0;
}

template< typename > static inline SleepingOn _sleepCond();
template<> inline SleepingOn _sleepCond< pthread_barrier_t >()
{ return Barrier; }
template<> inline SleepingOn _sleepCond< pthread_cond_t >()
{ return Condition; }

static inline bool _eqSleepTrait( Thread *t, pthread_cond_t *cond ) {
    return t->condition == cond;
}

static inline bool _eqSleepTrait( Thread *t, pthread_barrier_t *bar ) {
    return t->barrier == bar;
}

template< bool broadcast, typename CondOrBarrier >
int _cond_signal( CondOrBarrier *cond ) {
    if ( cond == NULL || !cond->initialized )
        return EINVAL;

    int count = cond->counter;
    if ( count )  { // some threads are waiting for condition
        int waiting = 0, wokenup = 0, choice;

        if ( !broadcast ) {
            /* non-determinism */
            choice = __divine_choice( ( 1 << count ) - 1 );
        }

        for ( int i = 0; i < alloc_pslots; ++i ) {
            if ( !threads[i] ) // empty slot
                continue;

            if ( threads[i]->sleeping == _sleepCond< CondOrBarrier >()
                    && _eqSleepTrait( threads[i], cond ) )
            {
                ++waiting;
                if ( broadcast || ( (choice + 1) & ( 1 << (waiting - 1) ) ) ) {
                   // wake up the thread
                   threads[i]->sleeping = NotSleeping;
                   threads[i]->condition = NULL;
                   ++wokenup;
                }
            }
        }

        DBG_ASSERT( count == waiting );

        _cond_adjust_count( cond,-wokenup );
    }
    return 0;
}

int pthread_cond_signal( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    int r = _cond_signal< false >( cond );
    if ( r == 0 && cond->counter == 0 )
        cond->mutex = NULL; // break binding between cond. variable and mutex
    __sync_synchronize();
    return r;
}

int pthread_cond_broadcast( pthread_cond_t *cond ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    int r = _cond_signal< true >( cond );
    if ( r == 0 && cond->counter == 0 )
        cond->mutex = NULL; // break binding between cond. variable and mutex
    __sync_synchronize();
    return r;
}

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    int ltid = __divine_get_tid();
    int gtid = _get_gtid( __divine_get_tid() );

    if ( cond == NULL || !cond->initialized )
        return EINVAL;

    if ( mutex == NULL || !mutex->initialized ) {
        return EINVAL;
    }

    if ( mutex->owner != gtid + 1 ) {
        // mutex is not locked or it is already locked by another thread
        return EPERM;
    }

    if ( __divine_choice( 2 ) )
        return 0; // simulate spurious wakeup

    // It is allowed to have one mutex associated with more than one conditional
    // variable. On the other hand, using more than one mutex for one
    // conditional variable results in undefined behaviour.
    __divine_assert( cond->mutex == NULL || cond->mutex == mutex );
    cond->mutex = mutex;

    // fall asleep
    Thread* thread = threads[ltid];
    thread->setSleeping( cond );

    _cond_adjust_count( cond, 1 ); // one more thread is waiting for this condition
    pthread_mutex_unlock( mutex ); // unlock associated mutex

    // sleeping
    WAIT_OR_CANCEL( thread->sleeping == Condition );

    // try to lock mutex which was associated to the cond. variable by this thread
    // (not the one from current binding, which may have changed)
    pthread_mutex_lock( mutex );
    return 0;
}

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex,
                            const struct timespec * abstime) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
/*
  pthread_once_t representation (extends pthread_mutex_t):
  --------------------------------------------------------------------------------
 | *free* | should be init_routine called?: 1bit | < pthread_mutex_t (27 bits) >  |
  --------------------------------------------------------------------------------
  */

int pthread_once( pthread_once_t *once_control, void (*init_routine)(void) ) {
    if ( once_control->mtx.once == 0 )
        return 0;

    pthread_mutex_lock( &once_control->mtx );

    if ( once_control->mtx.once ) {
        init_routine();
        once_control->mtx.once = 0;
    }

    pthread_mutex_unlock( &once_control->mtx );
    return 0;
}

/* Thread cancellation */
int pthread_setcancelstate( int state, int *oldstate ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if (state & ~0x1)
        return EINVAL;

    int ltid = __divine_get_tid();
    *oldstate = threads[ltid]->cancel_state;
    threads[ltid]->cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if (type & ~0x1)
        return EINVAL;

    int ltid = __divine_get_tid();
    *oldtype = threads[ltid]->cancel_type;
    threads[ltid]->cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t _ptid ) {
    real_pthread_t ptid = _real_pt( _ptid );
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( !ptid.initialized
            || ptid.ltid >= alloc_pslots
            || ptid.gtid >= thread_counter )
        return ESRCH;

    if ( ptid.gtid != _get_gtid( ptid.ltid ) )
        return ESRCH;

    if ( threads[ ptid.ltid ]->cancel_state == PTHREAD_CANCEL_ENABLE )
        threads[ ptid.ltid ]->cancelled = true;

    return 0;
}

void pthread_testcancel( void ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if (_canceled())
        _cancel();
}

void pthread_cleanup_push( void (*routine)(void *), void *arg ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    __divine_assert( routine != NULL );

    int ltid = __divine_get_tid();
    CleanupHandler* handler = reinterpret_cast< CleanupHandler* >( __divine_malloc( sizeof(CleanupHandler) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = threads[ltid]->cleanup_handlers;
    threads[ltid]->cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    WAIT ( !_rwlock_can_lock( rwlock, writer ) );

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

    __sync_synchronize();
    return 0;
}

int pthread_rwlock_destroy( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _rwlock_lock( rwlock, true, false );
}

int pthread_rwlock_wrlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _rwlock_lock( rwlock, true, true );
}

int pthread_rwlock_tryrdlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _rwlock_lock( rwlock, false, false );
}

int pthread_rwlock_trywrlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN();
    return _rwlock_lock( rwlock, false, true );
}

int pthread_rwlock_unlock( pthread_rwlock_t * rwlock ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
    __sync_synchronize();
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
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL )
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL || pshared == NULL )
        return EINVAL;

    *pshared = *attr & _RWLOCK_ATTR_SHARING_MASK;
    return 0;
}

int pthread_rwlockattr_init( pthread_rwlockattr_t *attr ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

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
     initialized: 1 bit
     .nthreads: the number of threads to synchronize: 15 bits
   }
*/

int pthread_barrier_destroy( pthread_barrier_t *barrier ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( barrier == NULL )
        return EINVAL;

    int r = _destroy_cond_or_barrier( barrier );
    if ( r == 0 )
        barrier->nthreads = 0;
    return r;
}

int pthread_barrier_init( pthread_barrier_t *barrier, const pthread_barrierattr_t * attr /* TODO: barrier attributes */,
                          unsigned count ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( count == 0 || barrier == NULL )
        return EINVAL;

    // make sure that no thread is blocked on the barrier
    // (probably better alternative when compared to: return EBUSY)
    __divine_assert( barrier->counter == 0 );

    // Set the number of threads that must call pthread_barrier_wait() before
    // any of them successfully return from the call.
    *barrier = (pthread_barrier_t){ .nthreads = ushort( count ), .initialized = 1, .counter = 0 };
    return 0;
}

int pthread_barrier_wait( pthread_barrier_t *barrier ) {
    PTHREAD_VISIBLE_FUN_BEGIN();

    if ( barrier == NULL || !barrier->initialized )
        return EINVAL;

    int ltid = __divine_get_tid();
    int ret = 0;
    int counter = barrier->counter;
    int release_count = barrier->nthreads;

    if ( (counter + 1) == release_count ) {
        _cond_signal< true >( barrier );
        ret = PTHREAD_BARRIER_SERIAL_THREAD;
    } else {
        // WARNING: this is not immune from spurious wakeup.
        // As of this writing, spurious wakeup is not implemented (and probably never will).

        // fall asleep
        Thread* thread = threads[ltid];
        thread->sleeping = Barrier;
        thread->setSleeping( barrier );

        _cond_adjust_count( barrier, 1 ); // one more thread is blocked on this barrier

        // sleeping
        WAIT_OR_CANCEL( thread->sleeping == Barrier );
    }

    __sync_synchronize();
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
    __divine_interrupt_unmask();
    __divine_interrupt();
    return 0;
}

/* signals */

namespace _sig {

void sig_ign( int ) { }

#define __sig_terminate( SIGNAME ) []( int ) { __divine_problem( Other, "Uncaught signal: " #SIGNAME ); }

// this is based on x86 signal numbers
static const sighandler_t defact[] = {
    __sig_terminate( SIGHUP ),  //  1
    __sig_terminate( SIGINT ),  //  2
    __sig_terminate( SIGQUIT ), //  3
    __sig_terminate( SIGILL ),  //  4
    __sig_terminate( SIGTRAP ), /// 5
    __sig_terminate( SIGABRT ), //  6
    __sig_terminate( SIGBUS ), // 7
    __sig_terminate( SIGFPE ),  //  8
    __sig_terminate( SIGKILL ), //  9
    __sig_terminate( SIGUSR1 ), // 10
    __sig_terminate( SIGSEGV ), // 11
    __sig_terminate( SIGUSR2 ), // 12
    __sig_terminate( SIGPIPE ), // 13
    __sig_terminate( SIGALRM ), // 14
    __sig_terminate( SIGTERM ), // 15
    __sig_terminate( SIGSTKFLT ), // 16
    sig_ign,          // SIGCHLD = 17
    sig_ign,          // SIGCONT = 18 ?? this should be OK since it should
    sig_ign,          // SIGSTOP = 19 ?? stop/resume whole process, we can
    sig_ign,          // SIGTSTP = 20 ?? simulate it as doing nothing
    sig_ign,          // SIGTTIN = 21 ?? at least untill we will have processes
    sig_ign,          // SIGTTOU = 22 ?? and process-aware kill
    sig_ign,          // SIGURG  = 23
    __sig_terminate( SIGXCPU ), // 24
    __sig_terminate( SIGXFSZ ), // 25
    __sig_terminate( SIGVTALRM ), // 26
    __sig_terminate( SIGPROF ), // 27
    sig_ign,         // SIGWINCH = 28
    __sig_terminate( SIGIO ),   // 29
    __sig_terminate( SIGPWR ),  // 30
    __sig_terminate( SIGUNUSED ), // 31

};

sighandler_t &get( Thread *thr, int sig ) { return thr->sighandlers[ sig - 1 ]; }
sighandler_t def( int sig ) { return defact[ sig - 1 ]; }

#undef __SIG_TERMINATE
}

int raise( int sig ) {
    __divine_interrupt_mask();
    assert( sig < 32 );

    if ( threads == nullptr ) // initialization not done yet
        (*_sig::def( sig ))( sig );

    Thread *thread = threads[ __divine_get_tid() ];
    if ( sig > thread->sigmaxused || _sig::get( thread, sig ) == SIG_DFL )
        (*_sig::def( sig ))( sig );
    else {
        sighandler_t h = _sig::get( thread, sig );
        if ( h != SIG_IGN )
            (*h)( sig );
    }
    return 0;
}

sighandler_t signal( int sig, sighandler_t handler ) {
    PTHREAD_FUN_BEGIN(); // init thread structures and mask

    Thread *thread = threads[ __divine_get_tid() ];
    if ( sig > thread->sigmaxused ) {
        int old = thread->sigmaxused;
        sighandler_t *oldptr = thread->sighandlers;
        thread->sigmaxused = sig;
        thread->sighandlers = reinterpret_cast< sighandler_t * >(__divine_malloc( sizeof( sighandler_t ) * sig ) );
        if ( oldptr ) {
            std::copy( oldptr, oldptr + old, thread->sighandlers );
            __divine_free( oldptr );
        }
        for ( int i = old + 1; i <= sig; ++i )
            _sig::get( thread, i ) = SIG_DFL;
    }
    sighandler_t old = _sig::get( thread, sig );
    _sig::get( thread, sig ) = handler;
    return old;
}
