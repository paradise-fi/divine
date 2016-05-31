// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_os_ostream.h>
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-assert>
#include <brick-query>
#include <brick-types>
#include <brick-string>

#include <algorithm>
#include <iterator>
#include <iostream>
#include <system_error>
#include <thread>

extern const char *DIVINE_RUNTIME_SHA;

namespace divine {

struct stringtable { const char *n, *c; };
extern stringtable runtime_list[];

namespace cc {

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
        auto mod = mastercc().compileModule( path, allFlags );
        tagWithRuntimeVersionSha( *mod );
        return mod;
    }

    llvm::Module *getLinked() {
        tagWithRuntimeVersionSha( *linker.get() );
        return linker.get();
    }

    void writeToFile( std::string filename ) {
        llvm::raw_os_ostream cerr( std::cerr );
        auto *mod = getLinked();
        if ( llvm::verifyModule( *mod, &cerr ) ) {
            cerr.flush(); // otherwise nothing will be displayed
            UNREACHABLE( "invalid bitcode" );
        }
        std::error_code serr;
        ::llvm::raw_fd_ostream outs( filename, serr, ::llvm::sys::fs::F_None );
        WriteBitcodeToFile( mod, outs );
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

    void tagWithRuntimeVersionSha( llvm::Module &m ) const {
        auto *meta = m.getNamedMetadata( runtimeVersMeta );
        // check old metadata
        std::set< std::string > found;
        found.emplace( DIVINE_RUNTIME_SHA );
        if ( meta ) {
            for ( int i = 0; i < meta->getNumOperands(); ++i )
                found.emplace( getWrappedMDS( meta, i ) );
            m.eraseNamedMetadata( meta );
        }
        meta = m.getOrInsertNamedMetadata( runtimeVersMeta );
        auto *tag = llvm::MDNode::get( m.getContext(),
                        llvm::MDString::get( m.getContext(),
                            found.size() == 1
                                ? DIVINE_RUNTIME_SHA
                                : "!mismatched version of divine cc and runtime!" ) );
        meta->addOperand( tag );
    }

    std::string getRuntimeVersionSha( llvm::Module &m ) const {
        auto *meta = m.getNamedMetadata( runtimeVersMeta );
        if ( !meta )
            return "";
        return getWrappedMDS( meta );
    }

    std::shared_ptr< llvm::LLVMContext > context() { return mastercc().context(); }

  private:
    Compiler &mastercc() { return compilers[0]; }

    enum class Type { Header, Source, All };

    static std::string getWrappedMDS( llvm::NamedMDNode *meta, int i = 0, int j = 0 ) {
        auto *op = llvm::cast< llvm::MDTuple >( meta->getOperand( i ) );
        auto *str = llvm::cast< llvm::MDString >( op->getOperand( j ).get() );
        return str->getString().str();
    }

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
        prepareSources( includeDir, runtime_list, Type::Header );
        prepareSources( srcDir, runtime_list, Type::Source,
            [&]( std::string name ) { return vfs || !brick::string::startsWith( name, "filesystem" ); } );
    }

    void setupLibs() {
        if ( precompiled.size() ) {
            auto input = std::move( llvm::MemoryBuffer::getFile( precompiled ).get() );
            ASSERT( !!input );

            auto inputData = input->getMemBufferRef();
            auto parsed = parseBitcodeFile( inputData, *context() );
            if ( !parsed )
                throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
            if ( getRuntimeVersionSha( *parsed.get() ) != DIVINE_RUNTIME_SHA )
                std::cerr << "WARNING: runtime version of the precompiled library does not match the current runtime version"
                          << std::endl;
            linker.load( std::move( parsed.get() ) );
        } else {
            compileLibrary( join( srcDir, "pdclib" ), { "-D_PDCLIB_BUILD" } );
            compileLibrary( join( srcDir, "limb" ) );
            std::initializer_list< std::string > cxxflags = { "-std=c++14"
                                                            // , "-fstrict-aliasing"
                                                            , "-I", join( includeDir, "libcxxabi/include" )
                                                            , "-I", join( includeDir, "libcxxabi/src" )
                                                            , "-I", join( includeDir, "libcxx/src" )
                                                            , "-I", join( includeDir, "filesystem" )
                                                            , "-Oz" };
            compileLibrary( join( srcDir, "libcxxabi" ), cxxflags );
            compileLibrary( join( srcDir, "libcxx" ), cxxflags );
            compileLibrary( join( srcDir, "divine" ), cxxflags );
            compileLibrary( join( srcDir, "filesystem" ), cxxflags );
            compileLibrary( join( srcDir, "lart" ), cxxflags );
        }
    }

    void compileLibrary( std::string path, std::initializer_list< std::string > flags = { } )
    {
        for ( const auto &f : mastercc().filesMappedUnder( path ) )
            compileAndLink( f, flags );
    }

    template< typename ... T >
    std::string join( T &&... xs ) { return brick::fs::joinPath( std::forward< T >( xs )... ); }

    const std::string includeDir = "/divine/include";
    const std::string srcDir = "/divine/src";

    std::string precompiled;
    std::vector< Compiler > compilers;
    std::vector< std::thread > workers;
    brick::llvm::Linker linker;
    bool vfs = false, dontLink = false; // FIXME: VFS
    bool libsOnly = false;
    std::string vfsSnapshot;
    std::string vfsInput;
    std::vector< std::string > commonFlags = { "-D__divine__"
                                             , "-isystem", includeDir
                                             , "-isystem", join( includeDir, "pdclib" )
                                             , "-isystem", join( includeDir, "libm" )
                                             , "-isystem", join( includeDir, "libcxx/include" )
                                             , "-D_POSIX_C_SOURCE=2008098L"
                                             , "-D_LITTLE_ENDIAN=1234"
                                             , "-D_BYTE_ORDER=1234"
//                                             , "-fno-slp-vectorize"
//                                             , "-fno-vectorize"
                                             };
    std::string runtimeVersMeta = "divine.compile.runtime.version";
};

}
}
