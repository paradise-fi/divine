// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2016 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <pthread.h>
#include <divine.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <dios.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgcc-compat"

// thresholds
#define MILLIARD 1000000000

// bit masks
#define _THREAD_ATTR_DETACH_MASK 0x1

#define _MUTEX_ATTR_TYPE_MASK 0x3

#define _RWLOCK_ATTR_SHARING_MASK 0x1

/* Internal data types */
struct Entry {
    void *( *entry )( void * );
    void *arg;
    bool initialized;
};

struct CleanupHandler {
    void ( *routine )( void * );
    void *arg;
    CleanupHandler *next;
};

enum SleepingOn { NotSleeping = 0, Condition, Barrier };

typedef void ( *sighandler_t )( int );
typedef unsigned short ushort;

struct Thread { // (user-space) information maintained for every (running)
                // thread
    Thread() {
        std::memset( this, 0, sizeof( Thread ) );
    }

    void *result;
    pthread_mutex_t *waiting_mutex;
    CleanupHandler *cleanup_handlers;
    union {
        pthread_cond_t *condition;
        pthread_barrier_t *barrier;
    };
    sighandler_t *sighandlers;

    bool running : 1;
    bool detached : 1;
    SleepingOn sleeping : 2;
    bool cancelled : 1;

    bool cancel_state : 1;
    bool cancel_type : 1;
    bool is_main : 1;
    bool deadlocked : 1;
    unsigned sigmaxused : 5; // at most 32 signals

    void setSleeping( pthread_cond_t *cond ) {
        sleeping = Condition;
        condition = cond;
    }

    void setSleeping( pthread_barrier_t *bar ) {
        sleeping = Barrier;
        barrier = bar;
    }
};

using Destructor = void (*)( void * );

struct _PthreadTLSDestructors {

    int count() {
        return __vm_obj_size( _dtors ) / sizeof( Destructor );
    }

    int getFirstAvailable() {
        int k = -1;
        int c = count();
        for ( int i = 0; i < c; ++i )
            if ( _dtors[ i ] == nullptr ) {
                k = i;
                break;
            }

        if ( k < 0 ) {
            k = c;
            __vm_obj_resize( _dtors, sizeof( Destructor ) * (k + 1) );
        }
        return k;
    }

    void shrink() {
        int toDrop = 0;
        int c = count();
        for ( int i = c - 1; i >= 0; --i ) {
            if ( _dtors[ i ] == nullptr )
                ++toDrop;
            else
                break;
        }
        if ( toDrop )
            __vm_obj_resize( _dtors, std::max( size_t( 1 ), (c - toDrop) * sizeof( Destructor ) ) );
    }

    void init() {
        assert( !_dtors );
        _dtors = static_cast< Destructor * >( __vm_obj_make( 1 ) ); // placeholder so that resize works
    }

    Destructor &operator[]( size_t x ) { return _dtors[ x ]; }

    Destructor *_dtors;
};

static _PthreadTLSDestructors tlsDestructors;


struct _PthreadTLS {
    Thread *thread;
    void *keys[0];

    size_t keyCount() {
        return (__vm_obj_size( static_cast< void * >( this ) ) - sizeof( Thread * )) / sizeof( void * );
    }
    void makeFit( int count ) {
        int now = keyCount();
        if ( count > now ) {
            __vm_obj_resize( static_cast< void * >( this ), sizeof( Thread * ) + count * sizeof( void * ) );
            for ( int i = now; i < count; ++i )
                keys[ i ] = nullptr;
        }
    }
    void shrink() {
        int count = keyCount();
        int toDrop = 0;
        for ( int i = count - 1; i >= 0; --i )
            if ( keys[ i ] == nullptr )
                ++toDrop;
            else
                break;
        if ( toDrop )
            __vm_obj_resize( static_cast< void * >( this ), sizeof( Thread * ) + (count - toDrop) * sizeof( void * ) );
    }
    void *getKey( unsigned key ) {
        assert( key < tlsDestructors.count() );
        if ( key >= keyCount() )
            return nullptr;
        return keys[ key ];
    }

    void setKey( unsigned key, const void *value ) {
        assert( key < tlsDestructors.count() );
        const int c = keyCount();
        if ( value == nullptr && key >= c )
            return;
        if ( key >= c )
            makeFit( key + 1 );
        keys[ key ] = const_cast< void * >( value );
        if ( value == nullptr )
            shrink();
    }

    struct TLSInfo {
        TLSInfo( unsigned index, _PthreadTLS *tls ) : index( index ), tls( tls ) { }

        void *getData() { return tls->getKey( index ); }
        void setData( const void *v ) { return tls->setKey( index, v ); }
        Destructor destructor() { return tlsDestructors[ index ]; }

        unsigned index;
        _PthreadTLS *tls;
    };

    struct TLSIter {
        TLSIter( unsigned index, _PthreadTLS *tls ) : index( index ), tls( tls ) { }

