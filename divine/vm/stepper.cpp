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

#include <divine/vm/stepper.hpp>
#include <divine/vm/debug.hpp>
#include <divine/vm/print.hpp>
#include <divine/vm/eval.hpp>

namespace divine {
namespace vm {

template< typename Context >
void Stepper< Context >::run( Context &ctx, YieldState yield, SchedPolicy sched_policy, Verbosity verb )
{
    Eval< typename Context::Program, Context, value::Void > eval( ctx.program(), ctx );
    bool in_fault = !_stop_on_fault ||
                    eval.pc().function() == ctx.get( _VM_CR_FaultHandler ).pointer.object();
    bool in_kernel = ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;
    bool error_set = !_stop_on_error || ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error;
    bool moved = false;

    while ( !_sigint && !ctx.frame().null() &&
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

        if ( verb == PrintInstructions && ( !in_kernel || !_ff_kernel ) )
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

        if ( verb != Quiet )
            for ( auto t : ctx._trace )
                std::cerr << "T: " << t << std::endl;
        ctx._trace.clear();
    }
    ctx.sync_pc();
}

template struct Stepper< DebugContext< Program, CowHeap > >;

}
}
