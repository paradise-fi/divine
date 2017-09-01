#include <divine/cc/clang.hpp>
#include <divine/cc/compile.hpp>

DIVINE_RELAX_WARNINGS
#include "llvm/Target/TargetMachine.h"
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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ELF.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-llvm>
#include <iostream>

static const std::string bcsec = ".llvm_bc";

using namespace divine;
using namespace llvm;

/// addPassesToX helper drives creation and initialization of TargetPassConfig.
static llvm::MCContext *
addPassesToGenerateCode( LLVMTargetMachine *TM, PassManagerBase &PM,
                        bool DisableVerify ) {
    // Add internal analysis passes from the target machine.
    PM.add(createTargetTransformInfoWrapperPass( TM->getTargetIRAnalysis()) );

    // Targets may override createPassConfig to provide a target-specific
    // subclass.
    TargetPassConfig *PassConfig = TM->createPassConfig(PM);
    PassConfig->setStartStopPasses( nullptr, nullptr, nullptr );

    // Set PassConfig options provided by TargetMachine.
    PassConfig->setDisableVerify( DisableVerify );

    PM.add(PassConfig);

    PassConfig->addIRPasses();

    PassConfig->addCodeGenPrepare();

    PassConfig->addPassesToHandleExceptions();

    PassConfig->addISelPrepare();

    // Install a MachineModuleInfo class, which is an immutable pass that holds
    // all the per-module stuff we're generating, including MCContext.
    MachineModuleInfo *MMI = new MachineModuleInfo(
      *TM->getMCAsmInfo(), *TM->getMCRegisterInfo(), TM->getObjFileLowering());
    PM.add(MMI);

    // Set up a MachineFunction for the rest of CodeGen to work on.
    PM.add( new MachineFunctionAnalysis( *TM, nullptr) );

    // Ask the target for an isel.
    if ( PassConfig->addInstSelector() )
        return nullptr;

    PassConfig->addMachinePasses();

    PassConfig->setInitialized();

    return &MMI->getContext();
}

bool addPassesToEmitObjFile(
  PassManagerBase &PM, raw_pwrite_stream &Out, LLVMTargetMachine *target,
  MCStreamer** RawAsmStreamer) {
    // Add common CodeGen passes.
    MCContext *Context =
      addPassesToGenerateCode( target, PM, true );
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
    MCAsmBackend *MAB =
        target->getTarget().createMCAsmBackend( MRI, target->getTargetTriple().str(), target->getTargetCPU() );
    if (!MCE || !MAB)
      return true;

    // Don't waste memory on names of temp labels.
    Context->setUseNamesOnTempLabels( false );

    Triple T(target->getTargetTriple().str());
    *RawAsmStreamer = target->getTarget().createMCObjectStreamer(
        T, *Context, *MAB, Out, MCE, STI, target->Options.MCOptions.MCRelaxAll,
        /*DWARFMustBeAtTheEnd*/ true );

    // Create the AsmPrinter, which takes ownership of AsmStreamer if successful.
    FunctionPass *Printer =
      target->getTarget().createAsmPrinter( *target, std::unique_ptr<MCStreamer>(*RawAsmStreamer) );
    if ( !Printer )
        return true;

    PM.add( Printer );

    return false;
}

int emitObjFile( Module &m, std::string filename ) {
    auto TargetTriple = sys::getDefaultTargetTriple();

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
    if ( !Target ) {
        errs() << Error;
        return 1;
    }

    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto RM = Reloc::Model();
    auto TargetMachine = Target->createTargetMachine( TargetTriple, CPU, Features, opt, RM );

    m.setDataLayout(TargetMachine->createDataLayout());
    m.setTargetTriple(TargetTriple);

    std::error_code EC;
    raw_fd_ostream dest( filename, EC, sys::fs::F_None );

    if ( EC ) {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    legacy::PassManager pass;

    MCStreamer* AsmStreamer;

    if ( addPassesToEmitObjFile( pass, dest, dynamic_cast< LLVMTargetMachine* >(TargetMachine), &AsmStreamer ) ) {
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

/* usage: same as gcc */
int main( int argc, char **argv ) {
    try {
        cc::Compiler clang;
        clang.allowIncludePath( "/" );

        std::vector< std::string > opts;
        std::copy( argv + 1, argv + argc, std::back_inserter( opts ) );
        auto po = cc::parseOpts( opts );

        if ( po.files.size() > 1 && po.outputFile != "" ) {
            std::cerr << "Cannot specify -o with multiple files" << std::endl;
            return 1;
        }

        for ( auto srcFile : po.files ) {
            if ( std::holds_alternative< cc::File >( srcFile ) ) {
                std::string ofn = po.outputFile != "" ? po.outputFile
                    : brick::fs::dropExtension( std::get< cc::File >( srcFile ).name ) + ".o";

                auto mod = clang.compileModule( std::get< cc::File >( srcFile ).name, po.opts );

                emitObjFile( *mod, ofn );
            }
        }

        return 0;
    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
