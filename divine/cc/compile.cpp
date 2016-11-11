// -*- C++ -*- (c) 2016 Vladimír Štill

#include <divine/cc/compile.hpp>
#include <divine/rt/runtime.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_os_ostream.h>
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-string>

namespace divine {
namespace cc {

using brick::fs::joinPath;

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

    commonFlags = { "-D__divine__=4"
                  , "-isystem", rt::includeDir
                  , "-isystem", joinPath( rt::includeDir, "pdclib" )
                  , "-isystem", joinPath( rt::includeDir, "libm" )
                  , "-isystem", joinPath( rt::includeDir, "libcxx/include" )
                  , "-isystem", joinPath( rt::includeDir, "libcxxabi/include" )
                  , "-isystem", joinPath( rt::includeDir, "libunwind/include" )
                  , "-D_POSIX_C_SOURCE=2008098L"
                  , "-D_LITTLE_ENDIAN=1234"
                  , "-D_BYTE_ORDER=1234"
                  , "-g"
                  };
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
    if ( path[0] == '/' )
        mastercc().allowIncludePath( std::string( path, 0, path.rfind( '/' ) ) );
    auto mod = mastercc().compileModule( path, allFlags );

    return mod;
}

llvm::Module *Compile::getLinked() {
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

std::shared_ptr< llvm::LLVMContext > Compile::context() { return mastercc().context(); }

Compiler &Compile::mastercc() { return compilers[0]; }

void Compile::setupLib( std::string name, const std::string &content )
{
    std::cerr << "loading " << name << "..." << std::flush;
    auto input = llvm::MemoryBuffer::getMemBuffer( content );
    auto parsed = parseBitcodeFile( input->getMemBufferRef(), *context() );
    if ( !parsed )
        throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
    std::cerr << " linking..." << std::flush;
    linker->link( std::move( parsed.get() ), false );
    std::cerr << " done" << std::endl;
}

void Compile::compileLibrary( std::string path, std::vector< std::string > flags )
{
    for ( const auto &f : mastercc().filesMappedUnder( path ) )
        compileAndLink( f, flags );
}
}
}