        TLSInfo operator*() {
            return TLSInfo( index, tls );
        }

        TLSIter &operator++() {
            ++index;
            return *this;
        }

        bool operator!=( const TLSIter &o ) const {
            return o.index != index || o.tls != tls;
        }

        unsigned index;
        _PthreadTLS *tls;
    };

    TLSIter begin() { return TLSIter( 0, this ); }
    TLSIter end() { return TLSIter( keyCount(), this ); }
};

static inline _PthreadTLS &tls( _DiOS_ThreadId tid ) {
    return *reinterpret_cast< _PthreadTLS * >( tid );
}

static inline Thread &getThread( pthread_t tid ) {
    return *tls( tid ).thread;
}

static inline Thread &getThread() {
    return getThread( __dios_get_thread_id() );
}

template< typename Yield >
static void iterateThreads( Yield yield ) {
    auto *threads = __dios_get_process_threads();
    int cnt = __vm_obj_size( threads ) / sizeof( _DiOS_ThreadId );
    for ( int i = 0; i < cnt; ++i )
        yield( threads[ i ] );
}

/* Process */
int pthread_atfork( void ( * )( void ), void ( * )( void ), void ( * )( void ) ) {
    /* TODO */
    return 0;
}

static void __init_thread( const _DiOS_ThreadId gtid, const pthread_attr_t attr ) {
    __dios_assert( gtid );

    if ( __vm_obj_size( gtid ) < sizeof( Thread * ) )
        __vm_obj_resize( gtid, sizeof( Thread * ) );
    auto *thread = static_cast< Thread * >( __vm_obj_make( sizeof( Thread ) ) );
    new ( thread ) Thread();
    tls( gtid ).thread = thread;

    // initialize thread metadata
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->condition = nullptr;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
    thread->sighandlers = nullptr;
    thread->sigmaxused = 0;
}

void __pthread_initialize( void ) {
    // initialize implicitly created main thread
    tlsDestructors.init();
    __init_thread( __dios_get_thread_id(), PTHREAD_CREATE_DETACHED );
    getThread().running = true;
    getThread().is_main = true;
}

// this is not run when thread's main returns!
void _run_cleanup_handlers() {
    Thread &thread = getThread();

    CleanupHandler *handler = thread.cleanup_handlers;
    thread.cleanup_handlers = NULL;
    CleanupHandler *next;

    while ( handler ) {
        next = handler->next;
        handler->routine( handler->arg );
        __vm_obj_free( handler );
        handler = next;
    }
}

static void _clean_and_become_zombie( __dios::FencedInterruptMask &mask, _DiOS_ThreadId tid );

void _cancel( __dios::FencedInterruptMask &mask ) {
    _DiOS_ThreadId tid = __dios_get_thread_id();
    Thread &thread = getThread( tid );
    thread.sleeping = NotSleeping;

    // call all cleanup handlers
    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, tid );
}

bool _canceled() {
    return getThread().cancelled;
}

template < bool cancelPoint, typename Cond >
static void _wait( __dios::FencedInterruptMask &mask, Cond &&cond )
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    while ( cond() && ( !cancelPoint || !_canceled() ) )
        mask.without( [] { }, true ); // break mask to allow control flow interrupt
    if ( cancelPoint && _canceled() )
        _cancel( mask );
}

template < typename Cond >
static void waitOrCancel( __dios::FencedInterruptMask &mask, Cond &&cond )
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    return _wait< true >( mask, std::forward< Cond >( cond ) );
}

template < typename Cond >
static void wait( __dios::FencedInterruptMask &mask, Cond &&cond )
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    return _wait< false >( mask, std::forward< Cond >( cond ) );
}

static void _clean_and_become_zombie( __dios::FencedInterruptMask &mask, _DiOS_ThreadId tid ) {
    Thread &thread = getThread( tid );
    // An  optional  destructor  function may be associated with each key
    // value.  At thread exit, if a key value has a non-NULL destructor
    // pointer, and the thread has a non-NULL value associated with that key,
    // the value of the key is set to NULL, and then the function pointed to is
    // called with the previously associated value as its sole argument. The
    // order of destructor calls is unspecified if more than one destructor
    // exists for a thread when it exits.
    //
    // If, after all the destructors have been called for all non-NULL values
    // with associated destructors, there are still some non-NULL values  with
    // associated  destructors, then  the  process  is  repeated.  If,  after
    // at  least {PTHREAD_DESTRUCTOR_ITERATIONS}  iterations  of destructor
    // calls for outstanding non-NULL values, there are still some non-NULL
    // values with associated destructors, implementations may stop calling
    // destructors, or they may continue calling destructors until no
    // non-NULL values with associated destructors exist, even though this
    // might result in an infinite loop.
    int iter = 0;
    auto tls = ::tls( tid );
    do {
        for ( auto tld : tls ) {
            if ( tld.getData() ) {
                auto *data = tld.getData();
                tld.setData( nullptr );
                tld.destructor()( data );
            }
        }
        ++iter;
    } while ( iter <= PTHREAD_DESTRUCTOR_ITERATIONS
              && std::find_if( tls.begin(), tls.end(), []( auto tls ) { return !tls.getData(); } ) != tls.end() );

    thread.running = false;

    if ( thread.detached ) {
        __vm_obj_free( &thread );
        __dios_kill_thread( tid );
    } else // wait until detach / join kills us
        wait( mask, [&] { return true; } );
}

