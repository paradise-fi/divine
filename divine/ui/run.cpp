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
#include <divine/ui/cli.hpp>
#include <divine/vm/setup.hpp>
#include <divine/dbg/stepper.hpp>

namespace divine::ui
{

void Run::run()
{
    if ( _trace )
        trace();
    else
        mc::Exec( bitcode() ).run();
}

void Run::trace()
{
    using Stepper = dbg::Stepper< mc::TraceContext >;
    Stepper step;
    step._ff_components = dbg::Component::Kernel;
    step._booting = true;

    mc::TraceContext ctx( bitcode()->program(), bitcode()->debug() );
    vm::setup::boot( ctx );

    auto mainpc = bitcode()->program().functionByName( "main" );
    auto startpc = bitcode()->program().functionByName( "_start" );

    step._breakpoint = [mainpc]( vm::CodePointer pc, bool ) { return pc == mainpc; };
    step.run(ctx, Stepper::Verbosity::Quiet);
    step._breakpoint = [startpc]( vm::CodePointer pc, bool )
                       {
                           return pc.function() == startpc.function();
                       };
    step.run(ctx, Stepper::Verbosity::TraceInstructions);
    step._breakpoint = {};
    step.run(ctx, Stepper::Verbosity::Quiet);
}

}
