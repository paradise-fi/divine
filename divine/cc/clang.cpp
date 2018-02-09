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
#include <llvm/Support/Errc.h> // for VFS
#include <llvm/Bitcode/ReaderWriter.h>
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

template< typename A, typename B = std::initializer_list< std::decay_t<
                        decltype( *std::declval< A & >().begin() ) > > >
static void add( A &a, B b ) {
    std::copy( b.begin(), b.end(), std::back_inserter( a ) );
}

enum class DivineVFSError {
    InvalidIncludePath = 1000 // avoid clash with LLVM's error codes, they don't check category
};

struct DivineVFSErrorCategory : std::error_category {
    const char *name() const noexcept override { return "DIVINE VFS error"; }
    std::string message( int condition ) const override {
        switch ( DivineVFSError( condition ) ) {
            case DivineVFSError::InvalidIncludePath:
                return "Invalid include path, not accessible in DIVINE";
        }
    }
};

static std::error_code make_error_code( DivineVFSError derr ) {
    return std::error_code( int( derr ), DivineVFSErrorCategory() );
}

}
}

namespace std {
    template<>
    struct is_error_code_enum< divine::cc::DivineVFSError > : std::true_type { };
}

namespace divine {
namespace cc {

struct DivineVFS : clang::vfs::FileSystem {
  private:

    // adapt file map to the expectations of LLVM's VFS
    struct File : clang::vfs::File {
        File( llvm::StringRef content, clang::vfs::Status stat ) :
            content( content ), stat( stat )
        { }

        llvm::ErrorOr< clang::vfs::Status > status() override { return stat; }

        auto getBuffer( const llvm::Twine &/* path */, int64_t /* fileSize */ = -1,
                        bool /* requiresNullTerminator */ = true,
                        bool /* IsVolatile */ = false ) ->
            llvm::ErrorOr< std::unique_ptr< llvm::MemoryBuffer > > override
        {
            return { llvm::MemoryBuffer::getMemBuffer( content ) };
        }

        void setName( llvm::StringRef ) override { }

        std::error_code close() override { return std::error_code(); }

      private:
        llvm::StringRef content;
        clang::vfs::Status stat;
    };

    static auto doesNotExits() { // forward to real FS
        return std::error_code( llvm::errc::no_such_file_or_directory );
    }
    static auto blockAccess( const llvm::Twine & ) {
        return std::error_code( DivineVFSError::InvalidIncludePath );
    };

    clang::vfs::Status statpath( const std::string &path, clang::vfs::Status stat ) {
        stat.setName( path );
        return stat;
    }

  public:

    auto status( const llvm::Twine &_path ) ->
        llvm::ErrorOr< clang::vfs::Status > override
    {
        auto path = brick::fs::normalize( _path.str() );
        auto it = filemap.find( path );
        if ( it != filemap.end() )
            return statpath( path, it->second.second );
        else if ( allowed( path ) )
            return { doesNotExits() };
        return { blockAccess( path ) };
    }

    auto openFileForRead( const llvm::Twine &_path ) ->
        llvm::ErrorOr< std::unique_ptr< clang::vfs::File > > override
    {
        auto path = brick::fs::normalize( _path.str() );

        auto it = filemap.find( path );
        if ( it != filemap.end() )
            return mkfile( it, statpath( path, it->second.second ) );
        else if ( allowed( path ) )
            return { doesNotExits() };
        return { blockAccess( path ) };
    }

    auto dir_begin( const llvm::Twine &_path, std::error_code & ) ->
        clang::vfs::directory_iterator override
    {
        auto path = brick::fs::normalize( _path.str() );
        std::cerr << "DVFS:dir_begin " << path << std::endl;
        NOT_IMPLEMENTED();
    }

    void allowPath( std::string path ) {
        path = brick::fs::normalize( path );
        allowedPrefixes.insert( path );
        auto parts = brick::fs::splitPath( path );
        addDir( parts.begin(), parts.end() );
    }

    void addFile( std::string name, std::string contents, bool allowOverride = false ) {
        storage.emplace_back( std::move( contents ) );
        addFile( name, llvm::StringRef( storage.back() ), allowOverride );
    }

    void addFile( std::string path, llvm::StringRef contents, bool allowOverride = false ) {
        ASSERT( allowOverride || !filemap.count( path ) );
        auto &ref = filemap[ path ];
        ref.first = contents;
        auto name = llvm::sys::path::filename( path );
        ref.second = clang::vfs::Status( name, name,
                        clang::vfs::getNextVirtualUniqueID(),
                        llvm::sys::TimeValue(), 0, 0, contents.size(),
                        llvm::sys::fs::file_type::regular_file,
                        llvm::sys::fs::perms::all_all );
        auto parts = brick::fs::splitPath( path );
        if ( !parts.empty() )
            addDir( parts.begin(), parts.end() - 1 );
    }

