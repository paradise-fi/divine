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

BitCode::BitCode( std::string file, BitCode::Env env, AutoTraceFlags tr, bool verbose, bool reduce,
                  std::vector< std::string > lartPasses )
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
    init( env, tr, verbose, reduce, lartPasses );
}


BitCode::BitCode( std::unique_ptr< llvm::Module > m, std::shared_ptr< llvm::LLVMContext > ctx,
                  BitCode::Env env, AutoTraceFlags tr, bool verbose, bool reduce,
                  std::vector< std::string > lartPasses )
    : _ctx( ctx ), _module( std::move( m ) )
{
    ASSERT( _module.get() );
    _program.reset( new Program( _module.get() ) );
    init( env, tr, verbose, reduce, lartPasses );
}


void BitCode::init( BitCode::Env env, AutoTraceFlags tr, bool verbose, bool reduce,
                    std::vector< std::string > lartPasses )
{
    lart::Driver lart;

    // User defined passes are run first so they don't break instrumentation
    for ( auto p : lartPasses )
        lart.setup( p );

    // reduce before any instrumentation to avoid unnecessary instrumentation
    if ( reduce )
        lart.setup( lart::reduction::paroptPass() );
    lart.setup( lart::divine::interruptPass() );
    if ( tr )
        lart.setup( lart::divine::autotracePass() );
    lart.setup( lart::divine::lowerExceptionHandling() );
    // reduce again before metadata are added to possibly tweak some generated
    // code + perform static tau
    if ( reduce ) {
        lart.setup( lart::reduction::paroptPass() );
        lart.setup( lart::reduction::staticTauMemPass() );
    }

    auto mod = _module.get();
    if ( mod->getGlobalVariable( "__md_functions" ) && mod->getGlobalVariable( "__md_globals" ) )
        lart.setup( lart::divine::functionMetaPass() );
    if ( mod->getGlobalVariable( "__sys_env" ) )
        lart::util::replaceGlobalArray( *mod, "__sys_env", env );
    if ( verbose )
        std::cerr << "annotating bitcode..." << std::flush;
    lart.process( mod );
    if ( verbose )
        std::cerr << " done" << std::endl;

    _program.reset( new Program( mod ) );

    if ( verbose )
        std::cerr << "computing RR..." << std::flush;
    _program->setupRR();
    _program->computeRR();
    if ( verbose )
        std::cerr << " constants..." << std::flush;
    _program->computeStatic();
    if ( verbose )
        std::cerr << " done" << std::endl;
}

BitCode::~BitCode() { }
