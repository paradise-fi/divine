#include <divine/cc/elf.hpp>
#include <divine/cc/clang.hpp>
#include <divine/cc/compile.hpp>
#include <divine/rt/runtime.hpp>
#include <divine/vm/xg-code.hpp>

DIVINE_RELAX_WARNINGS
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Object/IRObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ELF.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm-c/Object.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-llvm>
#include <iostream>
#include <sys/wait.h>

static const std::string bcsec = ".llvmbc";

using namespace divine;
using namespace llvm;

/// addPassesToX helper drives creation and initialization of TargetPassConfig.
static llvm::MCContext *
addPassesToGenerateCode( LLVMTargetMachine *TM, PassManagerBase &PM, bool DisableVerify )
{
    // Add internal analysis passes from the target machine.
    PM.add( createTargetTransformInfoWrapperPass( TM->getTargetIRAnalysis() ) );

    // Targets may override createPassConfig to provide a target-specific
    // subclass.
    TargetPassConfig *PassConfig = TM->createPassConfig( PM );
    PassConfig->setStartStopPasses( nullptr, nullptr, nullptr );

    // Set PassConfig options provided by TargetMachine.
    PassConfig->setDisableVerify( DisableVerify );

    PM.add( PassConfig );

    PassConfig->addIRPasses();
    PassConfig->addCodeGenPrepare();
    PassConfig->addPassesToHandleExceptions();
    PassConfig->addISelPrepare();

    // Install a MachineModuleInfo class, which is an immutable pass that holds
    // all the per-module stuff we're generating, including MCContext.
    MachineModuleInfo *MMI = new MachineModuleInfo( *TM->getMCAsmInfo(),
                                                    *TM->getMCRegisterInfo(),
                                                     TM->getObjFileLowering() );
    PM.add( MMI );

    // Set up a MachineFunction for the rest of CodeGen to work on.
    PM.add( new MachineFunctionAnalysis( *TM, nullptr ) );

    // Ask the target for an isel.
    if ( PassConfig->addInstSelector() )
        return nullptr;

    PassConfig->addMachinePasses();
    PassConfig->setInitialized();

    return &MMI->getContext();
}

bool addPassesToEmitObjFile( PassManagerBase &PM, raw_pwrite_stream &Out,
                             LLVMTargetMachine *target, MCStreamer** RawAsmStreamer )
{
    // Add common CodeGen passes.
    MCContext *Context = addPassesToGenerateCode( target, PM, true );
    if ( !Context )
        return true;

    if ( target->Options.MCOptions.MCSaveTempLabels )
        Context->setAllowTemporaryLabels( false );

    const MCSubtargetInfo &STI = *( target->getMCSubtargetInfo() );
    const MCRegisterInfo &MRI = *( target->getMCRegisterInfo() );
    const MCInstrInfo &MII = *( target->getMCInstrInfo() );

    // Create the code emitter for the target if it exists.  If not, .o file
    // emission fails.
    MCCodeEmitter *MCE = target->getTarget().createMCCodeEmitter( MII, MRI, *Context );
    MCAsmBackend *MAB = target->getTarget().createMCAsmBackend( MRI, target->getTargetTriple().str(),
                                                                target->getTargetCPU() );
    if ( !MCE || !MAB )
      return true;

    // Don't waste memory on names of temp labels.
    Context->setUseNamesOnTempLabels( false );

    Triple T( target->getTargetTriple().str() );
    *RawAsmStreamer = target->getTarget().createMCObjectStreamer( T, *Context, *MAB,
                                                                  Out, MCE, STI,
                                                                  target->Options.MCOptions.MCRelaxAll,
                                                                  /*DWARFMustBeAtTheEnd*/ true );

    // Create the AsmPrinter, which takes ownership of AsmStreamer if successful.
    FunctionPass *Printer =
            target->getTarget().createAsmPrinter( *target,
                                                  std::unique_ptr< MCStreamer >( *RawAsmStreamer ) );
    if ( !Printer )
        return true;

    PM.add( Printer );

    return false;
}

int emitObjFile( Module &m, std::string filename )
{
    //auto TargetTriple = sys::getDefaultTargetTriple();
    auto TargetTriple = "x86_64-unknown-none-elf";

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

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

    legacy::PassManager pass;

    MCStreamer *AsmStreamer;

    if ( addPassesToEmitObjFile( pass, dest, dynamic_cast< LLVMTargetMachine* >( TargetMachine ), &AsmStreamer ) )
    {
        errs() << "TargetMachine can't emit a file of this type";
        return 1;
    }

    // AsmStreamer now contains valid MCStreamer till pass destruction

    // write bitcode into section .bc
    AsmStreamer->SwitchSection( AsmStreamer->getContext().getELFSection( bcsec, ELF::SHT_NOTE, 0 ) );
    std::string bytes = brick::llvm::getModuleBytes( &m );
    AsmStreamer->EmitBytes( bytes );

    pass.run( m );

    dest.flush();

    return 0;
}

