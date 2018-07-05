#include <divine/cc/cc1.hpp>
#include <divine/cc/driver.hpp>
#include <divine/rt/runtime.hpp>
#include <divine/vm/xg-code.hpp>

DIVINE_RELAX_WARNINGS
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Object/IRObjectFile.h"
#include "llvm-c/Target.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>
#include <brick-string>
#include <brick-proc>

static const std::string bcsec = ".llvmbc";

using namespace divine;
using namespace llvm;


void addSection( std::string filepath, std::string sectionName, const std::string &sectionData )
{
    brick::fs::TempDir workdir( ".divine.addSection.XXXXXX", brick::fs::AutoDelete::Yes,
                                brick::fs::UseSystemTemp::Yes );
    auto secpath = brick::fs::joinPath( workdir, "sec" );
    std::ofstream secf( secpath, std::ios_base::out | std::ios_base::binary );
    secf << sectionData;
    secf.close();

    auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, "objcopy",
                                  "--remove-section", sectionName, // objcopy can't override section
                                  "--add-section", sectionName + "=" +  secpath,
                                  "--set-section-flags", sectionName + "=noload,readonly",
                                  filepath );
    if ( !r )
        throw cc::CompileError( "could not add section " + sectionName + " to " + filepath
                        + ", objcopy exited with " + to_string( r ) );
}

struct PM_BC : legacy::PassManager
{
    MCStreamer* mc = nullptr;

    void add( Pass *P ) override
    {
        legacy::PassManager::add( P );

        if( auto printer = dynamic_cast< AsmPrinter* >( P ) )
            mc = printer->OutStreamer.get();
    }
};

