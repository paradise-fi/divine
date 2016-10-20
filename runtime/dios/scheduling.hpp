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

void start_thread( __dios::Context& ctx, void *retval, va_list vl );
void kill_thread( __dios::Context& ctx, void *retval, va_list vl );
void kill_process( __dios::Context& ctx, void *retval, va_list vl );
void get_process_threads( __dios::Context &ctx, void *_ret, va_list vl );

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
    SortedStorage(): _storage( nullptr ) { }

    T *find( Tid id ) const noexcept {
        if ( !_storage )
            return nullptr;
        for ( auto t : *this ) {
            if ( t->getId() == id )
                return t;
        }
    }

    bool remove( Tid id ) noexcept {
        if ( !_storage )
            return;
        size_t s = size();
        for ( size_t i = 0; i != s; i++ ) {

            if ( _storage[ i ]->getId() != id )
                continue;
            delete_object( _storage[ i ] );
            _storage[ i ] = _storage[ s - 1 ];
            if ( s == 1 ) {
                __vm_obj_free( _storage );
                _storage = nullptr;
            }
            else {
                __vm_obj_resize( _storage, __vm_obj_size( _storage ) - sizeof( T * ) );
            }
            sort();
            return true;
        }
        return false;
    }

    template < class... Args >
    T *emplace( Args... args ) noexcept {
        if ( !_storage )
            _storage = static_cast< T** >( __vm_obj_make( sizeof( T * ) ) );
        else
            __vm_obj_resize( _storage, __vm_obj_size( _storage ) + sizeof( T * ) );
        size_t idx = size() - 1;
        T *ret = _storage[ idx ] = new_object< T >( args... );
        sort();
        return ret;
    }

    void erase( T** item ) noexcept {
        delete_object( *item );
        size_t s = size();
        if ( s == 1) {
            __vm_obj_free( _storage );
            _storage = nullptr;
        } else {
            *item = _storage[ s - 1 ];
            __vm_obj_resize( _storage, ( s - 1) * sizeof( T *) );
        }
        sort();
    }

    void erase( T** first, T** last ) noexcept {
        if ( empty() )
            return;
        size_t orig_size = size();
        for ( T** f = first; f != last; f++ ) {
            delete_object( *f );
        }
        size_t s = last - first;
        if ( s == orig_size ) {
            __vm_obj_free( _storage );
            _storage = nullptr;
        }
        else {
            memmove( first, last, ( end() - last ) * sizeof( T * ) );
            __vm_obj_resize( _storage, ( orig_size - s) * sizeof( T * ) );
        }
        sort();
    }

    T **begin() const noexcept { return _storage; }
    T **end() const noexcept { return _storage + size(); }
    size_t size() const noexcept { return _storage ? __vm_obj_size( _storage ) / sizeof( T * ) : 0; }
    bool empty() const noexcept { return !_storage; };
    T *operator[]( size_t i ) const noexcept { return _storage[ i ]; };
private:
    void sort() {
        if ( empty() )
            return;
        std::sort( begin(), end(), []( T *a, T *b ) {
            return a->getId() < b->getId();
        });
    }

    T **_storage;
};

struct Thread {
    _VM_Frame *_frame;
    void *     _tls;
    ProcId     _pid;

    template <class F>
    Thread( F routine, size_t tls_size ) noexcept {
        auto fun = __md_get_pc_meta( reinterpret_cast< uintptr_t >( routine ) );
        _frame = static_cast< _VM_Frame * >( __vm_obj_make( fun->frame_size ) );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = __vm_obj_make( std::max( tls_size, 1ul ) );
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
    size_t threadCount() const noexcept { return threads.size(); }
    Thread *chooseThread() noexcept;
    void traceThreads() const noexcept;
    void startMainThread( int argc, char** argv, char** envp ) noexcept;
    ThreadId startThread( void ( *routine )( void * ), void *arg, size_t tls_size ) noexcept;
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

        if ( !ctx->syscall->handle( ctx ) )
            return;

        /* reset intframe to ourselves */
        auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
        __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );

    }

    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
}

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
