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

#include <divine/vm/bitcode.hpp>
#include <divine/vm/program.hpp>
#include <lart/driver.h>
#include <lart/support/util.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DiagnosticPrinter.h>
DIVINE_UNRELAX_WARNINGS

using namespace divine::vm;

BitCode::BitCode( std::string file, BitCode::Env env, AutoTraceFlags tr )
{
    _ctx.reset( new llvm::LLVMContext() );
    std::unique_ptr< llvm::MemoryBuffer > input;
    input = std::move( llvm::MemoryBuffer::getFile( file ).get() );
    auto error = []( auto &info ) {
        std::string err;
        llvm::raw_string_ostream ostr( err );
        llvm::DiagnosticPrinterRawOStream print( ostr );
        info.print( print );
        throw BCParseError( ostr.str() );
    };
    auto parsed = llvm::parseBitcodeFile( input->getMemBufferRef(), *_ctx, error );
    if ( !parsed )
        throw BCParseError( "Error parsing input model; probably not a valid bitcode file." );
    _module = std::move( parsed.get() );
    init( env, tr );
}


BitCode::BitCode( std::unique_ptr< llvm::Module > m, std::shared_ptr< llvm::LLVMContext > ctx,
                  BitCode::Env env, AutoTraceFlags tr )
    : _module( std::move( m ) ), _ctx( ctx )
{
    ASSERT( _module.get() );
    _program.reset( new Program( _module.get() ) );
    init( env, tr );
}


void BitCode::init( BitCode::Env env, AutoTraceFlags tr )
{
    lart::Driver lart;
    lart.setup( "interrupt" );
    if ( tr )
        lart.setup( "autotrace" );

    auto mod = _module.get();
    if ( mod->getGlobalVariable( "__md_functions" ) )
        lart.setup( "functionmeta" );
    if ( mod->getGlobalVariable( "__sys_env" ) )
        lart::util::replaceGlobalArray( *mod, "__sys_env", env );
    lart.process( mod );

    _program.reset( new Program( mod ) );
}

BitCode::~BitCode()
{
    /* ordering is important */
    _program.reset();
    _module.reset();
}
