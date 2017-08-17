#include "../sys/divm.h"
#include "../divine/__env.h"
#include "../divine/metadata.h"
#include <vm.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <iostream>
#include <pthread.h>
#include <condition_variable>
#include <map>
#include <vector>

#define NATIVE_ASSERT( x ) do { if ( !(x) ) assertFn( #x, __FILE__, __LINE__, __PRETTY_FUNCTION__ ); } while ( false )
#define forceinline __attribute__((__always_inline__))

namespace nativeRuntime {

using Fault = _VM_Fault;
using FaultHandler = __sys_fault_t;
using SchedFn = void *(*)( int, void * );
using Frame = _VM_Frame;

using MutexGuard = std::unique_lock< std::recursive_mutex >;

struct UnlockGuard {
    UnlockGuard( MutexGuard &g ) : g( g ) {
        g.unlock();
    }

    ~UnlockGuard() { g.lock(); }

  private:
    MutexGuard &g;
};

static_assert( sizeof( Frame().pc ) == sizeof( unw_word_t ), "Invalid Frame field" );

_Noreturn void assertFn( const char *msg, const char *file, int line, const char *func ) {
    fprintf( stderr, "ASSERTION FAILED: \"%s\" in %s (%s:%d)", msg, func, file, line );
    std::abort();
}

std::recursive_mutex rtMutex;

struct Signal {
    void send( MutexGuard & ) {
        sig = true;
        turn.notify_one();
    }

    void wait( MutexGuard &g ) {
        turn.wait( g, [this]() { return sig; } );
        sig = false;
    }

  private:
    std::condition_variable_any turn;
    bool sig;
};

bool maskFlag;
bool unmaskInterrupts;

bool isInterruptPoint( MutexGuard & ) { return true; }
int getChoice() { return 0; }

FaultHandler faultHandler = nullptr;

bool vmFaultInvoked = false;

_Noreturn void doubleFault( Fault f ) noexcept {
    fprintf( stderr, "Double fault: %d, exiting.", int( f ) );
    std::abort();
}

struct FaultGuard {
    FaultGuard( Fault t ) noexcept {
        if ( vmFaultInvoked )
            doubleFault( t );
        vmFaultInvoked = true;
    }
    ~FaultGuard() {
        vmFaultInvoked = false;
    }
};

SchedFn sched;

Frame **interruptedFrameLocation;

struct Heap {
    struct Alloc {
        Alloc *next;
        size_t size;
        bool vmManaged;
        char data[0];
    };

    template< typename T = void, bool vmAlloc = false >
    T *allocate( int size = sizeof( T ) ) {
        NATIVE_ASSERT( size > 0 );
        Alloc *a = static_cast< Alloc * >( operator new( size + sizeof( Alloc ) ) );
        a->size = size;
        a->vmManaged = vmAlloc;

        if ( !allocs || a < allocs ) {
            a->next = allocs;
            allocs = a;
            return reinterpret_cast< T * >( a->data );
        }

        Alloc *pre = allocs;
        Alloc *post = allocs->next;
        while ( pre ) {
            if ( pre < a && (a < post || post == nullptr) ) {
                pre->next = a;
                a->next = post;
                return reinterpret_cast< T * >( a->data );
            }
            pre = post;
            post = post->next;
        }

        NATIVE_ASSERT( !"unreachable" );
        __builtin_unreachable();
    }

    void free( void *ptr, bool checkExists = true ) {
        if ( !ptr )
            return;

        Alloc **a = &allocs;
        while ( *a ) {
            if ( (*a)->data <= ptr && ptr < (*a)->data + (*a)->size ) {
                Alloc *todel = *a;
                *a = (*a)->next;
                operator delete( todel );
                return;
            }
            a = &((*a)->next);
        }
        NATIVE_ASSERT( !checkExists && "Invalid free" );
    }

    Alloc *findAlloc( void *ptr ) {
        Alloc *a = allocs;
        while ( a ) {
            if ( ptr < a )
                return nullptr;
            if ( a->data <= ptr && ptr < a->data + a->size )
                return a;
            a = a->next;
        }
        return nullptr;
    }

