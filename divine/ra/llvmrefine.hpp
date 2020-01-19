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
#pragma once

#include <memory>

#include <bricks/brick-assert>
#include <bricks/brick-llvm>

#include <divine/dbg/setup.hpp>
#include <divine/dbg/stepper.hpp>

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.hpp>

#include <divine/ra/base.hpp>
#include <divine/ra/util.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/divine/indirectionstubber.h>

namespace divine::ra {

struct get_ce_t_
{
    template< typename Ctx >
    void static create_ctx( Ctx &dbg_ctx, mc::Job &job)
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
        dbg_ctx._lock_mode = Ctx::LockBoth;
        vm::setup::scheduler( dbg_ctx );
        using Stepper = dbg::Stepper< Ctx >;
        Stepper step;
        step._stop_on_error = true;
        step._stop_on_accept = true;
        step._ff_components = dbg::component::kernel;
        step.run( dbg_ctx, Stepper::Quiet );

        return std::move( dbg_ctx );
    }
};

template< typename refiner_t >
struct get_ce_t : get_ce_t_ {};

template<>
struct get_ce_t< lart::divine::IndirectCallsStubber > : get_ce_t_
{
    using refiner_t = lart::divine::IndirectCallsStubber;

    auto static get_ce(refiner_t& stabber,
                       mc::Job &job, mc::BitCode *bc )
    -> std::pair< llvm::Function *, llvm::Function *>
    {

        dbg::Info info( *bc->_program, *bc->_module );

        dbg::Context< vm::CowHeap > dbg( bc->program(), bc->debug() );
        //InitializeContext( dbg, job );
        get_ce_t_::create_ctx( dbg, job );
        auto &heap = dbg.heap();

        vm::PointerV current_pc;
        vm::PointerV current_parent;

        // TODO: Check that it is not an error in the program but truly in our
        // indirection wrapper
        vm::PointerV iter_frame( dbg.frame() );

        while ( !iter_frame.cooked().null() ) {
            heap.read_shift( iter_frame, current_pc );

            auto where = info.function( current_pc.cooked() );

            heap.read_shift( iter_frame, current_parent );

            if ( stabber.isWrapper( where ) ) {

                vm::PointerV first_arg;
                heap.read_shift( iter_frame, first_arg );

                return { where, info.function( first_arg.cooked() ) };
            }

            iter_frame = current_parent;

        }

        return { nullptr, nullptr };
    }
};

// TODO: Maybe let refiner_t be mixin?
template< typename refiner_t >
struct llvm_refinement : refinement_t
{
    using BCOptions = refinement_t::BCOptions;

    refiner_t _refiner;

    llvm_refinement( const BCOptions &bc_opts )
        : refinement_t(bc_opts), _refiner( *_m )
    {
        _refiner.init();
    }

    bool iterate( uint64_t n )
    {
        if ( n == 0 ) return false;
        if ( n == 1 ) return iterate();
        iterate();
        return iterate( n - 1 );
    }

    void finish()
    {
        while ( !iterate() ) {}
    }

    bool iterate() {
        auto [ result, bc ] = this->run();
        if ( result->result() == mc::Result::Valid )
            return true;
        _refiner.enhance( get_ce_t< refiner_t >::get_ce( _refiner, *result, bc.get() ) ) ;
        return false;
    }
};

using indirect_calls_refinement_t = llvm_refinement< lart::divine::IndirectCallsStubber >;

} // namespace divine::ra
