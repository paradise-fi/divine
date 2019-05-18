// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018, 2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <dios/sys/kobject.hpp>
#include <util/array.hpp>
#include <sys/metadata.h>
#include <utility>

namespace __dios
{

    template < typename T >
    struct task_array : Array< std::unique_ptr< T > >
    {
        using tid_t = decltype( std::declval< T >().get_id() );

        T *find( tid_t id ) noexcept
        {
            for ( auto &t : *this )
                if ( t->get_id() == id )
                    return t.get();
            return nullptr;
        }

        bool remove( tid_t id ) noexcept
        {
            for ( auto &t : *this )
            {
                if ( t ->get_id() != id )
                    continue;
                std::swap( t, this->back() );
                this->pop_back();
                return true;
            }
            return false;
        }
    };

    template < typename process >
    struct task : KObject
    {
        _VM_Frame *_frame;
        __dios_tls *_tls;
        process *_proc;
        const _MD_Function *_fun;

        task() : _frame( nullptr ),
                 _tls( static_cast< __dios_tls * >(
                             __vm_obj_make( sizeof( __dios_tls ) , _VM_PT_Heap ) ) ),
                 _proc( nullptr ), _fun( nullptr )
        {}

        template< typename F >
        task( F routine, int tls_size, process *proc ) noexcept
            : _frame( nullptr ),
              _proc( proc ),
              _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
        {
            setup_stack();
            const auto size = sizeof( __dios_tls ) + tls_size;
            _tls = static_cast< __dios_tls * >( __vm_obj_make( size, _VM_PT_Heap ) );
            _tls->__errno = 0;
        }

        template< typename F >
        task( void *frame, void *tls, F routine, int tls_size, process *proc ) noexcept
            : _frame( nullptr ),
              _proc( proc ),
              _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
        {
            setup_stack( frame );
            _tls = static_cast< __dios_tls * >( tls );
            __vm_obj_resize( _tls, sizeof( __dios_tls ) + tls_size );
            _tls->__errno = 0;
        }

        task( const task& o ) noexcept = delete;
        task& operator=( const task& o ) noexcept = delete;

        task( task &&o ) noexcept
            : _frame( o._frame ), _tls( o._tls ), _proc( o._proc )
        {
            o._frame = nullptr;
            o._tls = nullptr;
        }

        task &operator=( task &&o ) noexcept
        {
            std::swap( _frame, o._frame );
            std::swap( _tls, o._tls );
            std::swap( _proc, o._proc );
            return *this;
        }

        ~task() noexcept
        {
            free_stack();
            __vm_obj_free( _tls );
        }

        bool active() const noexcept { return _frame; }
        __dios_task get_id() const noexcept { return _tls; }
        uint32_t get_user_id() const noexcept
        {
            auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
            return static_cast< uint32_t >( tid );
        }

        void setup_stack( void *frame = nullptr ) noexcept
        {
            free_stack();
            if ( frame )
            {
                _frame = static_cast< _VM_Frame * >( frame );
                __vm_obj_resize( _frame, _fun->frame_size );
            }
            else
                _frame = static_cast< _VM_Frame * >( __vm_obj_make( _fun->frame_size, _VM_PT_Heap ) );
            _frame->pc = _fun->entry_point;
            _frame->parent = nullptr;
        }

    private:

        void free_stack() noexcept
        {
            if ( _frame && _tls == __dios_this_task() )
                __dios_stack_cut( __dios_this_frame(), _frame );
            if ( _frame )
                __dios_stack_free( _frame, nullptr );

            _frame = nullptr;
        }
    };

}
