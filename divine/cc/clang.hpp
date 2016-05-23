// -*- C++ -*- (c) 2016 Vladimír Štill

DIVINE_RELAX_WARNINGS
#include <llvm/Support/Signals.h>
#include <clang/Tooling/Tooling.h> // ClangTool
#include <clang/CodeGen/CodeGenAction.h> // EmitLLVMAction
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Compilation.h>
#include <clang/Frontend/CompilerInstance.h> // CompilerInvocation
#include <clang/Frontend/DependencyOutputOptions.h>
#include <llvm/Support/Errc.h> // for VFS
#include <llvm/Bitcode/ReaderWriter.h>
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-assert>
#include <brick-query>
#include <brick-types>

#include <algorithm>
#include <iterator>
#include <iostream>
#include <system_error>
#include <thread>

namespace divine {

#if 0
extern stringtable pdclib_list[];
extern stringtable libm_list[];
extern stringtable libunwind_list[];
extern stringtable libcxxabi_list[];
extern stringtable libcxx_list[];
#endif

namespace cc {

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
    // just a wrapper over const char *
    struct CharMemBuf : llvm::MemoryBuffer {
        BufferKind getBufferKind() const override { return BufferKind( 2 ); } // meh
        CharMemBuf( const char *data ) {
            init( data, data + std::strlen( data ), false );
        }
    };

    // adapt file map to the expectations of LLVM's VFS
    struct File : clang::vfs::File {
        File( const char *content, clang::vfs::Status stat ) :
            content( content ), stat( stat )
        { }

        llvm::ErrorOr< clang::vfs::Status > status() override { return stat; }

        auto getBuffer( const llvm::Twine &/* path */, int64_t /* fileSize */ = -1,
                        bool /* requiresNullTerminator */ = true,
                        bool /* IsVolatile */ = false ) ->
            llvm::ErrorOr< std::unique_ptr< llvm::MemoryBuffer > > override
        {
            return { std::make_unique< CharMemBuf >( content ) };
        }

        void setName( llvm::StringRef ) override { }

        std::error_code close() override { return std::error_code(); }

      private:
        const char *content;
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
        addFile( name, storage.back().c_str(), allowOverride );
    }

