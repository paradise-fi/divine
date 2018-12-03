// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <pthread.h>
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <sys/start.h>
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

struct CleanupHandler
{
    void ( *routine )( void * );
    void *arg;
    CleanupHandler *next;
};

enum SleepingOn { NotSleeping = 0, Condition, Barrier };

struct _PThread // (user-space) information maintained for every (running) thread
{
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

    void init() noexcept
    {
        if ( !_dtors )
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

using _PthreadAtFork = _PthreadHandlers< ForkHandler >;

struct Entry
{
    void *( *entry )( void * );
    void *arg;
};

struct _PthreadTLS
{
    _PthreadTLS( const _PthreadTLS & ) noexcept = delete;
    _PthreadTLS( _PthreadTLS && ) noexcept = delete;

    _PThread *thread;
    void *keys[0];

    void *raw() noexcept;
    static int raw_size( int cnt ) noexcept {
        return sizeof( __dios_tls ) + sizeof( _PThread * ) + cnt * sizeof( void * );
    }

    int keyCount() noexcept;
    void makeFit( int count ) noexcept;
    void shrink() noexcept;
    void *getKey( int key ) noexcept;
    void setKey( int key, const void *value ) noexcept;
    void destroy( int key ) noexcept;
};

static inline _PthreadTLS &tls( __dios_task tid ) noexcept
{
    return *reinterpret_cast< _PthreadTLS * >( &( tid->__data ) );
}

static inline _PThread &getThread( pthread_t tid ) noexcept
{
    return *tls( tid ).thread;
}

static inline _PThread &getThread() noexcept
{
    return getThread( __dios_this_task() );
}

static inline void acquireThread( _PThread &thread )
{
    ++thread.refcnt;
}

static inline void releaseThread( _PThread &thread )
{
    --thread.refcnt;
    if ( thread.refcnt == 0 )
        __vm_obj_free( &thread );
}

static inline void releaseAndKillThread( __dios_task tid )
{
    releaseThread( getThread( tid ) );
    __dios_kill( tid );
}

template< typename Yield >
static void iterateThreads( Yield yield ) noexcept
{
    auto *threads = __dios_this_process_tasks();
    int cnt = __vm_obj_size( threads ) / sizeof( __dios_task );
    for ( int i = 0; i < cnt; ++i )
        yield( threads[ i ] );
    __vm_obj_free( threads );
}

inline void* operator new ( size_t, void* p ) noexcept { return p; }

int _mutex_lock( __dios::FencedInterruptMask &mask, pthread_mutex_t *mutex, bool wait ) noexcept;
void __init_thread( const __dios_task gtid, const pthread_attr_t attr ) noexcept;
_Noreturn void _clean_and_become_zombie( __dios::FencedInterruptMask &mask, __dios_task tid ) noexcept;

template < typename Cond >
__attribute__( ( __always_inline__, __flatten__ ) )
static void wait( __dios::FencedInterruptMask &mask, Cond cond ) noexcept
{
    if ( cond() )
        mask.without( []{ __dios_reschedule(); } );
    if ( cond() )
        __vm_cancel();
}

#pragma GCC diagnostic pop
