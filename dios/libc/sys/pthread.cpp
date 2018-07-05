// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <pthread.h>
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <sys/fault.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <dios.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgcc-compat"

// thresholds
#define MILLIARD 1000000000

// bit masks
#define _THREAD_ATTR_DETACH_MASK 0x1

#define _MUTEX_ATTR_TYPE_MASK 0x3

#define _RWLOCK_ATTR_SHARING_MASK 0x1

struct CleanupHandler {
    void ( *routine )( void * );
    void *arg;
    CleanupHandler *next;
};

enum SleepingOn { NotSleeping = 0, Condition, Barrier };

typedef unsigned short ushort;

struct _PThread { // (user-space) information maintained for every (running)
                // thread
    _PThread() noexcept
    {
        memset( this, 0, sizeof( _PThread ) );
        ++refcnt;
    }

    // avoid accidental copies
    _PThread( const _PThread & ) = delete;
    _PThread( _PThread && ) = delete;

    void *result;
    pthread_mutex_t *waiting_mutex;
    CleanupHandler *cleanup_handlers;
    union {
        pthread_cond_t *condition;
        pthread_barrier_t *barrier;
    };

    bool running : 1;
    bool detached : 1;
    SleepingOn sleeping : 2;
    bool cancelled : 1;

    bool cancel_state : 1;
    bool cancel_type : 1;
    bool is_main : 1;
    bool deadlocked : 1;
    uint32_t refcnt;

    void setSleeping( pthread_cond_t *cond ) noexcept {
        sleeping = Condition;
        condition = cond;
    }

    void setSleeping( pthread_barrier_t *bar ) noexcept {
        sleeping = Barrier;
        barrier = bar;
    }
};

static_assert( sizeof( _PThread ) == 5 * sizeof( void * ) );

template< typename Handler >
struct _PthreadHandlers
{
    _PthreadHandlers() noexcept = default;
    _PthreadHandlers( const _PthreadHandlers & ) noexcept = delete;
    _PthreadHandlers( _PthreadHandlers && ) noexcept = delete;

    int count() noexcept
    {
        return __vm_obj_size( _dtors ) / sizeof( Handler );
    }

    int getFirstAvailable() noexcept {
        int k = -1;
        int c = count();
        for ( int i = 0; i < c; ++i )
            if ( !_dtors[ i ] )
            {
                k = i;
                break;
            }

        if ( k < 0 ) {
            k = c;
            __vm_obj_resize( _dtors, sizeof( Handler ) * (k + 1) );
        }
        return k;
    }

    void shrink() noexcept
    {
        int toDrop = 0;
        int c = count();

        for ( int i = c - 1; i >= 0; --i )
        {
            if ( !_dtors[ i ] )
                ++ toDrop;
            else
                break;
        }

        int newsz = c - toDrop;
        if ( newsz < 1 ) newsz = 1;

        if ( toDrop )
            __vm_obj_resize( _dtors, newsz * sizeof( Handler ) );
    }

    void init() noexcept {
        assert( !_dtors );
        _dtors = static_cast< Handler * >( __vm_obj_make( 1 ) ); // placeholder so that resize works
    }

    Handler &operator[]( size_t x ) noexcept { return _dtors[ x ]; }
    Handler *_dtors;
};

using Destructor = void (*)( void * );

struct ForkHandler
{
    void (*prepare)();
    void (*parent)();
    void (*child)();
    bool operator!() const { return !prepare && !parent && !child; }
};

using _PthreadTLSDestructors = _PthreadHandlers< Destructor >;
using _PthreadAtFork = _PthreadHandlers< ForkHandler >;

static _PthreadTLSDestructors tlsDestructors;
static _PthreadAtFork atForkHandlers;

struct _PthreadTLS {

    _PthreadTLS( const _PthreadTLS & ) noexcept = delete;
    _PthreadTLS( _PthreadTLS && ) noexcept = delete;

    _PThread *thread;
    void *keys[0];