extern "C" void __pthread_entry( void *_args ) {
    __dios::FencedInterruptMask mask;

    Entry *args = static_cast< Entry * >( _args );
    auto tid = __dios_get_thread_id();
    Thread &thread = getThread( tid );

    // copy arguments
    void *arg = args->arg;
    void *( *entry )( void * ) = args->entry;
    // __vm_free_object( _args );

    // parent may delete args and leave pthread_create now
    args->initialized = true;

    // call entry function
    thread.running = true;
    mask.without( [&] {
        // from now on, args and _args should not be accessed
        thread.result = entry( arg );
    } );

    __dios_assert_v( thread.sleeping == false, "thread->sleeping == false" );

    _clean_and_become_zombie( mask, tid );
}

int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *( *entry )( void * ), void *arg ) {
    __dios::FencedInterruptMask mask;

    // test input arguments
    __dios_assert( entry );
    if ( ptid == NULL || entry == NULL )
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry *args = static_cast< Entry * >( __vm_obj_make( sizeof( Entry ) ) );
    args->entry = entry;
    args->arg = arg;
    args->initialized = false;
    auto gtid = __dios_start_thread( __pthread_entry, static_cast< void * >( args ), sizeof( Thread * ) );

    *ptid = gtid;

    // thread initialization
    __init_thread( gtid, ( attr == NULL ? PTHREAD_CREATE_JOINABLE : *attr ) );

    wait( mask, [&] { return !args->initialized; } );
    __vm_obj_free( args );

    return 0;
}

int _pthread_join( __dios::FencedInterruptMask &mask, pthread_t gtid, void **result ) {
    Thread &thread = getThread( gtid );

    if ( gtid == __dios_get_thread_id() )
        return EDEADLK;

    if ( thread.detached )
        return EINVAL;

    // wait for the thread to finnish
    waitOrCancel( mask, [&] { return thread.running; } );

    if ( thread.detached )
        // meanwhile detached
        return EINVAL;

    // copy result
    if ( result ) {
        if ( thread.cancelled )
            *result = PTHREAD_CANCELED;
        else
            *result = thread.result;
    }

    // kill the thread so that it does not pollute state space by ending
    // nondeterministically
    __vm_obj_free( &thread );
    __dios_kill_thread( gtid );
    return 0;
}

void pthread_exit( void *result ) {
    __dios::FencedInterruptMask mask;

    auto gtid = __dios_get_thread_id();
    Thread &thread = getThread( gtid );
    thread.result = result;

    if ( thread.is_main )
        iterateThreads( [&]( _DiOS_ThreadId tid ) {
                _pthread_join( mask, tid, nullptr ); // joining self is detected and ignored
            } );

    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, gtid );
}

int pthread_join( pthread_t gtid, void **result ) {
    __dios::FencedInterruptMask mask;
    return _pthread_join( mask, gtid, result );
}

int pthread_detach( pthread_t gtid ) {
    __dios::FencedInterruptMask mask;
    Thread &thread = getThread( gtid );

    if ( thread.detached )
        return EINVAL;

    bool ended = !thread.running;
    thread.detached = true;

    if ( ended ) {
        // kill the thread so that it does not pollute state space by ending
        // nondeterministically
        __vm_obj_free( &thread );
        __dios_kill_thread( gtid );
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
    __dios::FencedInterruptMask mask;
    return 0;
}

int pthread_attr_init( pthread_attr_t *attr ) {
    __dios::FencedInterruptMask mask;
    *attr = 0;
    return 0;
}

int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *state ) {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || state == NULL )
        return EINVAL;

    *state = *attr & _THREAD_ATTR_DETACH_MASK;
    return 0;
}