    template< typename It >
    void addDir( It begin, It end ) {
        for ( auto it = begin; it < end; ++it ) {
            auto path = brick::fs::joinPath( begin, std::next( it ) );
            filemap[ path ] = { "", clang::vfs::Status( *it, *it,
                    clang::vfs::getNextVirtualUniqueID(),
                    llvm::sys::TimeValue(), 0, 0, 0,
                    llvm::sys::fs::file_type::directory_file,
                    llvm::sys::fs::perms::all_all ) };
        }
    }

    std::vector< std::string > filesMappedUnder( std::string path ) {
        auto prefix = brick::fs::splitPath( path );
        return brick::query::query( filemap )
            .filter( []( auto &pair ) { return !pair.second.second.isDirectory(); } )
            .map( []( auto &pair ) { return pair.first; } )
            .filter( [&]( auto p ) {
                    auto split = brick::fs::splitPath( p );
                    return split.size() >= prefix.size()
                           && std::equal( prefix.begin(), prefix.end(), split.begin() );
                } )
            .freeze();
    }

  private:

    bool allowed( std::string path ) {
        if ( path.empty() || brick::fs::isRelative( path ) )
            return true; // relative or .

        auto parts = brick::fs::splitPath( path );
        for ( auto it = std::next( parts.begin() ); it < parts.end(); ++it )
            if ( allowedPrefixes.count( brick::fs::joinPath( parts.begin(), it ) ) )
                return true;
        return false;
    }

    template< typename It >
    std::unique_ptr< clang::vfs::File > mkfile( It it, clang::vfs::Status stat ) {
        return std::make_unique< File >( it->second.first, stat );
    }

    std::map< std::string, std::pair< llvm::StringRef, clang::vfs::Status > > filemap;
    std::vector< std::string > storage;
    std::set< std::string > allowedPrefixes;
};

Compiler::FileType Compiler::typeFromFile( std::string name ) {
    auto ext = brick::fs::takeExtension( name );
    if ( ext == ".c" )
        return FileType::C;
    for ( auto &x : { ".cpp", ".cc", ".C", ".CPP", ".c++", ".cp", ".cxx" } )
        if ( ext == x )
            return FileType::Cpp;
    if ( ext == ".i" )
        return FileType::CPrepocessed;
    if ( ext == ".ii" )
        return FileType::CppPreprocessed;
    if ( ext == ".bc" )
        return FileType::BC;
    if ( ext == ".ll" )
        return FileType::IR;
    if ( ext == ".S" || ext == ".s" )
        return FileType::Asm;
    if ( ext == ".o" )
        return FileType::Obj;
    if ( ext == ".a" )
        return FileType::Archive;
    return FileType::Unknown;
}

Compiler::FileType Compiler::typeFromXOpt( std::string selector ) {
    if ( selector == "c++" )
        return FileType::Cpp;
    if ( selector == "c" )
        return FileType::C;
    if ( selector == "c++cpp-output" )
        return FileType::CppPreprocessed;
    if ( selector == "cpp-output" )
        return FileType::CPrepocessed;
    if ( selector == "ir" )
        return FileType::IR;
    if ( selector == "obj" )
        return FileType::Obj;
    if ( selector == "archive" )
        return FileType::Archive;
    return FileType::Unknown;
}

std::vector< std::string > Compiler::argsOfType( FileType t ) {
    std::vector< std::string > out { "-x" };
    switch ( t ) {
        case FileType::Cpp:
            add( out, { "c++" } );
            break;
        case FileType::C:
            add( out, { "c" } );
            break;
        case FileType::CppPreprocessed:
            add( out, { "c++cpp-output" } );
            break;
        case FileType::CPrepocessed:
            add( out, { "cpp-output" } );
            break;
        case FileType::BC:
        case FileType::IR:
            add( out, { "ir" } );
            break;
        case FileType::Asm:
            add( out, { "assembler-with-cpp" } );
            break;
        case FileType::Obj:
            add( out, { "obj" } );
        case FileType::Archive:
            add( out, { "archive" } );
        case FileType::Unknown:
            UNREACHABLE( "Unknown file type" );
    }
    switch ( t ) {
        case FileType::Cpp:
        case FileType::CppPreprocessed:
            add( out, { "-fcxx-exceptions", "-fexceptions" } );
            break;
        default: ;
    }
    return out;
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
    auto invocation = std::make_unique< clang::CompilerInvocation >();
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
    add( cc1args, args );
    add( cc1args, argsOfType( type ) );
    cc1args.push_back( filename );


    std::vector< const char * > cc1a;
    std::transform( cc1args.begin(), cc1args.end(),
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
    compiler.setInvocation( invocation.release() );
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
    // EmitLLVMOnlyAction emits module in memory, does not write it info a file
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

std::unique_ptr< llvm::Module > Compiler::materializeModule( llvm::StringRef str ) {
    return std::move( llvm::parseBitcodeFile(
                llvm::MemoryBufferRef( str, "module.bc" ), *ctx ).get() );
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