    void *raw() noexcept {
        return reinterpret_cast< char * >( this ) - sizeof( __dios_tls );
    }

    static int raw_size( int cnt ) noexcept {
        return sizeof( __dios_tls ) + sizeof( _PThread * ) + cnt * sizeof( void * );
    }

    int keyCount() noexcept {
        return ( __vm_obj_size( raw() ) - raw_size( 0 ) ) / sizeof( void * );
    }

    void makeFit( int count ) noexcept {
        int now = keyCount();
        if ( count > now )
        {
            __vm_obj_resize( raw(), raw_size( count ) );
            for ( int i = now; i < count; ++i )
                keys[ i ] = nullptr;
        }
    }

    void shrink() noexcept {
        int count = keyCount();
        int toDrop = 0;
        for ( int i = count - 1; i >= 0; --i )
            if ( keys[ i ] == nullptr )
                ++toDrop;
            else
                break;
        if ( toDrop )
            __vm_obj_resize( raw(), raw_size( count - toDrop ) );
    }

    void *getKey( int key ) noexcept {
        assert( key >= 0 && key < tlsDestructors.count() );
        if ( key >= keyCount() )
            return nullptr;
        return keys[ key ];
    }

    void setKey( int key, const void *value ) noexcept {
        assert( key >= 0 && key < tlsDestructors.count() );
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
        TLSInfo( unsigned index, _PthreadTLS *tls ) noexcept : index( index ), tls( tls ) { }

        void *getData() noexcept { return tls->getKey( index ); }
        void setData( const void *v ) noexcept { return tls->setKey( index, v ); }
        Destructor destructor() noexcept { return tlsDestructors[ index ]; }

        unsigned index;
        _PthreadTLS *tls;
    };

    struct TLSIter {
        TLSIter( unsigned index, _PthreadTLS *tls ) noexcept : index( index ), tls( tls ) { }

        TLSInfo operator*() noexcept {
            return TLSInfo( index, tls );
        }

        TLSIter &operator++() noexcept {
            ++index;
            return *this;
        }

        bool operator!=( const TLSIter &o ) const noexcept {
            return o.index != index || o.tls != tls;
        }

        unsigned index;
        _PthreadTLS *tls;
    };

    TLSIter begin() noexcept { return TLSIter( 0, this ); }
    TLSIter end() noexcept { return TLSIter( keyCount(), this ); }
};

static inline _PthreadTLS &tls( __dios_task tid ) noexcept
{
    return *reinterpret_cast< _PthreadTLS * >( &( tid->__data ) );
}

static inline _PThread &getThread( pthread_t tid ) noexcept {
    return *tls( tid ).thread;
}

static inline _PThread &getThread() noexcept {
    return getThread( __dios_this_task() );
}

static inline void acquireThread( _PThread &thread ) {
    ++thread.refcnt;
}

static inline void releaseThread( _PThread &thread ) {
    --thread.refcnt;
    if ( thread.refcnt == 0 )
        __vm_obj_free( &thread );
}

static inline void releaseAndKillThread( __dios_task tid ) {
    releaseThread( getThread( tid ) );
    __dios_kill( tid );
}

template< typename Yield >
static void iterateThreads( Yield yield ) noexcept {
    auto *threads = __dios_this_process_tasks();
    int cnt = __vm_obj_size( threads ) / sizeof( __dios_task );
    for ( int i = 0; i < cnt; ++i )
        yield( threads[ i ] );
    __vm_obj_free( threads );
}

/* Process */
int pthread_atfork( void ( *prepare )( void ), void ( *parent )( void ),
                    void ( *child )( void ) ) noexcept
{
    if ( __vm_choose( 2 ) )
        return ENOMEM;
    int i = atForkHandlers.getFirstAvailable();
    atForkHandlers[i].prepare = prepare;
    atForkHandlers[i].parent = parent;
    atForkHandlers[i].child = child;
    return 0;
}

