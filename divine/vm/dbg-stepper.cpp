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

#include <divine/vm/dbg-stepper.hpp>
#include <divine/vm/dbg-context.hpp>
#include <divine/vm/dbg-print.hpp>
#include <divine/vm/eval.hpp>

#include <divine/vm/run.hpp>

namespace divine::vm::dbg
{

template< typename Context >
void Stepper< Context >::run( Context &ctx, Verbosity verb )
{
    Eval< Context, value::Void > eval( ctx );
    auto fault_handler = ctx.get( _VM_CR_FaultHandler ).pointer.object();
    bool error_set = !_stop_on_error || ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error;
    bool moved = false, in_fault, rewind_to_fault = false;
    CodePointer oldpc = eval.pc();
    Context _backup( ctx.program(), ctx.debug() );

    while ( !_sigint && !ctx.frame().null() )
    {
        in_fault = eval.pc().function() == fault_handler;

        if ( !error_set && ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error )
        {
            if ( rewind_to_fault )
                ctx = _backup;
            ctx.flush_ptr2i();
            break;
        }

        bool ff = ctx.in_component( _ff_components );

        if ( ff )
            eval.advance();
        else
        {
            if ( _check( _states ) )
                break;

            if ( check( ctx, eval, oldpc, moved ) )
                break;

            if ( _stop_on_fault && in_fault )
                break;

            if ( _stop_on_error && in_fault && !rewind_to_fault && _ff_components )
            {
                rewind_to_fault = true;
                _backup = ctx;
                ctx.flush_ptr2i();
            }

            in_frame( ctx.frame(), ctx.heap() );
            eval.advance();
            instruction();
        }

        moved = true;
        oldpc = eval.pc();
        auto frame = ctx.frame();

        if ( ( verb == PrintInstructions || verb == TraceInstructions ) && !ff )
        {
            std::string output = print::instruction( ctx.debug(), eval, 2 );
            std::string indent = ( verb == PrintInstructions ) ? "  " : "";
            eval.dispatch();
            if ( ctx.heap().valid( frame ) )
            {
                auto newframe = ctx.frame();
                auto newpc = ctx.get( _VM_CR_PC ).pointer;
                ctx.set( _VM_CR_Frame, frame ); /* :-( */
                ctx.set( _VM_CR_PC, oldpc );
                output = print::instruction( ctx.debug(), eval, 2 );
                ctx.set( _VM_CR_Frame, newframe );
                ctx.set( _VM_CR_PC, newpc );
            }
            ( verb == PrintInstructions ? std::cerr : std::cout ) << indent << output << std::endl;
        }
        else
            eval.dispatch();

        if ( in_fault && !ctx.heap().valid( frame ) )
            rewind_to_fault = false;

        while ( ctx.debug_mode() )
            eval.advance(), eval.dispatch();

        if ( schedule( ctx ) )
            state();

        if ( !ff )
            in_frame( ctx.frame(), ctx.heap() );

        if ( _sched_policy )
            _sched_policy();

        if ( verb != Quiet )
        {
            for ( auto t : ctx._trace )
                std::cerr << "T: " << t << std::endl;
            ctx._trace.clear();
        }
    }
    ctx.sync_pc();
}

template struct Stepper< dbg::Context< CowHeap > >;
template struct Stepper< DbgRunContext >;

}
