// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <dios.h>
#include <dios/syscall.hpp>
#include <dios/fault.hpp>
#include <divine/metadata.h>

namespace __sc {

void start_thread( __dios::Context& ctx, int *err, void *retval, va_list vl );
void kill_thread( __dios::Context& ctx, int *err, void *retval, va_list vl );
void kill_process( __dios::Context& ctx, int *err, void *retval, va_list vl );
void get_process_threads( __dios::Context &ctx, int *err, void *_ret, va_list vl );


} // namespace __sc

namespace __dios {

using ThreadId = _DiOS_ThreadId;
using ProcId = _DiOS_ProcId;

struct DiosMainFrame : _VM_Frame {
    int l;
    int argc;
    char** argv;
    char** envp;
};

struct ThreadRoutineFrame : _VM_Frame {
    void* arg;
};

struct CleanupFrame : _VM_Frame {
    int reason;
};

template < class T >
struct SortedStorage {
    using Tid = decltype( std::declval< T >().getId() );
    SortedStorage() {}
    SortedStorage( const SortedStorage & ) = delete;

    T *find( Tid id ) noexcept {
        for ( auto t : *this )
            if ( t->getId() == id )
                return t;
        return nullptr;
    }

    bool remove( Tid id ) noexcept {
        int s = size();
        for ( int i = 0; i != s; i++ ) {

            if ( _storage[ i ]->getId() != id )
                continue;
            delete_object( _storage[ i ] );
            _storage[ i ] = _storage[ s - 1 ];
            resize ( size() - 1 );
            sort();
            return true;
        }
        return false;
    }

    template < class... Args >
    T *emplace( Args... args ) noexcept {
        resize( size() + 1 );
        int idx = size() - 1;
        T *ret = _storage[ idx ] = new_object< T >( args... );
        sort();
        return ret;
    }

    void erase( T** first, T** last ) noexcept {
        if ( empty() )
            return;
        int orig_size = size();
        for ( T** f = first; f != last; f++ ) {
            delete_object( *f );
        }
        int s = last - first;
        if ( s != orig_size )
            memmove( first, last, ( end() - last ) * sizeof( T * ) );
        resize( orig_size - s );
        sort();
    }

    void resize( int n ) { __vm_obj_resize( this, std::max( 1,
                                    static_cast< int > (sizeof( *this ) + n * sizeof( T * ) ) ) ); }
    T **begin() noexcept { return _storage; }
    T **end() noexcept { return _storage + size(); }
    int size() const noexcept { return ( __vm_obj_size( this ) - sizeof( *this ) ) / sizeof( T * ); }
    bool empty() const noexcept { return size() == 0; };
    T *operator[]( int i ) const noexcept { return _storage[ i ]; };
private:
    void sort() {
        if ( empty() )
            return;
        std::sort( begin(), end(), []( T *a, T *b ) {
            return a->getId() < b->getId();
        });
    }

    T *_storage[];
};

struct Thread {
    _VM_Frame *_frame;
    void *     _tls;
    ProcId     _pid;

    template <class F>
    Thread( F routine, int tls_size, void *fMem = nullptr ) noexcept {
        auto fun = __md_get_pc_meta( reinterpret_cast< uintptr_t >( routine ) );
        if ( fMem )
            __vm_obj_resize( fMem, fun->frame_size );
        else
            fMem = __vm_obj_make( fun->frame_size );
        _frame = static_cast< _VM_Frame * >( fMem );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = __vm_obj_make( std::max( tls_size,  int( _DiOS_TLS_Reserved ) ) );
        std::memset( _tls, 0, _DiOS_TLS_Reserved );
        _pid = nullptr; // ToDo: Add process support
    }

    Thread( const Thread& o ) noexcept = delete;
    Thread& operator=( const Thread& o ) noexcept = delete;

    Thread( Thread&& o ) noexcept;
    Thread& operator=( Thread&& o ) noexcept;

    ~Thread() noexcept;

    bool active() const noexcept { return _frame; }
    ThreadId getId() const noexcept { return _tls; }
    uint32_t getUserId() const noexcept {
        auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
        return static_cast< uint32_t >( tid );
    }

private:
    void clearFrame() noexcept;
};

struct Scheduler {
    Scheduler() {}
    int threadCount() const noexcept { return threads.size(); }
    Thread *chooseThread() noexcept;
    void traceThreads() const noexcept;

    void startMainThread( int argc, char** argv, char** envp, void* fMem = nullptr ) noexcept;
    ThreadId startThread( void ( *routine )( void * ), void *arg, int tls_size, void* fMem = nullptr ) noexcept;
    void killThread( ThreadId t_id ) noexcept;
    void killProcess( ProcId id ) noexcept;

    SortedStorage< Thread > threads;
};

template < bool THREAD_AWARE_SCHED >
void sched() noexcept
{
    auto ctx = static_cast< Context * >( __vm_control( _VM_CA_Get, _VM_CR_State ) );
    if ( THREAD_AWARE_SCHED )
        ctx->scheduler->traceThreads();
    Thread *t = ctx->scheduler->chooseThread();
    while ( t && t->_frame )
    {
        __vm_control( _VM_CA_Set, _VM_CR_User2,
            reinterpret_cast< int64_t >( t->getId() ) );
        __vm_control( _VM_CA_Set, _VM_CR_Frame, t->_frame,
                      _VM_CA_Bit, _VM_CR_Flags,
                      uintptr_t( _VM_CF_Interrupted | _VM_CF_Mask | _VM_CF_KernelMode ), 0ull );
        t->_frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_IntFrame ) );

        if ( ctx->syscall->handle( ctx ) == SchedCommand::RESCHEDULE )
            return;

        /* reset intframe to ourselves */
        auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
        __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );

    }

    // Program ends
    ctx->finalize();
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
}

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