void __run_atfork_handlers( ushort index ) noexcept {

    auto invoke = []( void (*h)() ){ if ( h ) h(); };
    auto &h = atForkHandlers;

    if ( index == 0 )
        for ( int i = h.count() - 1; i >= 0; --i )
            invoke( h[i].prepare );
    else
        for ( int i = 0; i < h.count(); ++ i )
            if ( index == 1 )
                invoke( h[i].parent );
            else
            {
                getThread().is_main = true;
                invoke( h[i].child );
            }
}

inline void* operator new ( size_t, void* p ) noexcept { return p; }

static void __init_thread( const __dios_task gtid, const pthread_attr_t attr ) noexcept {
    __dios_assert( gtid );

    if ( __vm_obj_size( gtid ) < _PthreadTLS::raw_size( 0 ) )
        __vm_obj_resize( gtid, _PthreadTLS::raw_size( 0 ) );
    auto *thread = static_cast< _PThread * >( __vm_obj_make( sizeof( _PThread ) ) );
    new ( thread ) _PThread();
    tls( gtid ).thread = thread;

    // initialize thread metadata
    thread->running = true;
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->condition = nullptr;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
}

void __pthread_initialize() noexcept
{
    // initialize implicitly created main thread
    tlsDestructors.init();
    atForkHandlers.init();
    __init_thread( __dios_this_task(), PTHREAD_CREATE_DETACHED );
    getThread().is_main = true;
}

// this is not run when thread's main returns!
static void _run_cleanup_handlers() noexcept {
    _PThread &thread = getThread();

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

_Noreturn static void _clean_and_become_zombie( __dios::FencedInterruptMask &mask, __dios_task tid ) noexcept;

static void _cancel( __dios::FencedInterruptMask &mask ) noexcept {
    __dios_task tid = __dios_this_task();
    _PThread &thread = getThread( tid );
    thread.sleeping = NotSleeping;

    // call all cleanup handlers
    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, tid );
}

static bool _canceled() noexcept {
    return getThread().cancelled;
}

template < bool cancelPoint, typename Cond >
static void _wait( __dios::FencedInterruptMask &mask, Cond &&cond ) noexcept
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    if ( cancelPoint && _canceled() )
        _cancel( mask );
    else
    {
        if ( cond() )
            mask.without( []{ __dios_reschedule(); } );
        if ( cond() )
            __vm_cancel();
    }
}

template < typename Cond >
static void waitOrCancel( __dios::FencedInterruptMask &mask, Cond cond ) noexcept
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    return _wait< true >( mask, cond );
}

template < typename Cond >
static void wait( __dios::FencedInterruptMask &mask, Cond cond ) noexcept
        __attribute__( ( __always_inline__, __flatten__ ) )
{
    return _wait< false >( mask, cond );
}

_Noreturn static void _clean_and_become_zombie( __dios::FencedInterruptMask &mask,
                                                __dios_task tid ) noexcept
{
    _PThread &thread = getThread( tid );
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
    bool done;

    auto &tls = ::tls( tid );
    do {
        done = true;
        for ( auto tld : tls ) {
            if ( tld.getData() ) {
                done = false;
                auto *data = tld.getData();
                tld.setData( nullptr );
                tld.destructor()( data );
            }
        }
        ++iter;
    } while ( iter <= PTHREAD_DESTRUCTOR_ITERATIONS && !done );

    thread.running = false;

    if ( thread.is_main )
        exit( 0 );

    if ( thread.detached )
        releaseAndKillThread( tid );
    else // wait until detach / join kills us
        wait( mask, [&] { return true; } );
    __builtin_trap();
}

/* Internal data types */
struct Entry {
    void *( *entry )( void * );
    void *arg;
};

// no nexcept here, avoid adding landingpads and filters
__noinline void __pthread_start( void *_args )
{
    __dios::FencedInterruptMask mask;

    Entry *args = static_cast< Entry * >( _args );
    auto tid = __dios_this_task();
    _PThread &thread = getThread( tid );

    // copy arguments
    void *arg = args->arg;
    auto entry = args->entry;
    __vm_obj_free( _args );

    // call entry function
    mask.without( [&] {
        thread.result = entry( arg );
    } );

    assert( thread.sleeping == false );

    _clean_and_become_zombie( mask, tid );
}