int pthread_attr_getguardsize( const pthread_attr_t *, size_t * ) {
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

int pthread_attr_setdetachstate( pthread_attr_t *attr, int state ) {
    __dios::FencedInterruptMask mask;

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
    __dios::FencedInterruptMask mask;
    return __dios_get_thread_id();
}

int pthread_equal( pthread_t t1, pthread_t t2 ) {
    return t1 == t2;
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
 | *free* | initialized?: 1 bit | type: 2 bits | lock counter: 8 bits |
 (thread_id + 1): 16 bits |
  -----------------------------------------------------------------------------------------------
  */

int _mutex_adjust_count( pthread_mutex_t *mutex, int adj ) {
    int count = mutex->__lockCounter;
    count += adj;
    if ( count >= ( 1 << 28 ) || count < 0 )
        return EAGAIN;

    mutex->__lockCounter = count;
    return 0;
}

void _check_deadlock( pthread_mutex_t *mutex, _DiOS_ThreadId gtid ) {
    // note: the cycle is detected first time it occurs, therefore it must go
    // through this mutex, for this reason, we don't need to keep closed set of
    // visited threads and mutexes.
    // Furthermore, if the cycle is re-detected it is already marked in
    // 'deadlocked' attribute of Thread struct.

    while ( mutex && mutex->__owner ) {
        _DiOS_ThreadId ownertid = mutex->__owner;
        Thread &owner = getThread( ownertid );
        if ( ownertid == gtid ) {
            getThread( gtid ).deadlocked = owner.deadlocked = true;
            return __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: Mutex cycle closed" );
        }
        if ( owner.deadlocked ) {
            getThread( gtid ).deadlocked = true;
            return __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: waiting for deadlocked thread" );
        }
        if ( !owner.running ) {
            getThread( gtid ).deadlocked = true;
            return __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: Mutex locked by dead thread" );
        }
        mutex = owner.waiting_mutex;
    }
}

bool _mutex_can_lock( pthread_mutex_t *mutex, _DiOS_ThreadId gtid ) {
    return !mutex->__owner || ( mutex->__owner == gtid );
}

int _mutex_lock( __dios::FencedInterruptMask &mask, pthread_mutex_t *mutex, bool wait ) {

    _DiOS_ThreadId gtid = __dios_get_thread_id();
    Thread &thr = getThread( gtid );

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner == gtid ) {
        // already locked by this thread
        assert( mutex->__lockCounter ); // count should be > 0
        if ( mutex->__type != PTHREAD_MUTEX_RECURSIVE ) {
            if ( mutex->__type == PTHREAD_MUTEX_ERRORCHECK )
                return EDEADLK;
            else
                __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: Nonrecursive mutex locked again" );
        }
    }

    if ( !_mutex_can_lock( mutex, gtid ) ) {
        assert( mutex->__lockCounter ); // count should be > 0
        if ( !wait )
            return EBUSY;
    }

    // mark waiting now for wait cycle detection
    // note: waiting should not ne set in case of unsuccessfull try-lock
    // (cycle in try-lock is not deadlock, although it might be livelock)
    // so it must be here after return EBUSY
    thr.waiting_mutex = mutex;
    while ( !_mutex_can_lock( mutex, gtid ) ) {
        _check_deadlock( mutex, gtid );
        mask.without( [] { }, true ); // break mask to allow control flow interrupt
    }
    thr.waiting_mutex = NULL;

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if ( err )
        return err;

    // lock the mutex
    mutex->__owner = gtid;

    return 0;
}

int pthread_mutex_destroy( pthread_mutex_t *mutex ) {
    __dios::FencedInterruptMask mask;

    if ( mutex == NULL )
        return EINVAL;

    if ( mutex->__owner ) {
        // mutex is locked
        if ( mutex->__type == PTHREAD_MUTEX_ERRORCHECK )
            return EBUSY;
        else
            __dios_fault( _VM_Fault::_VM_F_Locking, "Locked mutex destroyed" );
    }
    mutex->__initialized = 0;
    return 0;
}

int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr ) {
    __dios::FencedInterruptMask mask;

    if ( mutex == NULL )
        return EINVAL;

    /* bitfield initializers can be only used as such, that is, initializers,
     * *not* as a right-hand side of an assignment to uninitialised memory */
    memset( mutex, 0, sizeof( pthread_mutex_t ) );
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
    *mutex = init;

    if ( attr )
        mutex->__type = attr->__type;
    else
        mutex->__type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutex_lock( pthread_mutex_t *mutex ) {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 1 );
}

int pthread_mutex_trylock( pthread_mutex_t *mutex ) {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 0 );
}

