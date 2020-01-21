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

#include <lart/divine/rewirecalls.h>

namespace divine::ra {

struct ce_t
{
    using stack_trace_t = std::vector< llvm::Instruction * >;
    using dbg_ctx_t = dbg::Context< vm::CowHeap >;

    dbg::Info info;
    dbg_ctx_t dbg_ctx;

    ce_t( mc::Job &job, mc::BitCode *bc )
        : info( *bc->_program, *bc->_module ),
          dbg_ctx( bc->program(), bc->debug() )
    {
        _create_ctx( dbg_ctx, job );
    }

    void _create_ctx( dbg_ctx_t &dbg_ctx, mc::Job &job );

    stack_trace_t stack_trace();

    template< typename Yield >
    void stack_trace( Yield &&yield )
    {
        auto &heap = dbg_ctx.heap();


        vm::PointerV iter_frame( dbg_ctx.frame() );
        vm::PointerV parent;

        while ( !iter_frame.cooked().null() )
        {
            yield( iter_frame, heap, info );
            // First entry of frame is program counter
            heap.read_shift( iter_frame, parent );
            heap.read_shift( iter_frame, parent );
            iter_frame = parent;
        }
    }

    template< typename Stream >
    static Stream &trace( Stream &os, const stack_trace_t& stack_trace )
    {
        os << "Stack trace:" << std::endl;
        for ( auto frame : stack_trace )
            os << "\t" << ( frame ? frame->getFunction()->getName().str() : "nullptr" )
               << std::endl;
        return os;
    }
};

// TODO: Maybe let refiner_t be mixin?
template< typename refiner_t >
struct llvm_refinement : refinement_t
{
    using BCOptions = refinement_t::BCOptions;

    refiner_t _refiner;

    llvm_refinement( std::shared_ptr< llvm::LLVMContext > ctx,
                     std::unique_ptr< llvm::Module > m,
                     const BCOptions &bc_opts )
        : refinement_t( std::move( ctx ), std::move( m ), bc_opts ),
         _refiner( *_m )
    {}

    llvm_refinement( const BCOptions &bc_opts )
        : refinement_t( bc_opts ), _refiner( *_m )
    {}

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
        ce_t ce( *result, bc.get() );
        _refiner.enhance( ce );
        return false;
    }

    std::string report() { _refiner.report(); }
};

struct remove_indirect_calls
{
    lart::divine::rewire_calls_t llvm_pass;

    remove_indirect_calls( llvm::Module &m ) : llvm_pass( m )
    {
        llvm_pass.init();
    }

    void enhance( ce_t &counter_example );

    std::string report() { return llvm_pass.report(); }
};

using indirect_calls_refinement_t = llvm_refinement< remove_indirect_calls >;

} // namespace divine::ra