    size_t size( void *ptr ) {
        Alloc *a = findAlloc( ptr );
        return a ? a->size : 0;
    }

    Alloc *allocs;
};

Heap heap;

struct ThreadAction {
    virtual ~ThreadAction() {
        NATIVE_ASSERT( activated );
    }

    void run( MutexGuard &g ) {
        NATIVE_ASSERT( !activated );
        activated = true;
        doRun( g );
    }

  private:
    virtual void doRun( MutexGuard &g ) = 0;

    bool activated = false;
};

struct Thread {
    Thread( int id ) : id( id ) { }

    explicit operator bool() const { return id > 0; }

    int id = -1;
    bool ended = false;
    bool scheduler = false;
    Signal activate;
    std::unique_ptr< ThreadAction > action;
};

std::map< pthread_t, Thread * > threadMap;
std::vector< std::unique_ptr< Thread > > threads;
Thread *schedThread;

void nuke( Frame *f ) {
}

struct ActivateNewAction : ThreadAction {
    ActivateNewAction( Frame *f ) : f( f ) { }

    constexpr static int argCountLimit = 4;

  private:
    template< typename List, typename ... Ts >
    static void invoke( List, Frame *f, char *ptr, Ts... vals ) {
        using Head = typename List::Head;
        auto val = *reinterpret_cast< Head * >( ptr );
        ptr += sizeof( Head );
        return invoke( typename List::Tail(), f, ptr, vals..., val );
    }

    template< typename ... Ts >
    static void invoke( metadata::TypeSig::TList<>, Frame *f, char *, Ts... vals ) {
        auto func = reinterpret_cast< void (*)( Ts... ) >( f->pc );
        nuke( f );
        func( vals... );
    }

    void doRun( MutexGuard &g ) override {
        auto *meta = __md_get_pc_meta( uintptr_t( f->pc ) );
        metadata::TypeSig sig( meta->type );
        NATIVE_ASSERT( meta->arg_count <= argCountLimit );

        UnlockGuard _( g );
        NATIVE_ASSERT( sig[ 0 ] == metadata::TypeSig::encodeOne< void > );
        ++sig;
        sig.decode< argCountLimit, metadata::TypeSig::ArgTypes >(
            [this]( auto list ) {
                invoke( metadata::TypeSig::ToTypes< decltype( list ) >(),
                        f, reinterpret_cast< char * >( f + 1 ) );
            } );
    }

    Frame *f;
};

FrameEx *getVMExtension( Frame *frame ) {
    return reinterpret_cast< FrameEx * >( frame + 1 );
}

FrameEx *getExtension( Frame *frame, Heap::Alloc *a = nullptr ) {
    a = a ? a : heap.findAlloc( frame );
    if ( a->vmManaged )
        return getVMExtension( frame );
    NATIVE_ASSERT( a->data == reinterpret_cast< char * >( frame ) );
    return reinterpret_cast< FrameEx * >( a->data + a->size - sizeof( FrameEx ) );
}

struct ActivateAction : ThreadAction {
    ActivateAction( Frame *f ) : f( f ) { }

  private:
    void doRun( MutexGuard &g ) override {
        NATIVE_ASSERT( getExtension( f )->child != nullptr
                && "Cannot explicitly jump native runtime control frame" );
        NATIVE_ASSERT( getExtension( getExtension( f )->child )->child == nullptr
                && "Cannot invoke frame to a function other then the one on the top of the stack" );
        // TODO: Assert PC did not change
        nuke( f );
        // note: the caller of run will just return to the point where the
        // function was interrupted
    }

