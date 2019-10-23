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

#include <brick-yaml>

#include <memory>
#include <vector>
#include <optional>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/dbg/info.hpp>

namespace llvm { class Module; }
namespace divine::vm { struct Program; }

namespace divine::mc
{

enum class AutoTrace { Nothing, Calls = 1, Allocs = 2 };
enum class LeakCheck { Nothing, Exit = 0x1 , Return = 0x2 , State = 0x4 };
using AutoTraceFlags = brick::types::StrongEnumFlags< AutoTrace >;
using LeakCheckFlags = brick::types::StrongEnumFlags< LeakCheck >;

std::string to_string( LeakCheckFlags, bool yaml = false );
std::string to_string( AutoTraceFlags, bool yaml = false );
LeakCheckFlags leakcheck_from_string( std::string x );
AutoTraceFlags autotrace_from_string( std::string x );

struct BCParseError : brq::error { using brq::error::error; };

struct BCOptions
{
    using Env = std::vector< std::tuple< std::string, std::vector< uint8_t > > >;

    std::string input_file;
    std::vector< std::string > ccopts;

    bool disable_static_reduction = false;
    bool symbolic = false;
    bool sequential = false;
    bool synchronous = false; // !interrupts
    bool svcomp = false;

    Env bc_env;
    std::vector< std::string > lart_passes;
    std::string dios_config;
    mc::AutoTraceFlags autotrace;
    mc::LeakCheckFlags leakcheck;
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

    static std::shared_ptr< BitCode > with_options( const BCOptions &opts );

private:
    void lazy_link_dios();
    void _save_original_module();
};

}

namespace divine::t_vm
{

static auto c2bc( std::string s )
{
    static std::shared_ptr< llvm::LLVMContext > ctx( new llvm::LLVMContext );
    divine::cc::CC1 c( ctx );
    c.mapVirtualFile( "/main.c", s );
    auto rv = std::make_shared< mc::BitCode >( c.compile( "/main.c" ), ctx );
    rv->_interrupts = false;
    rv->init();
    return rv;
}

}
