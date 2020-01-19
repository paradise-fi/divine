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

#include <divine/dbg/setup.hpp>
#include <divine/dbg/stepper.hpp>

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.tpp>
#include <divine/mc/safety.hpp>

#include <divine/ra/util.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::ra {

// Preset minimal attributes needed to be passed to divine::mc::make_job
struct _BitCode : divine::mc::BitCode
{
    _BitCode( std::unique_ptr< llvm::Module > m,
            std::shared_ptr< llvm::LLVMContext > ctx,
            const divine::mc::BCOptions &opts)
    : BitCode( std::move( m ), ctx )
    {
        this->set_options( opts );
        this->init();
    }
};

struct refinement_t
{
    using BCOptions = divine::mc::BCOptions;

    BCOptions _bc_opts;

    // Order is important because of dtors
    std::shared_ptr< llvm::LLVMContext > _ctx = std::make_shared< llvm::LLVMContext >();
    std::unique_ptr< llvm::Module > _m;

    refinement_t( const BCOptions &bc_opts ) : _bc_opts( bc_opts )
    {
        if ( _bc_opts.input_file.name.empty() )
            UNREACHABLE( "Invalid options passed to refinement_t input_file is missing" );
        _m = util::load_bc( _bc_opts.input_file.name, &*_ctx );
    }

    auto make_bc() {
        return std::make_shared< _BitCode >( llvm::CloneModule( *_m ), _ctx, _bc_opts );
    }

    template< template<typename, typename> class job_t = divine::mc::Safety >
    auto run() {
        auto bc = make_bc();
        auto safe = mc::make_job< job_t >( bc, ss::passive_listen() );
        // FIXME: thread_count
        safe->start( 1 );
        safe->wait();
        return std::pair( std::move( safe ), std::move( bc ) );
    }

};

} // namespace divine::ra
