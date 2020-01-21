/*
 * (c) 2020 Lukáš Korenčik  <xkorenc1@fi.muni.cz>
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
#include <divine/ra/llvmrefine.hpp>

namespace divine::ra {

void ce_t::_create_ctx( dbg_ctx_t &dbg_ctx, mc::Job &job )
{
    auto trace = job.ce_trace();

    if ( job.result() == mc::Result::BootError ) {
        dbg::setup::boot( dbg_ctx );
        trace.bootinfo = dbg_ctx._info;
        trace.labels = dbg_ctx._trace;
    }

    job.dbg_fill( dbg_ctx );
    dbg_ctx.load( trace.final );

    dbg_ctx._lock = trace.steps.back();
    dbg_ctx._lock_mode = dbg_ctx_t::LockBoth;
    vm::setup::scheduler( dbg_ctx );
    using Stepper = dbg::Stepper< dbg_ctx_t >;
    Stepper step;
    step._stop_on_error = true;
    step._stop_on_accept = true;
    step._ff_components = dbg::component::kernel;
    step.run( dbg_ctx, Stepper::Quiet );
}

auto ce_t::stack_trace() -> stack_trace_t
{
    stack_trace_t out;
    auto gather = [ & ]( auto frame, auto &heap, auto &info ) {
        vm::PointerV current_pc;
        heap.read_shift( frame, current_pc );
        auto [ inst, pc ] = info.find( nullptr, current_pc.cooked() );
        out.push_back( inst );
    };
    stack_trace( gather );
    return out;
}

void remove_indirect_calls::enhance( ce_t &counter_example )
{
    std::optional< std::pair< llvm::Function *, llvm::Function * > > target;
    auto gather = [ & ]( auto frame, auto &heap, auto &info )
    {
        if ( target ) return;

        vm::PointerV current_pc;
        heap.read_shift( frame, current_pc );
        auto where = info.function( current_pc.cooked() );

        if ( !llvm_pass.is_wrapper( where ) ) return;

        vm::PointerV first_arg;

        // Get rid of the parent frame first
        heap.read_shift( frame, first_arg );
        heap.read_shift( frame, first_arg );
        target = { where, info.function( first_arg.cooked() ) };
    };

    counter_example.stack_trace( gather );
    ASSERT( target );

    auto [ where, callee ] = *target;
    llvm_pass.enhance( where, callee );
}

} // namespace divine::ra