int emitObjFile( Module &m, std::string filename )
{
    //auto TargetTriple = sys::getDefaultTargetTriple();
    auto TargetTriple = "x86_64-unknown-none-elf";

    divine::cc::initTargets();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget( TargetTriple, Error );

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if ( !Target )
    {
        errs() << Error;
        return 1;
    }

    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto RM = Reloc::Model();
    auto TargetMachine = Target->createTargetMachine( TargetTriple, CPU, Features, opt, RM );

    m.setDataLayout( TargetMachine->createDataLayout() );
    m.setTargetTriple( TargetTriple );

    std::error_code EC;
    raw_fd_ostream dest( filename, EC, sys::fs::F_None );

    if ( EC )
    {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    PM_BC PM;

    if ( TargetMachine->addPassesToEmitFile( PM, dest, TargetMachine::CGFT_ObjectFile, false,
                                             nullptr, nullptr, nullptr, nullptr ) )
    {
        errs() << "TargetMachine can't emit a file of this type\n";
        return 1;
    }

    MCStreamer *AsmStreamer = PM.mc;
    // write bitcode into section .bc
    AsmStreamer->SwitchSection( AsmStreamer->getContext().getELFSection( bcsec, ELF::SHT_NOTE, 0 ) );
    std::string bytes = brick::llvm::getModuleBytes( &m );
    AsmStreamer->EmitBytes( bytes );

    PM.run( m );
    dest.flush();

    return 0;
}

bool isType( std::string file, cc::FileType type )
{
    return cc::typeFromFile( file ) == type;
}

std::unique_ptr< llvm::Module > llvmExtract( std::vector< std::pair< std::string, std::string > >& files, cc::CC1& clang )
{
    using FileType = cc::FileType;
    using namespace brick::types;
    std::unique_ptr< cc::DiosDriver > compil = std::unique_ptr< cc::DiosDriver >( new cc::DiosDriver( clang.context() ) );
    compil->setupFS( rt::each );

    for ( auto file : files )
    {
        if ( !isType( file.second, FileType::Obj ) && !isType( file.second, FileType::Archive ) )
            continue;

        ErrorOr< std::unique_ptr< MemoryBuffer > > buf = MemoryBuffer::getFile( file.second );
        if ( !buf ) throw cc::CompileError( "Error parsing file " + file.second + " into MemoryBuffer" );

        if ( isType( file.second, FileType::Archive ) )
        {
            compil->linkArchive( std::move( buf.get() ) , clang.context() );
            continue;
        }

        auto bc = llvm::object::IRObjectFile::findBitcodeInMemBuffer( (*buf.get()).getMemBufferRef() );
        if ( !bc ) std::cerr << "No .llvmbc section found in file " << file.second << "." << std::endl;

        std::unique_ptr< llvm::Module > m = std::move( llvm::parseBitcodeFile( bc.get(), *clang.context().get()).get() );
        m->setTargetTriple( "x86_64-unknown-none-elf" );
        verifyModule( *m );
        compil->linkModule( std::move( m ) );
    }

    compil->linkEssentials();
    compil->linkLibs( cc::DiosDriver::defaultDIVINELibs );

    auto m = compil->takeLinked();

    for( auto& func : *m )
        if( func.isDeclaration() && vm::xg::hypercall( &func ) == vm::lx::NotHypercall )
            throw cc::CompileError( "Symbol undefined (function): " + func.getName().str() );

    for ( auto& val : m->globals() )
        if( auto G = dyn_cast< llvm::GlobalVariable >( &val ) )
            if( !G->hasInitializer() && !brick::string::startsWith( val.getName(), "__md_" ) )
                throw cc::CompileError( "Symbol undefined (global variable): " + val.getName().str() );

    verifyModule( *m );
    return m;
}


/* usage: same as gcc */
int main( int argc, char **argv )
{
    try {
        cc::CC1 clang;
        clang.allowIncludePath( "/" );
        divine::rt::each( [&]( auto path, auto c ) { clang.mapVirtualFile( path, c ); } );
        std::vector< std::pair< std::string, std::string > > objFiles;

        std::vector< std::string > opts;
        std::copy( argv + 1, argv + argc, std::back_inserter( opts ) );
        auto po = cc::parseOpts( opts );

        using brick::fs::joinPath;
        using brick::fs::splitFileName;
        using divine::cc::includeDir;

        po.opts.insert( po.opts.end(), {
                        "-isystem", joinPath( includeDir, "libcxx/include" )
                      , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                      , "-isystem", joinPath( includeDir, "libunwind/include" )
                      , "-isystem", includeDir } );

        if ( po.files.size() > 1 && po.outputFile != ""
             && ( po.preprocessOnly || po.toObjectOnly ) )
        {
            std::cerr << "Cannot specify -o with multiple files" << std::endl;
            return 1;
        }

        using FileType = cc::FileType;
        using namespace brick::types;

        if ( po.preprocessOnly )
        {
            for ( auto srcFile : po.files )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                if ( isType( ifn, FileType::Obj ) || isType( ifn, FileType::Archive ) )
                    continue;
                std::cout << clang.preprocess( ifn, po.opts );
            }
            return 0;
        }

        for ( auto srcFile : po.files )
        {
            if ( srcFile.is< cc::File >() )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                std::string ofn = brick::fs::dropExtension( ifn );
                ofn = splitFileName( ofn ).second;
                if ( po.outputFile != "" && po.toObjectOnly )
                    ofn = po.outputFile;
                else
                {
                    if ( po.outputFile != "" )
                        ofn += ".temp";
                    ofn += ".o";
                }

                if ( isType( ifn, FileType::Obj ) || isType( ifn, FileType::Archive ) )
                    ofn = ifn;
                objFiles.emplace_back( ifn, ofn );
            }
        }

        int ret = 0;

        if ( po.toObjectOnly )
        {
            for ( auto file : objFiles )
            {
                if ( isType( file.first, FileType::Obj ) || isType( file.first, FileType::Archive ) )
                    continue;
                auto mod = clang.compile( file.first, po.opts );
                emitObjFile( *mod, file.second );
            }

            return 0;
        }
        else
        {
            std::string s;
            for ( auto file : objFiles )
            {
                if ( isType( file.first, FileType::Obj ) || isType( file.first, FileType::Archive ) )
                {
                    s += file.first + " ";
                    continue;
                }
                std::string ofn = file.second;
                auto mod = clang.compile( file.first, po.opts );
                emitObjFile( *mod, ofn );
                s += ofn + " ";
            }
            if ( po.outputFile != "" )
                s += " -o " + po.outputFile;
            s.insert( 0, "gcc " );
            s.append( " -static" );
            int gccret = system( s.c_str() );
            ret = WEXITSTATUS( gccret );
        }

        if ( !ret )
        {
            std::unique_ptr< llvm::Module > mod = llvmExtract( objFiles, clang );
            std::string file_out = po.outputFile != "" ? po.outputFile : "a.out";

            addSection( file_out, ".llvmbc", clang.serializeModule( *mod ) );
        }

        for ( auto file : objFiles )
        {
            if ( isType( file.first, FileType::Obj ) || isType( file.first, FileType::Archive ) )
                continue;
            std::string ofn = file.second;
            unlink( ofn.c_str() );
        }

        return ret;

    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