bool isType( std::string file, cc::Compiler::FileType type )
{
    return cc::Compiler::typeFromFile( file ) == type;
}

int llvmExtract( std::vector< std::pair< std::string, std::string > >& files, cc::Compiler& clang )
{
    using FileType = cc::Compiler::FileType;
    using namespace brick::types;
    std::unique_ptr< cc::Compile > compil = std::unique_ptr< cc::Compile >( new cc::Compile( clang.context() ) );
    compil->setupFS( rt::each );

    for ( auto file : files )
    {
        if ( !isType( file.second, FileType::Obj ) && !isType( file.second, FileType::Archive ) )
            continue;

        ErrorOr< std::unique_ptr< MemoryBuffer > > buf = MemoryBuffer::getFile( file.second );
        if ( !buf ) return 1;

        if ( isType( file.second, FileType::Archive ) )
        {
            compil->linkArchive( std::move( buf.get() ) , clang.context() );
            continue;
        }

        auto bc = llvm::object::IRObjectFile::findBitcodeInMemBuffer( (*buf.get()).getMemBufferRef() );
        std::unique_ptr< llvm::Module > m = std::move( llvm::parseBitcodeFile( bc.get(), *clang.context().get()).get() );
        m->setTargetTriple( "x86_64-unknown-none-elf" );
        verifyModule( *m );
        compil->linkModule( std::move( m ) );
    }

    compil->linkEssentials();
    compil->linkLibs( cc::Compile::defaultDIVINELibs );

    auto m = compil->getLinked();

    for( auto& func : *m )
        if( func.isDeclaration() && vm::xg::hypercall( &func ) == vm::lx::NotHypercall )
        {
            std::cerr << "symbol undefined: " << func.getName().str() << std::endl;
            return 1;
        }
    brick::llvm::writeModule( m, "linked.bc" );

    return 0;
}


/* usage: same as gcc */
int main( int argc, char **argv )
{
    try {
        cc::Compiler clang;
        clang.allowIncludePath( "/" );
        divine::rt::each( [&]( auto path, auto c ) { clang.mapVirtualFile( path, c ); } );
        std::vector< std::pair< std::string, std::string > > objFiles;

        std::vector< std::string > opts;
        std::copy( argv + 1, argv + argc, std::back_inserter( opts ) );
        auto po = cc::parseOpts( opts );

        using brick::fs::joinPath;
        using divine::cc::includeDir;

        po.opts.insert( po.opts.end(), {  "-isystem", includeDir
                      , "-isystem", joinPath( includeDir, "libcxx/include" )
                      , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                      , "-isystem", joinPath( includeDir, "libunwind/include" )
                      , "-isystem", joinPath( includeDir, "libc/include" )
                      , "-isystem", joinPath( includeDir, "libc/internals" )
                      , "-isystem", joinPath( includeDir, "libm/include" ) } );

        if ( po.files.size() > 1 && po.outputFile != ""
             && ( po.preprocessOnly || po.toObjectOnly ) )
        {
            std::cerr << "Cannot specify -o with multiple files" << std::endl;
            return 1;
        }

        using FileType = cc::Compiler::FileType;
        using namespace brick::types;

        if ( po.preprocessOnly )
        {
            for ( auto srcFile : po.files )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                if ( isType( ifn, FileType::Obj ) || isType( ifn, FileType::Archive ) )
                    continue;
                std::cout << clang.preprocessModule( ifn, po.opts );
            }
            return 0;
        }

        for ( auto srcFile : po.files )
        {
            if ( srcFile.is< cc::File >() )
            {
                std::string ifn = srcFile.get< cc::File >().name;
                std::string ofn = brick::fs::dropExtension( ifn );
                if ( po.outputFile != "" ) ofn += ".temp";
                ofn += ".o";

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
                auto mod = clang.compileModule( file.first, po.opts );
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
                auto mod = clang.compileModule( file.first, po.opts );
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

        if ( llvmExtract( objFiles, clang ) )
            return 1;

        for ( auto file : objFiles )
        {
            if ( isType( file.first, FileType::Obj ) || isType( file.first, FileType::Archive ) )
                continue;
            std::string ofn = file.second;
            unlink( ofn.c_str() );
        }

        ErrorOr< std::unique_ptr< MemoryBuffer > > buf = MemoryBuffer::getFile( "linked.bc" );
        if ( !buf ) return 1;
        std::string file_out = po.outputFile != "" ? po.outputFile : "a.out";

        divine::cc::elf::addSection( file_out, ".llvmbc", (*buf)->getBuffer() );

        return ret;

    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