    void addFile( std::string path, const char *contents, bool allowOverride = false ) {
        ASSERT( allowOverride || !filemap.count( path ) );
        auto &ref = filemap[ path ];
        ref.first = contents;
        auto name = llvm::sys::path::filename( path );
        ref.second = clang::vfs::Status( name, name,
                        clang::vfs::getNextVirtualUniqueID(),
                        llvm::sys::TimeValue(), 0, 0, std::strlen( contents ),
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

    std::map< std::string, std::pair< const char *, clang::vfs::Status > > filemap;
    std::vector< std::string > storage;
    std::set< std::string > allowedPrefixes;
};

/*
 A compiler, cabaple of compiling single file into LLVM module

 The compiler is not thread safe, and all modules are bound to its context (so
 they will not be usable when the compiler is destructed. However, as each
 compiler has its own context, multiple compilers in different threads will
 work
 */
struct Compiler {

    enum class FileType {
        Unknown,
        C, Cpp, CPrepocessed, CppPreprocessed, IR, BC
    };

    FileType typeFromFile( std::string name ) {
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
        return FileType::Unknown;
    }

    void setupStacktracePrinter() {
        llvm::sys::PrintStackTraceOnErrorSignal();
    }

    std::vector< std::string > argsOfType( FileType t ) {
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

    Compiler( std::shared_ptr< llvm::LLVMContext > ctx = nullptr ) :
        divineVFS( new DivineVFS() ),
        overlayFS( new clang::vfs::OverlayFileSystem( clang::vfs::getRealFileSystem() ) ),
        ctx( ctx )
    {
        // setup VFS
        overlayFS->pushOverlay( divineVFS );
        if ( !ctx )
            ctx.reset( new llvm::LLVMContext );
    }

    template< typename T >
    Compiler &mapVirtualFile( std::string path, const T &contents ) {
        divineVFS->addFile( path, contents );
        return *this;
    }

    template< typename T >
    Compiler &reMapVirtualFile( std::string path, const T &contents ) {
        divineVFS->addFile( path, contents, true );
        return *this;
    }

    template< typename T >
    Compiler &mapVirtualFiles( std::initializer_list< std::pair< std::string, const T & > > files ) {
        for ( auto &x : files )
            mapVirtualFile( x.first, x.second );
        return *this;
    }

    std::vector< std::string > filesMappedUnder( std::string path ) {
        return divineVFS->filesMappedUnder( path );
    }

    Compiler &allowIncludePath( std::string path ) {
        divineVFS->allowPath( path );
        return *this;
    }

    std::unique_ptr< llvm::Module > compileModule( std::string filename,
                                FileType type, std::vector< std::string > args )
    {
        // Build an invocation
        auto invocation = std::make_unique< clang::CompilerInvocation >();
        std::vector< std::string > cc1args = { "-cc1",
                                                "-triple", "x86_64-unknown-linux-gnu",
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
        ASSERT( succ );
        invocation->getDependencyOutputOpts() = clang::DependencyOutputOptions();

        // actually run the compiler invocation
        clang::CompilerInstance compiler( std::make_shared< clang::PCHContainerOperations>() );
        auto fmgr = std::make_unique< clang::FileManager >( clang::FileSystemOptions(), overlayFS );
        compiler.setFileManager( fmgr.get() );
        compiler.setInvocation( invocation.release() );
        ASSERT( compiler.hasInvocation() );
        compiler.createDiagnostics( &diagprinter, false );
        ASSERT( compiler.hasDiagnostics() );
        compiler.createSourceManager( *fmgr.release() );
        // emits module in memory, does not write it info a file
        auto emit = std::make_unique< clang::EmitLLVMOnlyAction >( ctx.get() );
        succ = compiler.ExecuteAction( *emit );
        ASSERT( succ );

        return emit->takeModule();
    }

    auto compileModule( std::string filename, std::vector< std::string > args = { } )
    {
        return compileModule( filename, typeFromFile( filename ), args );
    }

    static std::string serializeModule( llvm::Module &m ) {
        std::string str;
        {
            llvm::raw_string_ostream os( str );
            llvm::WriteBitcodeToFile( &m, os );
            os.flush();
        }
        return str;
    }

    template< typename ... Args >
    std::string compileAndSerializeModule( Args &&... args ) {
        auto mod = compileModule( std::forward< Args >( args )... );
        return serializeModule( mod.get() );
    }

    std::unique_ptr< llvm::Module > materializeModule( llvm::StringRef str ) {
        return std::move( llvm::parseBitcodeFile(
                              llvm::MemoryBufferRef( str, "module.bc" ), context() ).get() );
    }

    llvm::LLVMContext &context() { return *ctx.get(); }

  private:

    template< typename A, typename B = std::initializer_list< std::decay_t<
                            decltype( *std::declval< A & >().begin() ) > > >
    static void add( A &a, B b ) {
        std::copy( b.begin(), b.end(), std::back_inserter( a ) );
    }

    llvm::IntrusiveRefCntPtr< DivineVFS > divineVFS;
    llvm::IntrusiveRefCntPtr< clang::vfs::OverlayFileSystem > overlayFS;
    std::shared_ptr< llvm::LLVMContext > ctx;
};

template< typename Namespace = std::initializer_list< std::string > >
std::string stringifyToCode( Namespace ns, std::string name, std::string value ) {
    std::stringstream ss;
    for ( auto &n : ns )
        ss << "namespace " << n << "{" << std::endl;

    ss << "const char " << name << "[] = {" << std::hex << std::endl;
    int i = 0;
    for ( auto c : value ) {
        ss << "0x" << c << ", ";
        if ( ++i > 12 ) {
            ss << std::endl;
            i = 0;
        }
    }
    ss << "};";

    for ( auto &n : ns )
        ss << "} // namespace " << n << std::endl;
}

struct Compile {

    template< typename T >
    struct Wrapper {
        Wrapper( T &val ) : val( val ) { }
        Wrapper( T &&val ) : val( std::move( val ) ) { }
        T &get() { return val; }
        const T &get() const { return val; }
      private:
        T val;
    };

    struct NumThreads : Wrapper< int > {
        using Wrapper< int >::Wrapper;
    };
    struct Precompiled : Wrapper< std::string > {
        using Wrapper< std::string >::Wrapper;
    };
    struct LibsOnly { };
    struct DontLink { };
    struct DisableVFS { };
    struct VFSInput : Wrapper< std::string > {
        using Wrapper< std::string >::Wrapper;
    };
    struct VFSSnapshot : Wrapper< std::string > {
        using Wrapper< std::string >::Wrapper;
    };

    using Options = std::vector< brick::types::Union< NumThreads, Precompiled,
                                 LibsOnly, DisableVFS, VFSInput, VFSSnapshot,
                                 DontLink > >;

    std::string addSnapshot() {
        /*
        auto path = join( srcDir, "divine/fs-snapshot.cpp" );
        std::stringstream snapshot;
        compile::Snapshot::writeFile( snapshot, vfsSnapshot, vfsInput );
        mastercc().mapVirtualFile( path, snapshot.str() );
        return path;
        */
    }

    explicit Compile( Options opts = { } ) : compilers( 1 ), workers( 1 ) {
        for ( auto &opt : opts )
            opt.match( [this]( NumThreads ) {
                    NOT_IMPLEMENTED();
                }, [this]( Precompiled p ) {
                    precompiled = p.get();
                }, [this]( LibsOnly ) {
                    libsOnly = true;
                }, [this]( DisableVFS ) {
                    vfs = false;
                    ASSERT( vfsSnapshot.empty() );
                    ASSERT( vfsInput.empty() );
                }, [this]( VFSInput vi ) {
                    vfsInput = vi.get();
                    ASSERT( vfs );
                }, [this]( VFSSnapshot vs ) {
                    vfsSnapshot = vs.get();
                    ASSERT( vfs );
                }, [this]( DontLink ) {
                    dontLink = true;
                } );
        ASSERT_LEQ( 1ul, workers.size() );
        ASSERT_EQ( workers.size(), compilers.size() );

        setupFS();
        if ( !libsOnly && precompiled.empty() && vfs )
            addSnapshot();
        if ( !dontLink )
            setupLibs();
    }

    void compileAndLink( std::string path, std::vector< std::string > flags = { } ) {
        linker.link( compile( path, flags ) );
    }

    std::unique_ptr< llvm::Module > compile( std::string path,
            std::vector< std::string > flags = { } )
    {
        std::vector< std::string > allFlags;
        std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
        std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );

        std::cerr << "compiling " << path << std::endl;
        return mastercc().compileModule( path, allFlags );
    }

    llvm::Module *getLinked() { return linker.get(); }

    void writeToFile( std::string filename ) {
        std::error_code serr;
        ::llvm::raw_fd_ostream outs( filename, serr, ::llvm::sys::fs::F_None );
        WriteBitcodeToFile( linker.get(), outs );
    }

    std::string serialize() {
        return mastercc().serializeModule( *linker.get() );
    }

    void addDirectory( std::string path ) {
        mastercc().allowIncludePath( path );
    }

    void addFlags( std::vector< std::string > flags ) {
        std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
    }

    template< typename Roots = std::initializer_list< std::string > >
    void prune( Roots r ) { linker.prune( r, brick::llvm::Prune::UnusedModules ); }

  private:
    Compiler &mastercc() { return compilers[0]; }

    enum class Type { Header, Source, All };

    static bool isSource( std::string x ) {
        using brick::string::endsWith;
        return endsWith( x, ".c") || endsWith( x, ".cpp" ) || endsWith( x, ".cc" );
    }

    template< typename Src >
    void prepareSources( std::string basedir, Src src, Type type = Type::All,
            std::function< bool( std::string ) > filter = nullptr )
    {
        while ( src->n ) {
            if ( ( !filter || filter( src->n ) )
                 && ( ( type == Type::Header && !isSource( src->n ) )
                    || ( type == Type::Source && isSource( src->n ) )
                    || type == Type::All ) )
            {
                auto path = join( basedir, src->n );
                mastercc().mapVirtualFile( path, src->c );
            }
            ++src;
        }
    }

    void setupFS() {
#if 0
        prepareSources( includeDir, llvm_h_list );
        mastercc().mapVirtualFiles< const char * >( {
            { join( includeDir, "bits/pthreadtypes.h" ), "#include <pthread.h>\n" },
            { join( includeDir, "divine/problem.def" ), src_llvm::llvm_problem_def_str } } );
        prepareSources( join( srcDir, "divine" ),   llvm_list,      Type::All,
            [&]( std::string name ) { return vfs || !brick::string::startsWith( name, "fs" ); } );
        prepareSources( join( srcDir, "lart" ),     lart_list,      Type::Source );
        prepareSources( join( includeDir, "lart" ), lart_list,      Type::Header );
        prepareSources( join( srcDir, "libpdc" ),   pdclib_list,    Type::Source );
        prepareSources( includeDir,                 pdclib_list,    Type::Header );
        prepareSources( join( srcDir, "limb" ),     libm_list,      Type::Source );
        prepareSources( includeDir,                 libm_list,      Type::Header );
        prepareSources( includeDir,                 libcxxabi_list, Type::Header );
        prepareSources( includeDir,                 libcxx_list,    Type::Header );
        prepareSources( join( srcDir, "cxxabi" ),   libcxxabi_list, Type::Source );
        prepareSources( join( srcDir, "cxx" ),      libcxx_list,    Type::Source );

        mastercc().reMapVirtualFile( join( includeDir, "assert.h" ), "#include <divine.h>\n" ); // override PDClib's assert
#endif
    }

    void setupLibs() {
        if ( precompiled.size() ) {
            auto input = std::move( llvm::MemoryBuffer::getFile( precompiled ).get() );
            ASSERT( !!input );

            auto inputData = input->getMemBufferRef();
            auto parsed = parseBitcodeFile( inputData, context() );
            if ( !parsed )
                throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
            linker.load( std::move( parsed.get() ) );
            if ( vfs )
                compileAndLink( addSnapshot(), { "-std=c++14", "-Oz" } );
        } else {
            compileLibrary( join( srcDir, "libpdc" ), { "-D_PDCLIB_BUILD" } );
            compileLibrary( join( srcDir, "limb" ) );
            std::initializer_list< std::string > cxxflags = { "-std=c++14"
                                                            // , "-fstrict-aliasing"
                                                            , "-I/usr/include/include"
                                                            , "-I/usr/include/src"
                                                            , "-Oz" };
            compileLibrary( join( srcDir, "cxxabi" ), cxxflags );
            compileLibrary( join( srcDir, "cxx" ), cxxflags );
            compileLibrary( join( srcDir, "divine" ), cxxflags );
            compileLibrary( join( srcDir, "lart" ), cxxflags );
        }
    }

    void compileLibrary( std::string path, std::initializer_list< std::string > flags = { } )
    {
        for ( const auto &f : mastercc().filesMappedUnder( path ) )
            compileAndLink( f, flags );
    }

    llvm::LLVMContext &context() { return mastercc().context(); }

    template< typename ... T >
    std::string join( T &&... xs ) { return brick::fs::joinPath( std::forward< T >( xs )... ); }

    const std::string includeDir = "/usr/include";
    const std::string srcDir = "/dvc";

    std::string precompiled;
    std::vector< Compiler > compilers;
    std::vector< std::thread > workers;
    brick::llvm::Linker linker;
    bool vfs = true, dontLink = false;
    bool libsOnly = false;
    std::string vfsSnapshot;
    std::string vfsInput;
    std::vector< std::string > commonFlags = { "-D__divine__"
                                             , "-isystem", "/usr/include"
                                             , "-isystem", "/usr/include/std"
                                             , "-D_POSIX_C_SOURCE=2008098L"
                                             , "-D_LITTLE_ENDIAN=1234"
                                             , "-D_BYTE_ORDER=1234"
//                                             , "-fno-slp-vectorize"
//                                             , "-fno-vectorize"
                                             };
};

} // namespace cc
} // namespace divine
