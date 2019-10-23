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

#include <utility>

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
    bool has_boot = _module->getFunction( "__boot" );

    rt::DiosCC drv( _ctx );
    drv.link( std::move( _module ) );

    if ( !has_boot )
        drv.link_dios_config( _opts.dios_config );
    _module = drv.takeLinked();
}

void BitCode::_save_original_module()
{
    _pure_module = llvm::CloneModule( *_module );
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
}


BitCode::BitCode( std::unique_ptr< llvm::Module > m, std::shared_ptr< llvm::LLVMContext > ctx )
    : _ctx( ctx ), _module( std::move( m ) )
{
    ASSERT( _module.get() );
    _program.reset( new vm::Program( llvm::DataLayout( _module.get() ) ) );
}


void BitCode::do_lart()
{
    _save_original_module();

    lart::Driver lart;

    lart.setup( lart::FixPHI::meta() );

    // User defined passes are run first so they don't break instrumentation
    for ( auto p : _lart )
        lart.setup( p );

    if ( _opts.leakcheck )
        lart.setup( lart::divine::leakCheck(), to_string( _opts.leakcheck ) );

    // reduce before any instrumentation to avoid unnecessary instrumentation
    // and mark silent operations
    if ( !_opts.disable_static_reduction )
    {
        lart.setup( lart::reduction::paroptPass() );
        lart.setup( lart::reduction::staticTauMemPass() );
    }

    if ( _opts.svcomp )
        lart.setup( lart::svcomp::svcompPass() );

    if ( _opts.symbolic )
        lart.setup( lart::abstract::passes() );

    if ( !_opts.synchronous )
    {
        lart.setup( lart::propagateRecursiveAnnotationPass() );
        // note: weakmem also puts memory interrupts in
        lart.setup( lart::divine::cflInterruptPass(), "__dios_suspend" );
        if ( !_opts.sequential && _opts.relaxed.empty() )
            lart.setup( lart::divine::memInterruptPass(), "__dios_reschedule" );
    }

    if ( _opts.autotrace )
        lart.setup( lart::divine::autotracePass(), to_string( _opts.autotrace ) );

    if ( !_opts.relaxed.empty() )
        lart.setup( "weakmem:" + _opts.relaxed, false );

    lart.setup( lart::divine::lowering() );
    // reduce again before metadata are added to possibly tweak some generated
    // code + perform static tau
    if ( !_opts.disable_static_reduction )
        lart.setup( lart::reduction::paroptPass() );

    lart.setup( lart::divine::lsda() );
    auto mod = _module.get();
    if ( mod->getGlobalVariable( "__md_functions" ) || mod->getGlobalVariable( "__md_globals" ) )
        lart.setup( lart::divine::functionMetaPass() );
    if ( mod->getGlobalVariable( "__sys_env" ) )
        lart::util::replaceGlobalArray( *mod, "__sys_env", _opts.bc_env );
    lart.process( mod );

    // TODO brick::llvm::verifyModule( mod );
}

void BitCode::do_rr()
{
    auto mod = _module.get();
    _program.reset( new vm::Program( llvm::DataLayout( mod ) ) );
    _program->setupRR( mod );
    _program->computeRR( mod );
    _dbg.reset( new dbg::Info( *_program.get(), *mod ) );
}

void BitCode::do_constants()
{
    _program->computeStatic( _module.get() );
}

void BitCode::do_dios()
{
    lazy_link_dios();
}

void BitCode::init()
{
    do_dios();
    do_lart();
    do_rr();
    do_constants();
}

BitCode::~BitCode() { }

std::shared_ptr< BitCode > BitCode::with_options( const BCOptions &opts )
{
    auto magic_data = brick::fs::readFile( opts.input_file, 18 );
    auto magic = llvm::identify_magic( magic_data );
    auto bc = [&]
    {
        switch ( magic )
        {
            case llvm::file_magic::bitcode:
            case llvm::file_magic::elf_relocatable:
            case llvm::file_magic::elf_executable:
                return std::make_shared< mc::BitCode >( opts.input_file );

            default:
            {
                if ( cc::typeFromFile( opts.input_file ) == cc::FileType::Unknown )
                    throw std::runtime_error( "cannot create BitCode out of file " + opts.input_file
                                            + " (unknown type)" );
                cc::Options ccopt;
                rt::DiosCC driver( ccopt );
                driver.build( cc::parseOpts( opts.ccopts ) );
                return std::make_shared< mc::BitCode >( driver.takeLinked(), driver.context() );
            }
        }
    }();

    bc->set_options( opts );

    return bc;
}

BCOptions BCOptions::from_report( brick::yaml::Parser &parsed )
{
    auto opts = BCOptions();

    auto env = std::vector< std::pair< std::string, std::string > >();
    parsed.get( { "input options", "*" }, env );
    for ( auto [fst, snd] : env )
        opts.bc_env.emplace_back( fst, std::vector< uint8_t >( snd.begin(), snd.end() ) );

    opts.ccopts = parsed.getOr( { "compile options", "*" }, opts.ccopts );
    opts.lart_passes = parsed.getOr( { "lart passes", "*" }, opts.lart_passes );
    opts.relaxed = parsed.getOr( { "relaxed memory" }, opts.relaxed );
    opts.symbolic = parsed.getOr( { "symbolic" }, opts.symbolic );
    opts.svcomp = parsed.getOr( { "svcomp" }, opts.svcomp );
    opts.sequential = parsed.getOr( { "sequential" }, opts.sequential );
    opts.synchronous = parsed.getOr( { "synchronous" }, opts.synchronous );
    opts.disable_static_reduction = parsed.getOr( { "disable static reduction" },
                                                opts.disable_static_reduction );
    opts.dios_config = "default";
    opts.dios_config = parsed.getOr( { "dios config" }, opts.dios_config );

    auto lf_flags = LeakCheckFlags();
    auto leakcheck = std::vector< std::string >();
    leakcheck = parsed.getOr( { "leak check", "*" }, leakcheck );
    for ( auto s : leakcheck )
        lf_flags |= mc::leakcheck_from_string( s );
    opts.leakcheck = lf_flags;

    opts.input_file = parsed.get< std::string >( { "input file" } );

    return opts;
}

}