int pthread_mutex_unlock( pthread_mutex_t *mutex ) {
    __dios::FencedInterruptMask mask;
    _DiOS_ThreadId gtid = __dios_get_thread_id();

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner != gtid ) {
        // mutex is not locked or it is already locked by another thread
        assert( mutex->__lockCounter ); // count should be > 0
        if ( mutex->__type == PTHREAD_MUTEX_NORMAL )
            __dios_fault( _VM_Fault::_VM_F_Locking, "Mutex has to be released by the "
                                            "same thread which acquired the "
                                            "mutex." );
        else
            return EPERM; // recursive mutex can also detect
    }

    int r = _mutex_adjust_count( mutex, -1 );
    assert( r == 0 );
    if ( !mutex->__lockCounter ) {
        mutex->__owner = nullptr; // unlock if count == 0
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
    __dios::FencedInterruptMask mask;

    if ( abstime == NULL || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD ) {
        return EINVAL;
    }

    // Consider both scenarios, one in which the mutex could not be locked
    // before the specified timeout expired, and the other in which
    // pthread_mutex_timedlock behaves basically the same as pthread_mutex_lock.
    if ( __vm_choose( 2 ) ) {
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
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    attr->__type = 0;
    return 0;
}

int pthread_mutexattr_init( pthread_mutexattr_t *attr ) {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    attr->__type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *value ) {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || value == NULL )
        return EINVAL;

    *value = attr->__type & _MUTEX_ATTR_TYPE_MASK;
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
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( value & ~_MUTEX_ATTR_TYPE_MASK ) )
        return EINVAL;

    attr->__type = value;
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
int pthread_key_create( pthread_key_t *p_key, void ( *destructor )( void * ) ) {
    __dios::FencedInterruptMask mask;

    *p_key = tlsDestructors.getFirstAvailable();;
    tlsDestructors[ *p_key ] = destructor ?: []( void * ) { };

    // TODO: simulate errors?
    return 0;
}

int pthread_key_delete( pthread_key_t key ) {
    __dios::FencedInterruptMask mask;

    if ( key >= tlsDestructors.count() )
        return EINVAL;

    iterateThreads( [&]( _DiOS_ThreadId tid ) {
            tls( tid ).setKey( key, nullptr );
            tls( tid ).shrink();
        } );
    tlsDestructors[ key ] = nullptr;
    tlsDestructors.shrink();
    return 0;
}

int pthread_setspecific( pthread_key_t key, const void *data ) {
    __dios::FencedInterruptMask mask;

    tls( __dios_get_thread_id() ).setKey( key, data );
    return 0;
}

