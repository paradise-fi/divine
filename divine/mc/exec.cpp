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

#include <divine/mc/ctx-assume.hpp>
#include <divine/mc/exec.hpp>
#include <divine/mc/search.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/mc/machine.hpp>
#include <divine/mc/weaver.hpp>

#include <divine/vm/eval.hpp>
#include <divine/vm/setup.hpp>

#include <divine/vm/eval.tpp>
#include <divine/dbg/stepper.tpp>
#include <divine/dbg/print.tpp>

namespace divine::mc
{
    template< typename next >
    struct print_trace : next
    {
        using next::trace;
        void trace( std::string s ) { std::cout << s << std::endl; }
    };

    namespace ctx = vm::ctx;
    struct ctx_exec : brq::compose_stack< ctx_assume, ctx_choice, brq::module< print_trace >,
                                          ctx::with_debug, ctx::with_tracking,
                                          ctx::common< vm::Program, vm::CowHeap > > {};

    struct mach_exec : brq::compose_stack< machine::graph_dispatch,
                                           machine::compute,
                                           machine::tree_search,
                                           machine::choose_random, machine::choose,
                                           machine::with_context< ctx_exec >,
                                           machine::base< smt::NoSolver > > {};

    struct search : mc::Search< mc::State, mc::Label > {};

    void Exec::run()
    {
        mach_exec m;
        m.bc( _bc );
        m.context().enable_debug();
        weave( m ).extend( search() ).start();
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
