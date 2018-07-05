// -*- C++ -*- (c) 2016-2017 Vladimír Štill

#include <divine/cc/driver.hpp>

#include <brick-string>
#include <brick-types>

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

Driver::Driver( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    opts( opts ), compiler( ctx ), linker( new brick::llvm::Linker() )
    {
        commonFlags = { "-D__divine__=4"
                      , "-debug-info-kind=standalone"
                      , "-U__x86_64"
                      , "-U__x86_64__"
                      };
    }

Driver::~Driver() { }

std::unique_ptr< llvm::Module > Driver::compile( std::string path,
                                    std::vector< std::string > flags )
{
    return compile( path, typeFromFile( path ), flags );
}

std::unique_ptr< llvm::Module > Driver::compile( std::string path,
                                    FileType type, std::vector< std::string > flags )
{
    std::vector< std::string > allFlags;
    std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
    std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );
    if ( opts.verbose )
        std::cerr << "compiling " << path << std::endl;
    if ( path[0] == '/' )
        compiler.allowIncludePath( std::string( path, 0, path.rfind( '/' ) ) );
    compiler.allowIncludePath( "." ); /* clang 4.0 requires that cwd is always accessible */
    auto mod = compiler.compile( path, type, allFlags );

    return mod;
}

void Driver::runCC( std::vector< std::string > rawCCOpts,
                     std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback )
{
    using FT = FileType;
    auto po = parseOpts( rawCCOpts );

    for ( auto path : po.allowedPaths )
        compiler.allowIncludePath( path );

    if ( !moduleCallback )
        moduleCallback = []( ModulePtr &&m, std::string ) -> ModulePtr {
            return std::move( m );
        };

    for ( auto &f : po.files )
        f.match(
            [&]( const File &f ) {
                compiler.allowIncludePath( brick::string::dirname( f.name ) );
                if ( f.type == FT::Unknown )
                    throw std::runtime_error( "cannot detect file format for file '" + f.name + "', please supply -x option for it" );
                auto m = moduleCallback( compile( f.name, f.type, po.opts ), f.name );
                if ( m )
                    linker->link( std::move( m ) );
            },
            [&]( const Lib &l ) {
                linkLib( l.name, po.libSearchPath );
            } );
}

std::unique_ptr< llvm::Module > Driver::takeLinked()
{
    brick::llvm::verifyModule( linker->get() );
    return linker->take();
}

void Driver::writeToFile( std::string filename ) {
    writeToFile( filename, linker->get() );
}

void Driver::writeToFile( std::string filename, llvm::Module *mod )
{
    brick::llvm::writeModule( mod, filename );
}


std::string Driver::serialize() {
    return compiler.serializeModule( *linker->get() );
}

void Driver::addDirectory( std::string path ) {
    compiler.allowIncludePath( path );
}

void Driver::addFlags( std::vector< std::string > flags ) {
    std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
}

std::shared_ptr< llvm::LLVMContext > Driver::context() { return compiler.context(); }

void Driver::linkLibs( std::vector< std::string > ls, std::vector< std::string > searchPaths ) {
    for ( auto lib : ls )
        linkLib( lib, searchPaths );
}

void Driver::link( ModulePtr mod ) {
   linker->link( std::move( mod ) );
}

brick::llvm::ArchiveReader Driver::getLib( std::string lib, std::vector< std::string > searchPaths ) {
    using namespace brick::fs;

    std::string name;
    searchPaths.push_back( "/dios/lib" );
    for ( auto p : searchPaths )
        for ( auto suf : { "a", "bc" } )
            for ( auto pref : { "lib", "" } )
            {
                auto n = joinPath( p, pref + lib + "."s + suf );
                if ( compiler.fileExists( n ) )
                {
                    name = n;
                    break;
                }
            }

    if ( name.empty() )
        throw std::runtime_error( "Library not found: " + lib );

    auto buf = compiler.getFileBuffer( name );
    if ( !buf )
        throw std::runtime_error( "Cannot open library file: " + name );

    return brick::llvm::ArchiveReader( std::move( buf ), context() );
}

void Driver::linkLib( std::string lib, std::vector< std::string > searchPaths ) {
    auto archive = getLib( lib, std::move( searchPaths ) );
    linker->linkArchive( archive );
}

void Driver::linkArchive( std::unique_ptr< llvm::MemoryBuffer > buf, std::shared_ptr< llvm::LLVMContext > context )
{
    auto archive = brick::llvm::ArchiveReader( std::move( buf ), context );
    linker->linkArchive( archive );
}

void Driver::linkEntireArchive( std::string arch )
{
        auto archive = getLib( arch );
        auto modules = archive.modules();
        for ( auto it = modules.begin(); it != modules.end(); ++it )
            linker->link( it.take() );
}

}
}
