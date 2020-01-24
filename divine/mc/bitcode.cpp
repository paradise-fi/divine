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
#include <lart/mcsema/libcindirectcalls.hpp>
#include <lart/mcsema/segmentmasks.hpp>
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

std::string to_string( checkpoint lf, bool yaml )
{
    std::string out;
    auto append = [&]( auto str )
    {
        if ( !out.empty() ) out += yaml ? ", " : ",";
        if ( yaml ) out += '"';
        out += str;
        if ( yaml ) out += '"';
    };

    if ( lf & leakcheck::ret )
        append( "return" );
    if ( lf & leakcheck::exit )
        append( "exit" );
    if ( lf & leakcheck::state )
        append( "state" );

    if ( out.empty() )
        return "none";
    else
        return out;
}

std::string to_string( tracepoint lf, bool yaml )
{
    std::string out;
    auto append = [&]( auto str )
    {
        if ( !out.empty() ) out += yaml ? ", " : ",";
        if ( yaml ) out += '"';
        out += str;
        if ( yaml ) out += '"';
    };

    if ( lf & autotrace::calls )
        append( "calls" );
    if ( lf & autotrace::allocs )
        append( "allocs" );

    if ( out.empty() )
        return "none";
    else
        return out;
}

brq::parse_result from_string( std::string_view x, leakcheck &lc )
{
    if      ( x == "return" ) lc = leakcheck::ret;
    else if ( x == "state" )  lc = leakcheck::state;
    else if ( x == "exit" )   lc = leakcheck::exit;
    else if ( x == "none" )   lc = leakcheck::nothing;
    else return brq::no_parse( "expected 'return', 'state' or 'exit'" );

    return {};
}

brq::parse_result from_string( std::string_view x, autotrace &fl )
{
    if      ( x == "calls" )  fl = autotrace::calls;
    else if ( x == "allocs" ) fl = autotrace::allocs;
    else if ( x == "none" )   fl = autotrace::nothing;
    else return brq::no_parse( "expected 'calls' or 'allocs'" );

    return {};
}

void BitCode::lazy_link_dios()
{
    bool has_boot = _module->getFunction( "__boot" );

    rt::DiosCC drv( _ctx );
    drv.link( std::move( _module ) );

    if ( !has_boot )
        drv.link_dios_config( _opts.dios_config, _opts.lamp_config );
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

    // TODO: Unify with lart::Driver once it is rewritten
    if ( _opts.mcsema )
    {
        lart::mcsema::segment_masks().run( *_module.get() );
        lart::mcsema::libc_indirect_calls().run( *_module.get() );
    }

    lart::Driver lart;

    lart.setup( lart::FixPHI::meta() );

    // User defined passes are run first so they don't break instrumentation

    // Libc included in dios expects that fuseCtorsPass inserts some functions
    lart.setup( lart::divine::fuseCtorsPass() );

    for ( auto p : _opts.lart_passes )
        lart.setup( p );

    if ( _opts.leakcheck )
        lart.setup( lart::divine::leakCheck(), to_string( _opts.leakcheck ) );

    // reduce before any instrumentation to avoid unnecessary instrumentation
    // and mark silent operations
    if ( _opts.static_reduction )
    {
        lart.setup( lart::reduction::paroptPass() );
        lart.setup( lart::reduction::staticTauMemPass() );
    }

    if ( _opts.svcomp )
        lart.setup( lart::svcomp::svcompPass() );

    if ( _opts.symbolic || !_opts.lamp_config.empty() )
        lart.setup( lart::abstract::passes() );

    if ( !_opts.synchronous )
    {
        lart.setup( lart::divine::lowering() );
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
    if ( _opts.static_reduction )
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

std::shared_ptr< BitCode > BitCode::with_options( const BCOptions &opts, rt::DiosCC &cc_driver )
{
    auto magic_buf = cc_driver.compiler.getFileBuffer( opts.input_file.name, 18 );
    auto magic_data = magic_buf ? std::string( magic_buf->getBuffer() )
                                : brq::read_file( opts.input_file.name, 18 );
    auto magic = llvm::identify_magic( magic_data );

    auto bc = [&]
    {
        switch ( magic )
        {
            case llvm::file_magic::bitcode:
            case llvm::file_magic::elf_relocatable:
            case llvm::file_magic::elf_executable:
                return std::make_shared< mc::BitCode >( opts.input_file.name );

            default:
            {
                if ( cc::typeFromFile( opts.input_file.name ) == cc::FileType::Unknown )
                    throw std::runtime_error( "don't know how to verify file " + opts.input_file.name
                                            + " (unknown type)" );
                cc_driver.build( cc::parseOpts( opts.ccopts ) );
                return std::make_shared< mc::BitCode >( cc_driver.takeLinked(), cc_driver.context() );
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
    opts.static_reduction = parsed.getOr( { "static reduction" }, opts.static_reduction );
    opts.dios_config = "default";
    opts.dios_config = parsed.getOr( { "dios config" }, opts.dios_config );
    opts.lamp_config = parsed.getOr( { "lamp config" }, opts.lamp_config );

    auto leakcheck = std::vector< std::string >();
    leakcheck = parsed.getOr( { "leak check", "*" }, leakcheck );
    for ( auto s : leakcheck )
        from_string( s, opts.leakcheck );

    opts.input_file = parsed.get< brq::cmd_file >( { "input file" } );

    return opts;
}

}
