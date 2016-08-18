// -*- C++ -*- (c) 2016 Vladimír Štill

#include <divine/cc/compile.hpp>
#include <divine/cc/runtime.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_os_ostream.h>
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-string>

extern const char *DIVINE_RUNTIME_SHA;

namespace divine {
namespace cc {

using brick::fs::joinPath;

std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value ) {
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
    return ss.str();
}

static std::string getWrappedMDS( llvm::NamedMDNode *meta, int i = 0, int j = 0 ) {
    auto *op = llvm::cast< llvm::MDTuple >( meta->getOperand( i ) );
    auto *str = llvm::cast< llvm::MDString >( op->getOperand( j ).get() );
    return str->getString().str();
}

struct MergeFlags_ { // hide in clas so they can be mutually recursive
    void operator()( std::vector< std::string > & ) { }

    template< typename ... Xs >
    void operator()( std::vector< std::string > &out, std::string first, Xs &&... xs ) {
        out.emplace_back( std::move( first ) );
        (*this)( out, std::forward< Xs >( xs )... );
    }

    template< typename C, typename ... Xs >
    auto operator()( std::vector< std::string > &out, C &&c, Xs &&... xs )
        -> decltype( void( (c.begin(), c.end()) ) )
    {
        out.insert( out.end(), c.begin(), c.end() );
        (*this)( out, std::forward< Xs >( xs )... );
    }
};

template< typename ... Xs >
static std::vector< std::string > mergeFlags( Xs &&... xs ) {
    std::vector< std::string > out;
    MergeFlags_()( out, std::forward< Xs >( xs )... );
    return out;
}

Compile::Compile( Options opts ) :
    opts( opts ), compilers( 1 ), workers( 1 ), linker( new brick::llvm::Linker() )
{
    ASSERT_LEQ( 1ul, workers.size() );
    ASSERT_EQ( workers.size(), compilers.size() );

    commonFlags = { "-D__divine__"
                  , "-isystem", runtime::includeDir
                  , "-isystem", joinPath( runtime::includeDir, "pdclib" )
                  , "-isystem", joinPath( runtime::includeDir, "libm" )
                  , "-isystem", joinPath( runtime::includeDir, "libcxx/include" )
                  , "-isystem", joinPath( runtime::includeDir, "libunwind/include" )
                  , "-D_POSIX_C_SOURCE=2008098L"
                  , "-D_LITTLE_ENDIAN=1234"
                  , "-D_BYTE_ORDER=1234"
                  , "-g"
                  };

    setupFS();
    if ( !opts.dont_link )
        setupLibs();
}

Compile::~Compile() { }

void Compile::compileAndLink( std::string path, std::vector< std::string > flags )
{
    linker->link( compile( path, flags ) );
}

std::unique_ptr< llvm::Module > Compile::compile( std::string path,
                                    std::vector< std::string > flags )
{
    std::vector< std::string > allFlags;
    std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
    std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );

    std::cerr << "compiling " << path << std::endl;
    auto mod = mastercc().compileModule( path, allFlags );
    tagWithRuntimeVersionSha( *mod );

    return mod;
}

llvm::Module *Compile::getLinked() {
    tagWithRuntimeVersionSha( *linker->get() );
    return linker->get();
}

void Compile::writeToFile( std::string filename ) {
    writeToFile( filename, getLinked() );
}

void Compile::writeToFile( std::string filename, llvm::Module *mod )
{
    llvm::raw_os_ostream cerr( std::cerr );
    if ( llvm::verifyModule( *mod, &cerr ) ) {
        cerr.flush(); // otherwise nothing will be displayed
        UNREACHABLE( "invalid bitcode" );
    }
    std::error_code serr;
    ::llvm::raw_fd_ostream outs( filename, serr, ::llvm::sys::fs::F_None );
    WriteBitcodeToFile( mod, outs );
}

std::string Compile::serialize() {
    return mastercc().serializeModule( *getLinked() );
}

void Compile::addDirectory( std::string path ) {
    mastercc().allowIncludePath( path );
}

void Compile::addFlags( std::vector< std::string > flags ) {
    std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
}

void Compile::prune( std::vector< std::string > r ) {
    linker->prune( r, brick::llvm::Prune::UnusedModules );
}

void Compile::tagWithRuntimeVersionSha( llvm::Module &m ) const {
    auto *meta = m.getNamedMetadata( runtimeVersMeta );
    // check old metadata
    std::set< std::string > found;
    found.emplace( DIVINE_RUNTIME_SHA );
    if ( meta ) {
        for ( unsigned i = 0; i < meta->getNumOperands(); ++i )
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

std::string Compile::getRuntimeVersionSha( llvm::Module &m ) const {
    auto *meta = m.getNamedMetadata( runtimeVersMeta );
    if ( !meta )
        return "";
    return getWrappedMDS( meta );
}

std::shared_ptr< llvm::LLVMContext > Compile::context() { return mastercc().context(); }

Compiler &Compile::mastercc() { return compilers[0]; }

void Compile::setupFS()
{
    using brick::string::startsWith;

    runtime::each(
        [&]( auto path, auto c )
        {
            if ( startsWith( path, joinPath( runtime::srcDir, "filesystem" ) ) )
                return; /* ignore */
            mastercc().mapVirtualFile( path, c );
        } );
}

void Compile::setupLibs() {
    if ( opts.precompiled.size() ) {
        auto input = std::move( llvm::MemoryBuffer::getFile( opts.precompiled ).get() );
        ASSERT( !!input );

        auto inputData = input->getMemBufferRef();
        auto parsed = parseBitcodeFile( inputData, *context() );
        if ( !parsed )
            throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
        if ( getRuntimeVersionSha( *parsed.get() ) != DIVINE_RUNTIME_SHA )
            std::cerr << "WARNING: runtime version of the precompiled library does not match the current runtime version"
                      << std::endl;
        linker->load( std::move( parsed.get() ) );
    } else {
        auto libflags = []( auto... xs ) {
            return mergeFlags( "-Wall", "-Wextra", "-Wno-gcc-compat", "-Wno-unused-parameter", xs... );
        };
        std::initializer_list< std::string > cxxflags =
            { "-std=c++14"
              // , "-fstrict-aliasing"
              , "-I", joinPath( runtime::includeDir, "libcxxabi/include" )
              , "-I", joinPath( runtime::includeDir, "libcxxabi/src" )
              , "-I", joinPath( runtime::includeDir, "libcxx/src" )
              , "-I", joinPath( runtime::includeDir, "filesystem" )
              , "-Oz" };
        compileLibrary( joinPath( runtime::srcDir, "pdclib" ), libflags( "-D_PDCLIB_BUILD" ) );
        compileLibrary( joinPath( runtime::srcDir, "limb" ), libflags() );
        compileLibrary( joinPath( runtime::srcDir, "libcxxabi" ),
                        libflags( cxxflags, "-DLIBCXXABI_USE_LLVM_UNWINDER" ) );
        compileLibrary( joinPath( runtime::srcDir, "libcxx" ), libflags( cxxflags ) );
        compileLibrary( joinPath( runtime::srcDir, "divine" ), libflags( cxxflags ) );
        compileLibrary( joinPath( runtime::srcDir, "filesystem" ), libflags( cxxflags ) );
        compileLibrary( joinPath( runtime::srcDir, "lart" ), libflags( cxxflags ) );
    }
}

void Compile::compileLibrary( std::string path, std::vector< std::string > flags )
{
    for ( const auto &f : mastercc().filesMappedUnder( path ) )
        compileAndLink( f, flags );
}
}
}