void *pthread_getspecific( pthread_key_t key ) {
    __dios::FencedInterruptMask mask;
    return tls( __dios_get_thread_id() ).getKey( key );
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

template < typename CondOrBarrier > int _cond_adjust_count( CondOrBarrier *cond, int adj ) {
    int count = cond->__counter;
    count += adj;
    assert( count < ( 1 << 16 ) );
    assert( count >= 0 );

    cond->__counter = count;
    return count;
}

template < typename CondOrBarrier > int _destroy_cond_or_barrier( CondOrBarrier *cond ) {

    if ( cond == NULL || !cond->__initialized )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    assert( cond->__counter == 0 );

    cond->__counter = 0;
    cond->__initialized = 0;
    return 0;
}

int pthread_cond_destroy( pthread_cond_t *cond ) {
    __dios::FencedInterruptMask mask;

    int r = _destroy_cond_or_barrier( cond );
    if ( r == 0 )
        cond->__mutex = NULL;
    return r;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */ ) {
    __dios::FencedInterruptMask mask;

    if ( cond == NULL )
        return EINVAL;

    if ( cond->__initialized )
        return EBUSY; // already initialized

    *cond = ( pthread_cond_t ){.__mutex = NULL, .__initialized = 1, .__counter = 0 };
    return 0;
}

template < typename > static inline SleepingOn _sleepCond();
template <> inline SleepingOn _sleepCond< pthread_barrier_t >() {
    return Barrier;
}
template <> inline SleepingOn _sleepCond< pthread_cond_t >() {
    return Condition;
}

static inline bool _eqSleepTrait( Thread &t, pthread_cond_t *cond ) {
    return t.condition == cond;
}

static inline bool _eqSleepTrait( Thread &t, pthread_barrier_t *bar ) {
    return t.barrier == bar;
}

template< bool broadcast, typename CondOrBarrier >
int _cond_signal( CondOrBarrier *cond ) {
    if ( cond == NULL || !cond->__initialized )
        return EINVAL;

    int count = cond->__counter;
    if ( count ) { // some threads are waiting for condition
        int waiting = 0, wokenup = 0, choice;

        if ( !broadcast ) {
            /* non-determinism */
            choice = __vm_choose( ( 1 << count ) - 1 );
        }

        iterateThreads( [&]( pthread_t tid ) {

            Thread &thread = getThread( tid );

            if ( thread.sleeping == _sleepCond< CondOrBarrier >() && _eqSleepTrait( thread, cond ) ) {
                ++waiting;
                if ( broadcast || ( ( choice + 1 ) & ( 1 << ( waiting - 1 ) ) ) ) {
                    // wake up the thread
                    thread.sleeping = NotSleeping;
                    thread.condition = NULL;
                    ++wokenup;
                }
            }
        } );

        assert( count == waiting );

        _cond_adjust_count( cond, -wokenup );
    }
    return 0;
}

int pthread_cond_signal( pthread_cond_t *cond ) {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< false >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_broadcast( pthread_cond_t *cond ) {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< true >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) {
    __dios::FencedInterruptMask mask;

    _DiOS_ThreadId gtid = __dios_get_thread_id();

    if ( cond == NULL || !cond->__initialized )
        return EINVAL;

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL;
    }

    if ( mutex->__owner != gtid ) {
        // mutex is not locked or it is already locked by another thread
        return EPERM;
    }

    if ( __vm_choose( 2 ) )
        return 0; // simulate spurious wakeup

    // It is allowed to have one mutex associated with more than one conditional
    // variable. On the other hand, using more than one mutex for one
    // conditional variable results in undefined behaviour.
    if ( cond->__mutex != NULL && cond->__mutex != mutex )
        __dios_fault( _VM_Fault::_VM_F_Locking, "Attempted to wait on condition variable using other mutex "
                                        "that already in use by this condition variable" );
    cond->__mutex = mutex;

    // fall asleep
    Thread &thread = getThread( gtid );
    thread.setSleeping( cond );

    _cond_adjust_count( cond, 1 ); // one more thread is waiting for this condition
    pthread_mutex_unlock( mutex ); // unlock associated mutex

    // sleeping
    waitOrCancel( mask, [&] { return thread.sleeping == Condition; } );

    // try to lock mutex which was associated to the cond. variable by this
    // thread
    // (not the one from current binding, which may have changed)
    _mutex_lock( mask, mutex, 1 );
    return 0;
}

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime ) {
    __dios::FencedInterruptMask mask;

    if ( abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD )
        return EINVAL;

    // Consider both scenarios, one in which the time specified by abstime has
    // passed
    // before the function could finish, and the other in which
    // pthread_cond_timedwait behaves basically the same as pthread_cond_wait.
    if ( __vm_choose( 2 ) ) {
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
 | *free* | should be init_routine called?: 1bit | < pthread_mutex_t (27 bits) > |
  --------------------------------------------------------------------------------
  */

int pthread_once( pthread_once_t *once_control, void ( *init_routine )( void ) ) {
    if ( once_control->__mtx.__once == 0 )
        return 0;

    pthread_mutex_lock( &once_control->__mtx );

    if ( once_control->__mtx.__once ) {
        init_routine();
        once_control->__mtx.__once = 0;
    }

    pthread_mutex_unlock( &once_control->__mtx );
    return 0;
}

/* Thread cancellation */
int pthread_setcancelstate( int state, int *oldstate ) {
    __dios::FencedInterruptMask mask;

    if ( state & ~0x1 )
        return EINVAL;

    Thread &thread = getThread();
    *oldstate = thread.cancel_state;
    thread.cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) {
    __dios::FencedInterruptMask mask;

    if ( type & ~0x1 )
        return EINVAL;

    Thread &thread = getThread();
    *oldtype = thread.cancel_type;
    thread.cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t gtid ) {
    __dios::FencedInterruptMask mask;

    Thread &thread = getThread();

    if ( thread.cancel_state == PTHREAD_CANCEL_ENABLE )
        thread.cancelled = true;

    return 0;
}

void pthread_testcancel( void ) {
    __dios::FencedInterruptMask mask;

    if ( _canceled() )
        _cancel( mask );
}

void pthread_cleanup_push( void ( *routine )( void * ), void *arg ) {
    __dios::FencedInterruptMask mask;

    assert( routine != NULL );

    Thread &thread = getThread();
    CleanupHandler *handler = reinterpret_cast< CleanupHandler * >(
            __vm_obj_make( sizeof( CleanupHandler ) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = thread.cleanup_handlers;
    thread.cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) {
    __dios::FencedInterruptMask mask;

    Thread &thread = getThread();
    CleanupHandler *handler = thread.cleanup_handlers;
    if ( handler != NULL ) {
        thread.cleanup_handlers = handler->next;
        if ( execute ) {
            handler->routine( handler->arg );
        }
        __vm_obj_free( handler );
    }
}

/* Readers-Writer lock */

int _rlock_adjust_count( _ReadLock *rlock, int adj ) {
    int count = rlock->__count;
    count += adj;
    if ( count < 0 )
        return EAGAIN;
    rlock->__count = count;
    return 0;
}

_ReadLock *_get_rlock( pthread_rwlock_t *rwlock, _DiOS_ThreadId gtid, _ReadLock **prev = nullptr ) {
    _ReadLock *rlock = rwlock->__rlocks;
    _ReadLock *_prev = nullptr;

    while ( rlock && ( rlock->__owner != gtid ) ) {
        _prev = rlock;
        rlock = rlock->__next;
    }

    if ( rlock && prev )
        *prev = _prev;

    return rlock;
}

_ReadLock *_create_rlock( pthread_rwlock_t *rwlock, _DiOS_ThreadId gtid ) {
    _ReadLock *rlock = reinterpret_cast< _ReadLock * >( __vm_obj_make( sizeof( _ReadLock ) ) );
    rlock->__owner = gtid;
    rlock->__count = 1;
    rlock->__next = rwlock->__rlocks;
    rwlock->__rlocks = rlock;
    return rlock;
}

bool _rwlock_can_lock( pthread_rwlock_t *rwlock, bool writer ) {
    return !rwlock->__wrowner && !( writer && rwlock->__rlocks );
}

int _rwlock_lock( __dios::FencedInterruptMask &mask, pthread_rwlock_t *rwlock, bool shouldwait, bool writer ) {
    _DiOS_ThreadId gtid = __dios_get_thread_id();

    if ( rwlock == nullptr || !rwlock->__initialized ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    if ( rwlock->__wrowner == gtid )
        // thread already owns write lock
        return EDEADLK;

    _ReadLock *rlock = _get_rlock( rwlock, gtid );
    // existing read lock implies read lock counter having value more than zero
    assert( !rlock || rlock->__count );

    if ( writer && rlock )
        // thread already owns read lock
        return EDEADLK;

    if ( !_rwlock_can_lock( rwlock, writer ) ) {
        if ( !shouldwait )
            return EBUSY;
    }
    wait( mask, [&] { return !_rwlock_can_lock( rwlock, writer ); } );

    if ( writer )
        rwlock->__wrowner = gtid;
    else {
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

int pthread_rwlock_destroy( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;

    if ( rwlock->__wrowner || rwlock->__rlocks )
        // rwlock is locked
        return EBUSY;

    rwlock->__initialized = 0;
    return 0;
}

int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr ) {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;

    if ( rwlock->__initialized ) {
        // already initialized
        return EBUSY;
    }

    rwlock->__processShared = attr && bool( *attr & _RWLOCK_ATTR_SHARING_MASK );
    rwlock->__initialized = true;
    rwlock->__wrowner = nullptr;
    rwlock->__rlocks = nullptr;
    return 0;
}

int pthread_rwlock_rdlock( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, false );
}

int pthread_rwlock_wrlock( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, true );
}

int pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, false );
}

int pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, true );
}

