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

#include <divine/mc/exec.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/vm/eval.hpp>
#include <divine/vm/setup.hpp>

#include <divine/vm/eval.tpp>
#include <divine/vm/context.tpp>
#include <divine/dbg/stepper.tpp>
#include <divine/dbg/print.tpp>

namespace divine::mc
{

    void Exec::run()
    {
        using Eval = vm::Eval< ExecContext >;
        auto &program = _bc->program();
        ExecContext _ctx( program );
        Eval eval( _ctx );

        vm::setup::boot( _ctx );
        _ctx.enable_debug();
        eval.run();
        if ( !_ctx.flags_any( _VM_CF_Cancel | _VM_CF_Error ) )
            ASSERT( !_ctx.state_ptr().null() );

        while ( !_ctx.flags_any( _VM_CF_Cancel | _VM_CF_Error ) )
        {
            vm::setup::scheduler( _ctx );
            eval.run();
        }
    }

    void Exec::trace()
    {
        using Stepper = dbg::Stepper< mc::TraceContext >;
        Stepper step;
        step._ff_components = dbg::Component::Kernel;
        step._booting = true;

        mc::TraceContext ctx( _bc->program(), _bc->debug() );
        vm::setup::boot( ctx );
        step.run( ctx, Stepper::Verbosity::TraceInstructions );
    }

}

namespace divine::dbg        { template struct Stepper< mc::TraceContext >; }
namespace divine::dbg::print { template struct Print< mc::TraceContext >;   }