    Frame *f;
};

Frame forceinline *buildStackShadow() {
    auto tid = threadMap[ pthread_self() ]->id;
    NATIVE_ASSERT( tid > 0 );
    auto *ctx = heap.allocate< unw_context_t, true >( 4096 - sizeof( Heap::Alloc ) );
    char *block = reinterpret_cast< char * >( ctx );
    int r = unw_getcontext( ctx );
    NATIVE_ASSERT( r == 0 && "unw_getcontext failed" );

    auto *frame = reinterpret_cast< Frame * >( ctx + 1 );
    auto *topFrame = frame;
    getVMExtension( frame )->child = nullptr;
    getVMExtension( frame )->uwctx = ctx;
    getVMExtension( frame )->tid = tid;
    r = unw_init_local( &getVMExtension( frame )->uwcur, ctx );
    NATIVE_ASSERT( r == 0 && "unw_init_local failed" );

    do {
        unw_get_reg( &getVMExtension( frame )->uwcur, UNW_REG_IP,
                reinterpret_cast< unw_word_t * >( &frame->pc ) );
        Frame *nxt;
        if ( reinterpret_cast< char * >( frame ) + (2 * (sizeof( FrameEx ) + sizeof( Frame ))) > block + 4096 ) {
            nxt = heap.allocate< Frame >( 4096 - sizeof( Heap::Alloc ) );
            block = reinterpret_cast< char * >( nxt );
        }
        else
            nxt = reinterpret_cast< Frame * >(
                    reinterpret_cast< char * >( frame ) + sizeof( Frame ) + sizeof( FrameEx ) );
        nxt->parent = nullptr;
        getVMExtension( nxt )->child = frame;
        frame->parent = nxt;
        getVMExtension( nxt )->uwctx = ctx;
        getVMExtension( nxt )->tid = tid;
        getVMExtension( nxt )->uwcur = getVMExtension( frame )->uwcur;
        frame = nxt;
    } while( (r = unw_step( &getVMExtension( frame )->uwcur )) > 0 );
    NATIVE_ASSERT( r == 0 && "unw_step failed" );

    getVMExtension( frame )->child->parent = nullptr;
    heap.free( frame );
    return topFrame;
}

void waitForActivation( MutexGuard &g, Thread &t ) {
    t.activate.wait( g );
    t.action->run( g );
}

void forceinline interrupt( MutexGuard &g ) {
    auto *frame = buildStackShadow();
    if ( interruptedFrameLocation ) {
        *interruptedFrameLocation = frame->parent; // the callee of __vm_*interrupt
        interruptedFrameLocation = nullptr;
    }

    schedThread->activate.send( g );
    waitForActivation( g, *threadMap[ pthread_self() ] );
}

void threadStart( Thread *t ) {
    MutexGuard g( rtMutex );
    NATIVE_ASSERT( t );
    waitForActivation( g, *t );

    // thread ended, we should abandon it and go to scheduler
    threadMap[ pthread_self() ]->ended = true;
    if ( interruptedFrameLocation ) {
        *interruptedFrameLocation = nullptr;
        interruptedFrameLocation = nullptr;
    }
    schedThread->activate.send( g );
}

void swapStack( MutexGuard &g, Frame *f )
{
    auto *ext = getExtension( f );
    auto *self = threadMap[ pthread_self() ];

    if ( ext->tid == 0 ) { // no stack allocated yet
        pthread_t tid;
        threads.emplace_back( std::make_unique< Thread >( threads.size() ) );
        Thread *ref = threads.back().get();

        pthread_create( &tid, nullptr,
            []( void *x ) -> void * {
                threadStart( static_cast< Thread * >( x ) );
                return nullptr;
            }, static_cast< void * >( ref ) );
        threadMap.emplace( tid, ref );

        NATIVE_ASSERT( ref->id > 0 );
        ext->tid = ref->id;
        ref->action = std::make_unique< ActivateNewAction >( f );
        ref->activate.send( g );
    } else {
        Thread *ref = threads[ ext->tid ].get();
        NATIVE_ASSERT( !ref->ended && "Trying to jump to an ended thread" );
        ref->action = std::make_unique< ActivateAction >( f );
        ref->activate.send( g );
    }
    self->activate.wait( g );
}

}

using namespace nativeRuntime;

