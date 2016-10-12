// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/pointer.hpp>
#include <divine/vm/debug.hpp>
#include <divine/vm/program.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/eval.hpp>
#include <divine/vm/setup.hpp>
#include <divine/vm/print.hpp>
#include <set>

namespace divine {
namespace vm {

struct Stepper
{
    GenericPointer _frame, _frame_cur, _parent_cur;
    bool _ff_kernel;
    bool _stop_on_fault, _stop_on_error;
    std::pair< int, int > _lines, _instructions, _states, _jumps;
    std::pair< std::string, int > _line;
    std::set< CodePointer > _bps;

    Stepper()
        : _frame( nullPointer() ),
          _frame_cur( nullPointer() ),
          _parent_cur( nullPointer() ),
          _ff_kernel( false ),
          _lines( 0, 0 ), _instructions( 0, 0 ),
          _states( 0, 0 ), _jumps( 0, 0 ),
          _stop_on_fault( false ), _stop_on_error( true ),
          _line( "", 0 )
    {}

    void lines( int l ) { _lines.second = l; }
    void instructions( int i ) { _instructions.second = i; }
    void states( int s ) { _states.second = s; }
    void jumps( int j ) { _jumps.second = j; }
    void frame( GenericPointer f ) { _frame = f; }
    auto frame() { return _frame; }

    void add( std::pair< int, int > &p )
    {
        if ( _frame.null() || _frame == _frame_cur )
            p.first ++;
    }

    bool _check( std::pair< int, int > &p )
    {
        return p.second && p.first >= p.second;
    }

    template< typename Context, typename Eval >
    bool check( Context &ctx, Eval &eval, bool breakpoints )
    {
        if ( breakpoints )
            for ( auto bp : _bps )
                if ( eval.pc() == bp )
                    return true;

        if ( !_frame.null() && !ctx.heap().valid( _frame ) )
            return true;
        if ( _check( _jumps ) )
            return true;
        if ( !_frame.null() && _frame_cur != _frame )
            return false;
        return _check( _lines ) || _check( _instructions ) || _check( _states );
    }

    void instruction( Program::Instruction &i )
    {
        add( _instructions );
        if ( i.op )
        {
            auto l = fileline( *llvm::cast< llvm::Instruction >( i.op ) );
            if ( _line.second && l != _line )
                add( _lines );
            if ( _frame.null() || _frame == _frame_cur )
                _line = l;
        }
    }

    void state() { add( _states ); }

    template< typename Heap >
    void in_frame( GenericPointer next, Heap &heap )
    {
        GenericPointer last = _frame_cur, last_parent = _parent_cur;
        value::Pointer next_parent;

        heap.read( next + PointerBytes, next_parent );

        _frame_cur = next;
        _parent_cur = next_parent.cooked();

        if ( last == next )
            return; /* no change */
        if ( last.null() || next.null() )
            return; /* entry or exit */

        if ( next == last_parent )
            return; /* return from last into next */
        if ( next_parent.cooked() == last )
            return; /* call from last into next */

        ++ _jumps.first;
    }

    template< typename Context, typename YieldState >
    bool schedule( Context &ctx, YieldState yield )
    {
        if ( !ctx.frame().null() )
            return false; /* nothing to be done */

        if ( ctx.ref( _VM_CR_Flags ).integer & _VM_CF_Cancel )
            return true;

        yield( ctx.snapshot() );
        vm::setup::scheduler( ctx );
        return true;
    }

    template< typename Context, typename YieldState, typename SchedPolicy >
    void run( Context &ctx, YieldState yield, SchedPolicy sched_policy, bool verbose )
    {
        Eval< typename Context::Program, Context, value::Void > eval( ctx.program(), ctx );
        bool in_fault = !_stop_on_fault ||
                        eval.pc().function() == ctx.get( _VM_CR_FaultHandler ).pointer.object();
        bool in_kernel = ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;
        bool error_set = !_stop_on_error || ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error;
        bool moved = false;

        while ( !ctx.frame().null() &&
                ( ( _ff_kernel && in_kernel ) || !check( ctx, eval, moved ) ) &&
                ( error_set || ( ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error ) == 0 ) &&
                ( in_fault || eval.pc().function()
                  != ctx.get( _VM_CR_FaultHandler ).pointer.object() ) )
        {
            in_kernel = ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;
            moved = true;

            if ( in_kernel && _ff_kernel )
                eval.advance();
            else
            {
                in_frame( ctx.frame(), ctx.heap() );
                eval.advance();
                instruction( eval.instruction() );
            }

            if ( verbose && ( !in_kernel || !_ff_kernel ) )
            {
                auto frame = ctx.frame();
                std::string before = vm::print::instruction( eval, 2 );
                eval.dispatch();
                if ( ctx.heap().valid( frame ) )
                {
                    auto newframe = ctx.frame();
                    ctx.set( _VM_CR_Frame, frame ); /* :-( */
                    std::cerr << "  " << vm::print::instruction( eval, 2 ) << std::endl;
                    ctx.set( _VM_CR_Frame, newframe );
                }
                else
                    std::cerr << "  " << before << std::endl;
            }
            else
                eval.dispatch();

            if ( schedule( ctx, yield ) )
                state();

            in_kernel = ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;
            if ( !in_kernel || !_ff_kernel )
                in_frame( ctx.frame(), ctx.heap() );

            sched_policy();

            for ( auto t : ctx._trace )
                std::cerr << "T: " << t << std::endl;
            ctx._trace.clear();
        }
        ctx.sync_pc();
    }

};

}
}