extern "C" void __pthread_entry( void *_args )
{
    __pthread_start( _args );
    __dios_suicide();
}

int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *( *entry )( void * ), void *arg ) noexcept {
    __dios::FencedInterruptMask mask;

    // test input arguments
    __dios_assert( entry );
    if ( ptid == NULL || entry == NULL )
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry *args = static_cast< Entry * >( __vm_obj_make( sizeof( Entry ) ) );
    args->entry = entry;
    args->arg = arg;
    auto tid = __dios_start_task( __pthread_entry, static_cast< void * >( args ), 0 );
    // init thread here, before it has first chance to run
    __init_thread( tid, attr == nullptr ? PTHREAD_CREATE_JOINABLE : *attr );
    *ptid = tid;

    return 0;
}

static int _pthread_join( __dios::FencedInterruptMask &mask, pthread_t gtid, void **result ) noexcept {
    _PThread &thread = getThread( gtid );

    if ( gtid == __dios_this_task() )
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
    releaseAndKillThread( gtid );
    return 0;
}

void pthread_exit( void *result ) noexcept {
    __dios::FencedInterruptMask mask;

    auto gtid = __dios_this_task();
    _PThread &thread = getThread( gtid );
    thread.result = result;

    if ( thread.is_main )
        iterateThreads( [&]( __dios_task tid ) {
                _pthread_join( mask, tid, nullptr ); // joining self is detected and ignored
            } );

    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, gtid );
}

int pthread_join( pthread_t gtid, void **result ) noexcept {
    __dios::FencedInterruptMask mask;
    return _pthread_join( mask, gtid, result );
}

int pthread_detach( pthread_t gtid ) noexcept {
    __dios::FencedInterruptMask mask;
    _PThread &thread = getThread( gtid );

    if ( thread.detached )
        return EINVAL;

    bool ended = !thread.running;
    thread.detached = true;

    if ( ended ) {
        // kill the thread so that it does not pollute state space by ending
        // nondeterministically
        releaseAndKillThread( gtid );
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

int pthread_attr_destroy( pthread_attr_t * ) noexcept {
    __dios::FencedInterruptMask mask;
    return 0;
}

int pthread_attr_init( pthread_attr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;
    *attr = 0;
    return 0;
}

int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *state ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || state == NULL )
        return EINVAL;

    *state = *attr & _THREAD_ATTR_DETACH_MASK;
    return 0;
}