int pthread_rwlock_unlock( pthread_rwlock_t *rwlock ) {
    __dios::FencedInterruptMask mask;

    _DiOS_ThreadId gtid = __dios_get_thread_id();

    if ( rwlock == NULL || !rwlock->__initialized ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    _ReadLock *rlock, *prev;
    rlock = _get_rlock( rwlock, gtid, &prev );
    // existing read lock implies read lock counter having value more than zero
    assert( !rlock || rlock->__count );

    if ( rwlock->__wrowner != gtid && !rlock ) {
        // the current thread does not own the read-write lock
        return EPERM;
    }

    if ( rwlock->__wrowner == gtid ) {
        assert( !rlock );
        // release write lock
        rwlock->__wrowner = nullptr;
    } else {
        // release read lock
        _rlock_adjust_count( rlock, -1 );
        if ( rlock->__count == 0 ) {
            // no more read locks are owned by this thread
            if ( prev )
                prev->__next = rlock->__next;
            else
                rwlock->__rlocks = rlock->__next;
            __vm_obj_free( rlock );
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
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared ) {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || pshared == NULL )
        return EINVAL;

    *pshared = *attr & _RWLOCK_ATTR_SHARING_MASK;
    return 0;
}

int pthread_rwlockattr_init( pthread_rwlockattr_t *attr ) {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared ) {
    __dios::FencedInterruptMask mask;

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
    __dios::FencedInterruptMask mask;

    if ( barrier == NULL )
        return EINVAL;

    int r = _destroy_cond_or_barrier( barrier );
    if ( r == 0 )
        barrier->__nthreads = 0;
    return r;
}

int pthread_barrier_init(
        pthread_barrier_t *barrier,
        const pthread_barrierattr_t *attr /* TODO: barrier attributes */,
        unsigned count ) {
    __dios::FencedInterruptMask mask;

    if ( count == 0 || barrier == NULL )
        return EINVAL;

    // make sure that no thread is blocked on the barrier
    // (probably better alternative when compared to: return EBUSY)
    assert( barrier->__counter == 0 );

    // Set the number of threads that must call pthread_barrier_wait() before
    // any of them successfully return from the call.
    *barrier = ( pthread_barrier_t ){ .__nthreads = ushort( count ), .__initialized = 1, .__counter = 0 };
    return 0;
}

int pthread_barrier_wait( pthread_barrier_t *barrier ) {
    __dios::FencedInterruptMask mask;

    if ( barrier == NULL || !barrier->__initialized )
        return EINVAL;

    int ret = 0;
    int counter = barrier->__counter;
    int release_count = barrier->__nthreads;

    if ( ( counter + 1 ) == release_count ) {
        _cond_signal< true >( barrier );
        ret = PTHREAD_BARRIER_SERIAL_THREAD;
    } else {
        // WARNING: this is not immune from spurious wakeup.
        // As of this writing, spurious wakeup is not implemented (and probably
        // never will).

        // fall asleep
        Thread &thread = getThread();
        thread.setSleeping( barrier );

        _cond_adjust_count( barrier, 1 ); // one more thread is blocked on this barrier

        // sleeping
        waitOrCancel( mask, [&] { return thread.sleeping == Barrier; } );
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
int sched_get_priority_max( int ) {
    /* TODO */
    return 0;
}

int sched_get_priority_min( int ) {
    /* TODO */
    return 0;
}

int sched_getparam( pid_t, struct sched_param * ) {
    /* TODO */
    return 0;
}

int sched_getscheduler( pid_t ) {
    /* TODO */
    return 0;
}

int sched_rr_get_interval( pid_t, struct timespec * ) {
    /* TODO */
    return 0;
}

int sched_setparam( pid_t, const struct sched_param * ) {
    /* TODO */
    return 0;
}

int sched_setscheduler( pid_t, int, const struct sched_param * ) {
    /* TODO */
    return 0;
}

int sched_yield( void ) {
    /* TODO */
    return 0;
}

/* signals */

namespace _sig {

void sig_ign( int ) {}

#define __sig_terminate( SIGNAME ) []( int ) { __dios_fault( _VM_Fault::_VM_F_Control, "Uncaught signal: " #SIGNAME ); }

// this is based on x86 signal numbers
static const sighandler_t defact[] = {
        __sig_terminate( SIGHUP ),    //  1
        __sig_terminate( SIGINT ),    //  2
        __sig_terminate( SIGQUIT ),   //  3
        __sig_terminate( SIGILL ),    //  4
        __sig_terminate( SIGTRAP ),   /// 5
        __sig_terminate( SIGABRT ),   //  6
        __sig_terminate( SIGBUS ),    // 7
        __sig_terminate( SIGFPE ),    //  8
        __sig_terminate( SIGKILL ),   //  9
        __sig_terminate( SIGUSR1 ),   // 10
        __sig_terminate( SIGSEGV ),   // 11
        __sig_terminate( SIGUSR2 ),   // 12
        __sig_terminate( SIGPIPE ),   // 13
        __sig_terminate( SIGALRM ),   // 14
        __sig_terminate( SIGTERM ),   // 15
        __sig_terminate( SIGSTKFLT ), // 16
        sig_ign,                      // SIGCHLD = 17
        sig_ign,                      // SIGCONT = 18 ?? this should be OK since it should
        sig_ign,                      // SIGSTOP = 19 ?? stop/resume whole process, we can
        sig_ign,                      // SIGTSTP = 20 ?? simulate it as doing nothing
        sig_ign,                      // SIGTTIN = 21 ?? at least untill we will have processes
        sig_ign,                      // SIGTTOU = 22 ?? and process-aware kill
        sig_ign,                      // SIGURG  = 23
        __sig_terminate( SIGXCPU ),   // 24
        __sig_terminate( SIGXFSZ ),   // 25
        __sig_terminate( SIGVTALRM ), // 26
        __sig_terminate( SIGPROF ),   // 27
        sig_ign,                      // SIGWINCH = 28
        __sig_terminate( SIGIO ),     // 29
        __sig_terminate( SIGPWR ),    // 30
        __sig_terminate( SIGUNUSED ), // 31

};

sighandler_t &get( Thread &thr, int sig ) {
    return thr.sighandlers[sig - 1];
}
sighandler_t def( int sig ) {
    return defact[sig - 1];
}

#undef __sig_terminate
}

int raise( int sig ) {
    __dios::InterruptMask mask;
    assert( sig < 32 );

    Thread &thread = getThread();
    if ( sig > thread.sigmaxused || _sig::get( thread, sig ) == SIG_DFL )
        ( *_sig::def( sig ) )( sig );
    else {
        sighandler_t h = _sig::get( thread, sig );
        if ( h != SIG_IGN )
            ( *h )( sig );
    }
    return 0;
}

sighandler_t signal( int sig, sighandler_t handler ) {
    __dios::FencedInterruptMask mask;

    Thread &thread = getThread();
    if ( sig > thread.sigmaxused ) {
        int old = thread.sigmaxused;
        sighandler_t *oldptr = thread.sighandlers;
        thread.sigmaxused = sig;
        thread.sighandlers = reinterpret_cast< sighandler_t * >(
                __vm_obj_make( sizeof( sighandler_t ) * sig ) );
        if ( oldptr ) {
            std::copy( oldptr, oldptr + old, thread.sighandlers );
            __vm_obj_free( oldptr );
        }
        for ( int i = old + 1; i <= sig; ++i )
            _sig::get( thread, i ) = SIG_DFL;
    }
    sighandler_t old = _sig::get( thread, sig );
    _sig::get( thread, sig ) = handler;
    return old;
}

#pragma GCC diagnostic pop