// this is firts thing called from _start
extern "C" void __native_start_rt() __attribute__((__annotate__("divine.link.always"),
                                                   __visibility__("default")))
{
    threads.emplace_back( nullptr ); // dummy thread 0 (invalid)
    threads.emplace_back( std::make_unique< Thread >( threads.size() ) );
    schedThread = threads.back().get();
    schedThread->scheduler = true;
    threadMap.emplace( pthread_self(), schedThread );

    auto *schedState = __sys_init( __sys_env_get() );
    while ( schedState ) {
        for ( auto it = threadMap.begin(), end = threadMap.end(); it != end; ++it ) {
            if ( it->second->ended ) {
                pthread_join( it->first, nullptr );
                threadMap.erase( it );
            }
        }
        schedState = sched( heap.size( schedState ), schedState );
    }
    std::exit( 0 );
}

void __vm_trace( const char *what ) noexcept {
    MutexGuard _( rtMutex );
    std::fputs( what, stderr );
    std::fputc( '\n', stderr );
}

int __vm_mask( int val ) noexcept {
    MutexGuard g( rtMutex );
    bool old = maskFlag;
    maskFlag = val;
    if ( !maskFlag && unmaskInterrupts ) {
        unmaskInterrupts = false;
        interrupt( g );
    }
    return old;
}

void __vm_mem_interrupt() noexcept {
    MutexGuard g( rtMutex );
    if ( isInterruptPoint( g ) ) {
        if ( maskFlag )
            unmaskInterrupts = true;
        else
            interrupt( g );
    }
}

void __vm_cfl_interrupt() noexcept {
    MutexGuard g( rtMutex );
    if ( isInterruptPoint( g ) ) {
        if ( maskFlag )
            unmaskInterrupts = true;
        else
            interrupt( g );
    }
}

int __vm_interrupt( int v ) noexcept {
    MutexGuard g( rtMutex );
    bool old = unmaskInterrupts;
    if ( v ) {
        if ( maskFlag ) {
            old = unmaskInterrupts;
            unmaskInterrupts = true;
        } else {
            unmaskInterrupts = false;
            interrupt( g );
        }
    } else if ( unmaskInterrupts && maskFlag )
        unmaskInterrupts = false;
    return old;
}

void __vm_set_fault( FaultHandler f ) noexcept {
    MutexGuard _( rtMutex );
    faultHandler = f;
}

void __vm_fault( enum _VM_Fault t, ... ) noexcept {
    MutexGuard g( rtMutex );
    FaultGuard _( t );
    faultHandler( t, 0, 0 ); /* FIXME */
    fprintf( stderr, "Fault handler returned, aborting." );
    std::abort();
}

void __vm_set_sched( SchedFn s ) noexcept {
    MutexGuard _( rtMutex );
    sched = s;
}

void __vm_set_ifl( Frame **where ) noexcept {
    MutexGuard _( rtMutex );
    interruptedFrameLocation = where;
}

void __vm_jump( Frame *dest, int forgetful ) noexcept {
    MutexGuard g( rtMutex );
    if ( forgetful )
        maskFlag = false;
    swapStack( g, dest );
}

int __vm_choose( int n, ... ) noexcept {
    MutexGuard _( rtMutex );
    int v = getChoice();
    NATIVE_ASSERT( v < n && v >= 0 );
    return v;
}

void *__vm_make_object( int size ) noexcept {
    MutexGuard _( rtMutex );
    return heap.allocate( size );
}

void __vm_free_object( void *ptr ) noexcept {
    MutexGuard _( rtMutex );
    return heap.free( ptr );
}
int __vm_query_object_size( void *ptr ) noexcept {
    MutexGuard _( rtMutex );
    return heap.size( ptr );
}

// this will be repalced by LART with ‘llvm.va_start‘ Intrinsic
void *__vm_query_varargs() noexcept;

Frame *__vm_query_frame() noexcept {
    MutexGuard _( rtMutex );
    return buildStackShadow()->parent;
}

void *__vm_query_varargs() noexcept { }
void *__sys_env[1] __attribute__((__visibility__("default")));
