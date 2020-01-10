// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Petr Ročkai <code@fixp.eu>
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
#include <brick-types>
#include <brick-except>
#include <brick-cmd>
#include <brick-yaml>

#include <memory>
#include <vector>
#include <optional>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/dbg/info.hpp>
#include <divine/rt/dios-cc.hpp>

namespace llvm { class Module; }
namespace divine::vm { struct Program; }

namespace divine::mc
{
    enum class autotrace { nothing, calls = 1, allocs = 2 };
    enum class leakcheck { nothing, exit = 0x1 , ret = 0x2 , state = 0x4 };

    struct tracepoint : brick::types::StrongEnumFlags< autotrace >
    {
        static auto help() { return "a comma-separated list of: nothing, calls or allocs"; }
    };

    struct checkpoint : brick::types::StrongEnumFlags< leakcheck >
    {
        static auto help() { return "a comma-separated list of: exit, ret or state"; }
    };

    std::string to_string( checkpoint, bool yaml = false );
    std::string to_string( tracepoint, bool yaml = false );
    brq::parse_result from_string( std::string_view s, leakcheck &f );
    brq::parse_result from_string( std::string_view s, autotrace &f );

struct BCParseError : brq::error { using brq::error::error; };

struct BCOptions
{
    using Env = std::vector< std::tuple< std::string, std::vector< uint8_t > > >;

    brq::cmd_file input_file;
    std::vector< std::string > ccopts;

    brq::cmd_flag static_reduction = true, symbolic, sequential, synchronous,
                                     svcomp, mcsema;

    Env bc_env;
    std::vector< std::string > lart_passes;
    std::string dios_config;
    tracepoint autotrace;
    checkpoint leakcheck;
    std::string relaxed;

    static BCOptions from_report( brick::yaml::Parser &parsed );
};

struct BitCode
{
    std::shared_ptr< llvm::LLVMContext > _ctx; // the order of these members is important as
    std::unique_ptr< llvm::Module > _module;   // _program depends on _module which depends on _ctx
    std::unique_ptr< vm::Program > _program;       // and they have to be destroyed in the right order
                                               // otherwise DIVINE will SEGV if exception is thrown
    std::unique_ptr< llvm::Module > _pure_module; // pre-LART version
    std::unique_ptr< dbg::Info > _dbg;

    std::string _solver;
    BCOptions _opts;

    bool is_symbolic() const { return _opts.symbolic; }
    std::string solver() const { ASSERT( is_symbolic() ); return _solver; }

    vm::Program &program() { ASSERT( _program.get() ); return *_program.get(); }
    dbg::Info &debug() { ASSERT( _dbg.get() ); return *_dbg.get(); }

    BitCode( std::string file );
    BitCode( std::unique_ptr< llvm::Module > m,
             std::shared_ptr< llvm::LLVMContext > ctx = nullptr );

    void set_options( const BCOptions& opts ) { _opts = opts; }
    void solver( std::string s ) { _solver = s; }

    void do_lart();
    void do_dios();
    void do_rr();
    void do_constants();

    void init();

    // TODO: Disables move synthesis, probably should be removed
    ~BitCode();

    static std::shared_ptr< BitCode > with_options( const BCOptions &opts, rt::DiosCC &cc_driver );

private:
    void lazy_link_dios();
    void _save_original_module();
};

}
