// -*- C++ -*- (c) 2016 Vladimír Štill

DIVINE_RELAX_WARNINGS
#include <llvm/Support/Signals.h>
#include <clang/Tooling/Tooling.h> // ClangTool
#include <clang/CodeGen/CodeGenAction.h> // EmitLLVMAction
#include <clang/Frontend/FrontendActions.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Compilation.h>
#include <clang/Frontend/CompilerInstance.h> // CompilerInvocation
#include <clang/Frontend/DependencyOutputOptions.h>
#include <clang/Frontend/Utils.h>
#include <llvm/Support/Path.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/TargetSelect.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-assert>
#include <brick-query>
#include <brick-except>

#include <iostream>
#include <mutex> // call_once

#include <divine/cc/clang.hpp>
#include <lart/divine/vaarg.h>

namespace divine {
namespace cc {

class GetPreprocessedAction : public clang::PreprocessorFrontendAction {
public:
    std::string output;
protected:
    void ExecuteAction() override {
        clang::CompilerInstance &CI = getCompilerInstance();
        llvm::raw_string_ostream os( output );
        clang::DoPrintPreprocessedInput(CI.getPreprocessor(), &os,
                            CI.getPreprocessorOutputOpts());
    }

    bool hasPCHSupport() const override { return true; }
};

template < typename Action >
auto buildAction( llvm::LLVMContext *ctx )
    -> decltype( std::enable_if_t< std::is_base_of< clang::CodeGenAction, Action >::value,
        std::unique_ptr< Action > >{} )
{
    return std::make_unique< Action >( ctx );
}

template < typename Action >
auto buildAction( llvm::LLVMContext * )
    -> decltype( std::enable_if_t< !std::is_base_of< clang::CodeGenAction, Action >::value,
        std::unique_ptr< Action > >{} )
{
    return std::make_unique< Action >();
}

template< typename Contailer >
void dump( const Contailer &c ) {
    std::copy( c.begin(), c.end(),
               std::ostream_iterator< std::decay_t< decltype( *c.begin() ) > >( std::cerr, " " ) );
}

template< typename Contailer >
void dump( const Contailer *c ) {
    std::cerr << static_cast< const void * >( c ) << "= @( ";
    dump( *c );
    std::cerr << ")" << std::endl;
}

Compiler::Compiler( std::shared_ptr< llvm::LLVMContext > ctx ) :
    divineVFS( new DivineVFS() ),
    overlayFS( new clang::vfs::OverlayFileSystem( clang::vfs::getRealFileSystem() ) ),
    ctx( ctx )
{
    if ( !ctx )
        this->ctx = std::make_shared< llvm::LLVMContext >();
    // setup VFS
    overlayFS->pushOverlay( divineVFS );
    if ( !ctx )
        ctx.reset( new llvm::LLVMContext );
}

Compiler::~Compiler() { }

void Compiler::mapVirtualFile( std::string path, std::string contents )
{
    divineVFS->addFile( path, std::move( contents ) );
}

void Compiler::mapVirtualFile( std::string path, std::string_view contents )
{
    divineVFS->addFile( path, llvm::StringRef( contents.data(), contents.size() ) );
}

std::vector< std::string > Compiler::filesMappedUnder( std::string path ) {
    return divineVFS->filesMappedUnder( path );
}

void Compiler::allowIncludePath( std::string path ) {
    divineVFS->allowPath( path );
}

template< typename CodeGenAction >
std::unique_ptr< CodeGenAction > Compiler::cc1( std::string filename,
                            FileType type, std::vector< std::string > args,
                            llvm::IntrusiveRefCntPtr< clang::vfs::FileSystem > vfs )
{
    if ( !vfs )
        vfs = overlayFS;

    // Build an invocation
    auto invocation = std::make_shared< clang::CompilerInvocation >();
    std::vector< std::string > cc1args = { "-cc1",
                                            "-triple", "x86_64-unknown-none-elf",
                                            "-emit-obj",
                                            "-mrelax-all",
                                            // "-disable-free",
                                            // "-disable-llvm-verifier",
//                                                "-main-file-name", "test.c",
                                            "-mrelocation-model", "static",
                                            "-mthread-model", "posix",
                                            "-mdisable-fp-elim",
                                            "-fmath-errno",
                                            "-masm-verbose",
                                            "-mconstructor-aliases",
                                            "-munwind-tables",
                                            "-fuse-init-array",
                                            "-target-cpu", "x86-64",
                                            "-dwarf-column-info",
                                            // "-coverage-file", "/home/xstill/DiVinE/clangbuild/test.c", // ???
                                            // "-resource-dir", "../lib/clang/3.7.1", // ???
                                            // "-internal-isystem", "/usr/local/include",
                                            // "-internal-isystem", "../lib/clang/3.7.1/include",
                                            // "-internal-externc-isystem", "/include",
                                            // "-internal-externc-isystem", "/usr/include",
                                            // "-fdebug-compilation-dir", "/home/xstill/DiVinE/clangbuild", // ???
                                            "-ferror-limit", "19",
                                            "-fmessage-length", "212",
                                            "-mstackrealign",
                                            "-fobjc-runtime=gcc",
                                            "-fdiagnostics-show-option",
                                            "-fcolor-diagnostics",
                                            // "-o", "test.o",
                                            };
    add( args, cc1args );
    add( args, argsOfType( type ) );
    args.push_back( filename );


    std::vector< const char * > cc1a;
    std::transform( args.begin(), args.end(),
                    std::back_inserter( cc1a ),
                    []( std::string &str ) { return str.c_str(); } );

    clang::TextDiagnosticPrinter diagprinter( llvm::errs(), new clang::DiagnosticOptions() );
    clang::DiagnosticsEngine diag(
            llvm::IntrusiveRefCntPtr< clang::DiagnosticIDs >( new clang::DiagnosticIDs() ),
            new clang::DiagnosticOptions(), &diagprinter, false );
    bool succ = clang::CompilerInvocation::CreateFromArgs(
                        *invocation, &cc1a[ 0 ], &*cc1a.end(), diag );
    if ( !succ )
        throw CompileError( "Failed to create a compiler invocation for " + filename );
    invocation->getDependencyOutputOpts() = clang::DependencyOutputOptions();

    // actually run the compiler invocation
    clang::CompilerInstance compiler( std::make_shared< clang::PCHContainerOperations>() );
    auto fmgr = std::make_unique< clang::FileManager >( clang::FileSystemOptions(), vfs );
    compiler.setFileManager( fmgr.get() );
    compiler.setInvocation( invocation );
    ASSERT( compiler.hasInvocation() );
    compiler.createDiagnostics( &diagprinter, false );
    ASSERT( compiler.hasDiagnostics() );
    compiler.createSourceManager( *fmgr.release() );
    compiler.getPreprocessorOutputOpts().ShowCPP = 1; // Allows for printing preprocessor
    auto emit = buildAction< CodeGenAction >( ctx.get() );
    succ = compiler.ExecuteAction( *emit );
    if ( !succ )
        throw CompileError( "Error building " + filename );

    return emit;
}

std::string  Compiler::preprocessModule( std::string filename,
                            FileType type, std::vector< std::string > args )
{
    auto prep = cc1< GetPreprocessedAction >( filename, type, args );
    return prep->output;
}

std::unique_ptr< llvm::Module > Compiler::compileModule( std::string filename,
                            FileType type, std::vector< std::string > args )
{
    // EmitLLVMOnlyAction emits module in memory, does not write it into a file
    auto emit = cc1< clang::EmitLLVMOnlyAction >( filename, type, args );
    auto mod = emit->takeModule();
    lart::divine::VaArgInstr().run( *mod );
    return mod;
}

static std::once_flag initTargetsFlags;

static void initTargets() {
    std::call_once( initTargetsFlags, [] {
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmPrinters();
            llvm::InitializeAllAsmParsers();
        } );
}

void Compiler::emitObjFile( llvm::Module &m, std::string filename, std::vector< std::string > args ) {
    args.push_back( "-o" );
    args.push_back( filename );

    std::string modulepath = "/divine/module.bc";

    llvm::IntrusiveRefCntPtr< DivineVFS > modulefs( new DivineVFS() );
    modulefs->addFile( modulepath, serializeModule( m ) );

    initTargets();
    cc1< clang::EmitObjAction >( modulepath, FileType::BC, args, modulefs );
}

std::string Compiler::serializeModule( llvm::Module &m ) {
    std::string str;
    {
        llvm::raw_string_ostream os( str );
        llvm::WriteBitcodeToFile( &m, os );
        os.flush();
    }
    return str;
}

std::unique_ptr< llvm::Module > Compiler::materializeModule( llvm::StringRef str )
{
    auto parsed = llvm::parseBitcodeFile( llvm::MemoryBufferRef( str, "module.bc" ), *ctx );
    if ( !parsed )
        return nullptr;
    return std::move( parsed.get() );
}

bool Compiler::fileExists( llvm::StringRef file )
{
    auto stat = overlayFS->status( file );
    return stat && stat->exists();
}

std::unique_ptr< llvm::MemoryBuffer > Compiler::getFileBuffer( llvm::StringRef file )
{
    auto buf = overlayFS->getBufferForFile( file );
    return buf ? std::move( buf.get() ) : nullptr;
}

} // namespace cc
} // namespace divine
