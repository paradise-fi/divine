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
using namespace std::literals;

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

Compile::Compile( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    opts( opts ), compiler( ctx ), linker( new brick::llvm::Linker() )
{
    commonFlags = { "-D__divine__=4"
                  , "-isystem", rt::includeDir
                  , "-isystem", joinPath( rt::includeDir, "pdclib" )
                  , "-isystem", joinPath( rt::includeDir, "libtre" )
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

void Compile::compileAndLink( std::string path, Compiler::FileType type, std::vector< std::string > flags )
{
    linker->link( compile( path, type, flags ) );
}

std::unique_ptr< llvm::Module > Compile::compile( std::string path,
                                    std::vector< std::string > flags )
{
    return compile( path, Compiler::typeFromFile( path ), flags );
}

std::unique_ptr< llvm::Module > Compile::compile( std::string path,
                                    Compiler::FileType type,
                                    std::vector< std::string > flags )
{
    std::vector< std::string > allFlags;
    std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
    std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );
    if ( opts.verbose )
        std::cerr << "compiling " << path << std::endl;
    if ( path[0] == '/' )
        compiler.allowIncludePath( std::string( path, 0, path.rfind( '/' ) ) );
    auto mod = compiler.compileModule( path, type, allFlags );

    return mod;
}

void Compile::runCC( std::vector< std::string > rawCCOpts,
                     std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback )
{
    using FT = Compiler::FileType;
    FT xType = FT::Unknown;

    std::vector< std::string > opts;
    std::vector< std::pair< std::string, FT > > files;

    for ( auto it = rawCCOpts.begin(), end = rawCCOpts.end(); it != end; ++it )
    {
        std::string isystem = "-isystem";
        if ( brick::string::startsWith( *it, "-I" ) ) {
            std::string val;
            if ( it->size() > 2 )
                val = it->substr( 2 );
            else
                val = *++it;
            compiler.allowIncludePath( val );
            opts.emplace_back( "-I" + val );
        }
        else if ( brick::string::startsWith( *it, isystem ) ) {
            std::string val;
            if ( it->size() > isystem.size() )
                val = it->substr( isystem.size() );
            else
                val = *++it;
            compiler.allowIncludePath( val );
            opts.emplace_back( isystem + val );
        }
        else if ( brick::string::startsWith( *it, "-x" ) ) {
            std::string val;
            if ( it->size() > 2 )
                val = it->substr( 2 );
            else
                val = *++it;
            if ( val == "none" )
                xType = FT::Unknown;
            else {
                xType = Compiler::typeFromXOpt( val );
                if ( xType == FT::Unknown )
                    throw std::runtime_error( "-x value not recognized: " + val );
            }
            // this option is propagated to CC by xType, not directly
        }
        else if ( !brick::string::startsWith( *it, "-" ) && brick::fs::access( *it, F_OK ) )
            files.emplace_back( *it, xType == FT::Unknown ? Compiler::typeFromFile( *it ) : xType );
        else
            opts.emplace_back( *it );
    }

    if ( !moduleCallback )
        moduleCallback = []( ModulePtr &&m, std::string ) -> ModulePtr {
            return std::move( m );
        };

    for ( auto &f : files ) {
        auto m = moduleCallback( compile( f.first, f.second, opts ), f.first );
        if ( m )
            linker->link( std::move( m ) );
    }
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
    return compiler.serializeModule( *getLinked() );
}

void Compile::addDirectory( std::string path ) {
    compiler.allowIncludePath( path );
}

void Compile::addFlags( std::vector< std::string > flags ) {
    std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
}

void Compile::prune( std::vector< std::string > r ) {
    linker->prune( r, brick::llvm::Prune::UnusedModules );
}

std::shared_ptr< llvm::LLVMContext > Compile::context() { return compiler.context(); }

void Compile::setupLib( std::string name, const std::string &content )
{
    if ( opts.verbose )
        std::cerr << "loading " << name << "..." << std::flush;
    auto input = llvm::MemoryBuffer::getMemBuffer( content );
    auto parsed = parseBitcodeFile( input->getMemBufferRef(), *context() );
    if ( !parsed )
        throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
    if ( opts.verbose )
        std::cerr << " linking..." << std::flush;
    linker->link( std::move( parsed.get() ), false );
    if ( opts.verbose )
        std::cerr << " done" << std::endl;
}

void Compile::compileLibrary( std::string path, std::vector< std::string > flags )
{
    for ( const auto &f : compiler.filesMappedUnder( path ) )
        compileAndLink( f, flags );
}
}
}
