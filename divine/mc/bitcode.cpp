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

#include <divine/rt/dios-cc.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/program.hpp>
#include <lart/driver.h>
#include <lart/support/util.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/Object/IRObjectFile.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>

namespace divine::mc
{

std::string to_string( LeakCheckFlags lf, bool yaml )
{
    std::string out;
    auto append = [&]( auto str )
    {
        if ( !out.empty() ) out += yaml ? ", " : ",";
        if ( yaml ) out += '"';
        out += str;
        if ( yaml ) out += '"';
    };

    if ( lf & LeakCheck::Return )
        append( "return" );
    if ( lf & LeakCheck::Exit )
        append( "exit" );
    if ( lf & LeakCheck::State )
        append( "state" );

    if ( out.empty() )
        return "none";
    else
        return out;
}

std::string to_string( AutoTraceFlags lf, bool yaml )
{
    std::string out;
    auto append = [&]( auto str )
    {
        if ( !out.empty() ) out += yaml ? ", " : ",";
        if ( yaml ) out += '"';
        out += str;
        if ( yaml ) out += '"';
    };

    if ( lf & AutoTrace::Calls )
        append( "calls" );
    if ( lf & AutoTrace::Allocs )
        append( "allocs" );

    if ( out.empty() )
        return "none";
    else
        return out;
}

LeakCheckFlags leakcheck_from_string( std::string x )
{
    if ( x == "return" )
        return mc::LeakCheck::Return;
    if ( x == "state" )
        return mc::LeakCheck::State;
    if ( x == "exit" )
        return mc::LeakCheck::Exit;

    return mc::LeakCheck::Nothing;
}

AutoTraceFlags autotrace_from_string( std::string x )
{
    if ( x == "calls" )
        return mc::AutoTrace::Calls;
    if ( x == "allocs" )
        return mc::AutoTrace::Allocs;

    return mc::AutoTrace::Nothing;
}

void BitCode::lazy_link_dios()
{
    if( !(_module->getFunction( "__boot" )) )
    {
        rt::DiosCC drv( _ctx );
        drv.link( std::move( _module ) );
        drv.linkDios();
        drv.linkLibs( rt::DiosCC::defaultDIVINELibs );
        _module = drv.takeLinked();
    }
}

BitCode::BitCode( std::string file )
{
    _ctx.reset( new llvm::LLVMContext() );
    std::unique_ptr< llvm::MemoryBuffer > input;

    using namespace llvm::object;

    input = std::move( llvm::MemoryBuffer::getFile( file ).get() );
    auto bc = IRObjectFile::findBitcodeInMemBuffer( input->getMemBufferRef() );

    if ( !bc )
        std::cerr << toString( bc.takeError() ) << std::endl;
    auto parsed = llvm::parseBitcodeFile( bc.get(), *_ctx );
    if ( !parsed )
        throw BCParseError( "Error parsing input model; probably not a valid bitcode file." );
    _module = std::move( parsed.get() );
    lazy_link_dios();
}


BitCode::BitCode( std::unique_ptr< llvm::Module > m, std::shared_ptr< llvm::LLVMContext > ctx )
    : _ctx( ctx ), _module( std::move( m ) )
{
    ASSERT( _module.get() );
    lazy_link_dios();
    _program.reset( new vm::Program( llvm::DataLayout( _module.get() ) ) );
}


void BitCode::do_lart()
{
    lart::Driver lart;

    lart.setup( lart::FixPHI::meta() );

    // User defined passes are run first so they don't break instrumentation
    for ( auto p : _lart )
        lart.setup( p );

    if ( _leakcheck )
        lart.setup( lart::divine::leakCheck(), to_string( _leakcheck ) );

    // reduce before any instrumentation to avoid unnecessary instrumentation
    // and mark silent operations
    if ( _reduce )
    {
        lart.setup( lart::reduction::paroptPass() );
        lart.setup( lart::reduction::staticTauMemPass() );
    }

    if ( _svcomp )
        lart.setup( lart::svcomp::svcompPass() );

    if ( _symbolic )
        lart.setup( lart::abstract::passes() );

    if ( _interrupts )
    {
        // note: weakmem also puts memory interrupts in
        lart.setup( lart::divine::cflInterruptPass(), "__dios_suspend" );
        if ( !_sequential && _relaxed.empty() )
            lart.setup( lart::divine::memInterruptPass(), "__dios_reschedule" );
    }

    if ( _autotrace )
        lart.setup( lart::divine::autotracePass(), to_string( _autotrace ) );

    if ( !_relaxed.empty() )
        lart.setup( "weakmem:" + _relaxed, false );

    lart.setup( lart::divine::lowering() );
    // reduce again before metadata are added to possibly tweak some generated
    // code + perform static tau
    if ( _reduce )
        lart.setup( lart::reduction::paroptPass() );

    lart.setup( lart::divine::lsda() );
    auto mod = _module.get();
    if ( mod->getGlobalVariable( "__md_functions" ) && mod->getGlobalVariable( "__md_globals" ) )
        lart.setup( lart::divine::functionMetaPass() );
    if ( mod->getGlobalVariable( "__sys_env" ) )
        lart::util::replaceGlobalArray( *mod, "__sys_env", _env );
    lart.process( mod );

    // TODO brick::llvm::verifyModule( mod );
}

void BitCode::do_rr()
{
    auto mod = _module.get();
    _program.reset( new vm::Program( llvm::DataLayout( mod ) ) );
    _program->setupRR( mod );
    _program->computeRR( mod );
    _dbg.reset( new dbg::Info( *_program.get() ) );
}

void BitCode::do_constants()
{
    _program->computeStatic( _module.get() );
}

void BitCode::init()
{
    do_lart();
    do_rr();
    do_constants();
}

BitCode::~BitCode() { }

}