int pthread_attr_getguardsize( const pthread_attr_t *, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getinheritsched( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getschedparam( const pthread_attr_t *, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getschedpolicy( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getscope( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstack( const pthread_attr_t *, void **, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstackaddr( const pthread_attr_t *, void ** ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstacksize( const pthread_attr_t *, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setdetachstate( pthread_attr_t *attr, int state ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( state & ~_THREAD_ATTR_DETACH_MASK ) )
        return EINVAL;

    *attr &= ~_THREAD_ATTR_DETACH_MASK;
    *attr |= state;
    return 0;
}

int pthread_attr_setguardsize( pthread_attr_t *, size_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setinheritsched( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setschedparam( pthread_attr_t *, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setschedpolicy( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setscope( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstack( pthread_attr_t *, void *, size_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstackaddr( pthread_attr_t *, void * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstacksize( pthread_attr_t *, size_t ) noexcept {
    /* TODO */
    return 0;
}

/* Thread ID */
pthread_t pthread_self( void ) noexcept {
    __dios::FencedInterruptMask mask;
    return __dios_this_task();
}

int pthread_equal( pthread_t t1, pthread_t t2 ) noexcept {
    return t1 == t2;
}

/* Scheduler */
int pthread_getconcurrency( void ) noexcept {
    /* TODO */
    return 0;
}

int pthread_getcpuclockid( pthread_t, clockid_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_getschedparam( pthread_t, int *, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setconcurrency( int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setschedparam( pthread_t, int, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setschedprio( pthread_t, int ) noexcept {
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

static int _mutex_adjust_count( pthread_mutex_t *mutex, int adj ) noexcept {
    int count = mutex->__lockCounter;
    count += adj;
    if ( count >= ( 1 << 28 ) || count < 0 )
        return EAGAIN;

    mutex->__lockCounter = count;
    return 0;
}

static bool _check_deadlock( pthread_mutex_t *mutex, _PThread &tid ) noexcept
{
    // note: the cycle is detected first time it occurs, therefore it must go
    // through this mutex, for this reason, we don't need to keep closed set of
    // visited threads and mutexes.
    // Furthermore, if the cycle is re-detected it is already marked in
    // 'deadlocked' attribute of Thread struct.

    while ( mutex && mutex->__owner )
    {
        _PThread *owner = mutex->__owner;
        if ( owner == &tid )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: mutex cycle closed, circular waiting" );
            return tid.deadlocked = owner->deadlocked = true;
        }
        if ( owner->deadlocked )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: waiting for a deadlocked thread" );
            return tid.deadlocked = true;
        }
        if ( !owner->running )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: mutex locked by a dead thread" );
            return tid.deadlocked = true;
        }
        mutex = owner->waiting_mutex;
    }
    return false;
}

static bool _mutex_can_lock( pthread_mutex_t *mutex, _PThread &thr ) noexcept {
    return !mutex->__owner || ( mutex->__owner == &thr );
}

static int _mutex_lock( __dios::FencedInterruptMask &mask, pthread_mutex_t *mutex, bool wait ) noexcept {

    __dios_task gtid = __dios_this_task();
    _PThread &thr = getThread( gtid );

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner == &thr ) {
        // already locked by this thread
        assert( mutex->__lockCounter ); // count should be > 0
        if ( mutex->__type != PTHREAD_MUTEX_RECURSIVE ) {
            if ( mutex->__type == PTHREAD_MUTEX_ERRORCHECK )
                return EDEADLK;
            else
                __dios_fault( _VM_Fault::_VM_F_Locking, "Deadlock: Nonrecursive mutex locked again" );
        }
    }

    if ( !_mutex_can_lock( mutex, thr ) ) {
        assert( mutex->__lockCounter ); // count should be > 0
        if ( !wait )
            return EBUSY;
    }

    // mark waiting now for wait cycle detection
    // note: waiting should not be set in case of unsuccessfull try-lock
    // (cycle in try-lock is not deadlock, although it might be livelock)
    // so it must be here after return EBUSY
    thr.waiting_mutex = mutex;
    bool can_lock = _mutex_can_lock( mutex, thr );

    if ( !can_lock )
        mask.without( [] { __dios_reschedule(); } );

    if ( !_mutex_can_lock( mutex, thr ) )
    {
        if ( _check_deadlock( mutex, thr ) )
            __dios_suspend();
        else
            __vm_cancel();
    }

    thr.waiting_mutex = NULL;

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if ( err )
        return err;

    // lock the mutex
    acquireThread( thr );
    mutex->__owner = &thr;

    return 0;
}

int pthread_mutex_destroy( pthread_mutex_t *mutex ) noexcept {
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

int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( mutex == NULL )
        return EINVAL;

    {
        __dios::DetectFault f( _VM_F_Control );
        if ( mutex->__initialized && !f.triggered() ) {
            __dios_trace_t( "WARN: re-initializing mutex" );
            return EBUSY;
        }
    }

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

int pthread_mutex_lock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 1 );
}

int pthread_mutex_trylock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 0 );
}

int pthread_mutex_unlock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    _PThread &thr = getThread();

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner == nullptr ) {
        if ( mutex->__type == PTHREAD_MUTEX_NORMAL )
            __dios_fault( _VM_Fault::_VM_F_Locking,
                          "Attempting to unlock mutex which is not locked." );
        else
            return EPERM;
    }

    if ( mutex->__owner != &thr ) {
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
        releaseThread( *mutex->__owner );
        mutex->__owner = nullptr; // unlock if count == 0

    }
    return 0;
}

int pthread_mutex_getprioceiling( const pthread_mutex_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutex_setprioceiling( pthread_mutex_t *, int, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutex_timedlock( pthread_mutex_t *mutex, const struct timespec *abstime ) noexcept {
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

int pthread_mutexattr_destroy( pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    attr->__type = 0;
    return 0;
}

int pthread_mutexattr_init( pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    attr->__type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *value ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || value == NULL )
        return EINVAL;

    *value = attr->__type & _MUTEX_ATTR_TYPE_MASK;
    return 0;
}

int pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getprotocol( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getpshared( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_settype( pthread_mutexattr_t *attr, int value ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( value & ~_MUTEX_ATTR_TYPE_MASK ) )
        return EINVAL;

    attr->__type = value;
    return 0;
}

int pthread_mutexattr_setprioceiling( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_setprotocol( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_setpshared( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

/* Spin lock */

int pthread_spin_destroy( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_init( pthread_spinlock_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_lock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_trylock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_unlock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

/* Thread specific data */
int pthread_key_create( pthread_key_t *p_key, void ( *destructor )( void * ) ) noexcept {
    __dios::FencedInterruptMask mask;

    *p_key = tlsDestructors.getFirstAvailable();;
    tlsDestructors[ *p_key ] = destructor ?: []( void * ) { };

    // TODO: simulate errors?
    return 0;
}

int pthread_key_delete( pthread_key_t key ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( key >= tlsDestructors.count() )
        return EINVAL;

    iterateThreads( [&]( __dios_task tid ) {
            tls( tid ).setKey( key, nullptr );
            tls( tid ).shrink();
        } );
    tlsDestructors[ key ] = nullptr;
    tlsDestructors.shrink();
    return 0;
}

int pthread_setspecific( pthread_key_t key, const void *data ) noexcept {
    __dios::FencedInterruptMask mask;

    tls( __dios_this_task() ).setKey( key, data );
    return 0;
}

void *pthread_getspecific( pthread_key_t key ) noexcept {
    __dios::FencedInterruptMask mask;
    return tls( __dios_this_task() ).getKey( key );
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

template < typename CondOrBarrier > int _cond_adjust_count( CondOrBarrier *cond, int adj ) noexcept {
    int count = cond->__counter;
    count += adj;
    assert( count < ( 1 << 16 ) );
    assert( count >= 0 );

    cond->__counter = count;
    return count;
}

template < typename CondOrBarrier > int _destroy_cond_or_barrier( CondOrBarrier *cond ) noexcept {

    if ( cond == NULL || !cond->__initialized )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    assert( cond->__counter == 0 );

    cond->__counter = 0;
    cond->__initialized = 0;
    return 0;
}

int pthread_cond_destroy( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;

    int r = _destroy_cond_or_barrier( cond );
    if ( r == 0 )
        cond->__mutex = NULL;
    return r;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */ ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( cond == NULL )
        return EINVAL;

    {
        __dios::DetectFault f( _VM_F_Control );
        if ( cond->__initialized && !f.triggered() ) {
            __dios_trace_t( "WARN: re-initializing conditional variable" );
            return EBUSY;
        }
    }

    *cond = ( pthread_cond_t ){.__mutex = NULL, .__initialized = 1, .__counter = 0 };
    return 0;
}

template < typename > static inline SleepingOn _sleepCond() noexcept;
template <> inline SleepingOn _sleepCond< pthread_barrier_t >() noexcept {
    return Barrier;
}
template <> inline SleepingOn _sleepCond< pthread_cond_t >() noexcept {
    return Condition;
}

static inline bool _eqSleepTrait( _PThread &t, pthread_cond_t *cond ) noexcept {
    return t.condition == cond;
}

static inline bool _eqSleepTrait( _PThread &t, pthread_barrier_t *bar ) noexcept {
    return t.barrier == bar;
}

template< bool broadcast, typename CondOrBarrier >
static int _cond_signal( CondOrBarrier *cond ) noexcept {
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

            _PThread &thread = getThread( tid );

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

int pthread_cond_signal( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< false >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_broadcast( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< true >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();

    if ( cond == NULL || !cond->__initialized )
        return EINVAL;

    if ( mutex == NULL || !mutex->__initialized ) {
        return EINVAL;
    }

    if ( mutex->__owner != &thread ) {
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

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime ) noexcept {
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
int pthread_condattr_destroy( pthread_condattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_getclock( const pthread_condattr_t *, clockid_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_getpshared( const pthread_condattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_init( pthread_condattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_setclock( pthread_condattr_t *, clockid_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_setpshared( pthread_condattr_t *, int ) noexcept {
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
int pthread_setcancelstate( int state, int *oldstate ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( state & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldstate )
        *oldstate = thread.cancel_state;
    thread.cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( type & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldtype )
        *oldtype = thread.cancel_type;
    thread.cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t gtid ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread *thread = nullptr;
    {
        __dios::DetectFault f( _VM_F_Memory );
        thread = &getThread( gtid );
        if ( f.triggered() )
            return ESRCH;
    }

    if ( thread->cancel_state == PTHREAD_CANCEL_ENABLE )
        thread->cancelled = true;

    return 0;
}

void pthread_testcancel( void ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( _canceled() )
        _cancel( mask );
}

void pthread_cleanup_push( void ( *routine )( void * ), void *arg ) noexcept {
    __dios::FencedInterruptMask mask;

    assert( routine != NULL );

    _PThread &thread = getThread();
    CleanupHandler *handler = reinterpret_cast< CleanupHandler * >(
            __vm_obj_make( sizeof( CleanupHandler ) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = thread.cleanup_handlers;
    thread.cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();
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

static int _rlock_adjust_count( _ReadLock *rlock, int adj ) noexcept {
    int count = rlock->__count;
    count += adj;
    if ( count < 0 )
        return EAGAIN;
    rlock->__count = count;
    return 0;
}

static _ReadLock *_get_rlock( pthread_rwlock_t *rwlock, _PThread &tid, _ReadLock **prev = nullptr ) noexcept {
    _ReadLock *rlock = rwlock->__rlocks;
    _ReadLock *_prev = nullptr;

    while ( rlock && ( rlock->__owner != &tid ) ) {
        _prev = rlock;
        rlock = rlock->__next;
    }

    if ( rlock && prev )
        *prev = _prev;

    return rlock;
}

static _ReadLock *_create_rlock( pthread_rwlock_t *rwlock, _PThread &tid ) noexcept {
    _ReadLock *rlock = reinterpret_cast< _ReadLock * >( __vm_obj_make( sizeof( _ReadLock ) ) );
    acquireThread( tid );
    rlock->__owner = &tid;
    rlock->__count = 1;
    rlock->__next = rwlock->__rlocks;
    rwlock->__rlocks = rlock;
    return rlock;
}

static bool _rwlock_can_lock( pthread_rwlock_t *rwlock, bool writer ) noexcept {
    return !rwlock->__wrowner && !( writer && rwlock->__rlocks );
}

static int _rwlock_lock( __dios::FencedInterruptMask &mask, pthread_rwlock_t *rwlock, bool shouldwait, bool writer ) noexcept {
    _PThread &thr = getThread();

    if ( rwlock == nullptr || !rwlock->__initialized ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    if ( rwlock->__wrowner == &thr )
        // thread already owns write lock
        return EDEADLK;

    _ReadLock *rlock = _get_rlock( rwlock, thr );
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

    if ( writer ) {
        acquireThread( thr );
        rwlock->__wrowner = &thr;
    }
    else {
        if ( !rlock )
            rlock = _create_rlock( rwlock, thr );
        else {
            int err = _rlock_adjust_count( rlock, 1 );
            if ( err )
                return err;
        }
    }
    return 0;
}

int pthread_rwlock_destroy( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;

    if ( rwlock->__wrowner || rwlock->__rlocks )
        // rwlock is locked
        return EBUSY;

    rwlock->__initialized = 0;
    return 0;
}

int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;


    {
        __dios::DetectFault f( _VM_F_Control );
        if ( rwlock->__initialized && !f.triggered() ) {
            __dios_trace_t( "WARN: re-initializing rwlock" );
            return EBUSY;
        }
    }

    rwlock->__processShared = attr && bool( *attr & _RWLOCK_ATTR_SHARING_MASK );
    rwlock->__initialized = true;
    rwlock->__wrowner = nullptr;
    rwlock->__rlocks = nullptr;
    return 0;
}

int pthread_rwlock_rdlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, false );
}

int pthread_rwlock_wrlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, true );
}

int pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, false );
}

int pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, true );
}

int pthread_rwlock_unlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thr = getThread();

    if ( rwlock == NULL || !rwlock->__initialized ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    _ReadLock *rlock, *prev;
    rlock = _get_rlock( rwlock, thr, &prev );
    // existing read lock implies read lock counter having value more than zero
    assert( !rlock || rlock->__count );

    if ( rwlock->__wrowner != &thr && !rlock ) {
        // the current thread does not own the read-write lock
        return EPERM;
    }

    if ( rwlock->__wrowner == &thr ) {
        assert( !rlock );
        // release write lock
        releaseThread( *rwlock->__wrowner );
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
            releaseThread( *rlock->__owner );
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

int pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || pshared == NULL )
        return EINVAL;

    *pshared = *attr & _RWLOCK_ATTR_SHARING_MASK;
    return 0;
}

int pthread_rwlockattr_init( pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared ) noexcept {
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

int pthread_barrier_destroy( pthread_barrier_t *barrier ) noexcept {
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
        const pthread_barrierattr_t * /* TODO: barrier attributes */,
        unsigned count ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( count == 0 || barrier == NULL )
        return EINVAL;

    {
        __dios::DetectFault f( _VM_F_Control );
        if ( barrier->__initialized && !f.triggered() ) {
            __dios_trace_t( "WARN: re-initializing barrier" );
            return EBUSY;
        }
    }

    // Set the number of threads that must call pthread_barrier_wait() before
    // any of them successfully return from the call.
    *barrier = ( pthread_barrier_t ){ .__nthreads = ushort( count ), .__initialized = 1, .__counter = 0 };
    return 0;
}

int pthread_barrier_wait( pthread_barrier_t *barrier ) noexcept {
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
        _PThread &thread = getThread();
        thread.setSleeping( barrier );

        _cond_adjust_count( barrier, 1 ); // one more thread is blocked on this barrier

        // sleeping
        waitOrCancel( mask, [&] { return thread.sleeping == Barrier; } );
    }

    return ret;
}

/* Barrier attributes */
int pthread_barrierattr_destroy( pthread_barrierattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_getpshared( const pthread_barrierattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_init( pthread_barrierattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_setpshared( pthread_barrierattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

/* POSIX Realtime Extension - sched.h */
int sched_get_priority_max( int ) noexcept {
    /* TODO */
    return 0;
}

int sched_get_priority_min( int ) noexcept {
    /* TODO */
    return 0;
}

int sched_getparam( pid_t, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_getscheduler( pid_t ) noexcept {
    /* TODO */
    return 0;
}

int sched_rr_get_interval( pid_t, struct timespec * ) noexcept {
    /* TODO */
    return 0;
}

int sched_setparam( pid_t, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_setscheduler( pid_t, int, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_yield( void ) noexcept {
    /* TODO */
    return 0;
}

int pthread_yield( void ) noexcept {
    return 0;
}

#pragma GCC diagnostic pop
