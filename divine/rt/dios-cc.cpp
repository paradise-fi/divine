#include <divine/rt/dios-cc.hpp>

namespace divine {
namespace rt {

using namespace cc;

void DiosCC::link_dios_config( std::string n )
{
    linkEntireArchive( "rst" );
    auto mod = load_object( "config/" + n );
    link( std::move( mod ) );
    for ( int i = 0; i < 3; ++i )
    {
        linkLib( "dios" );
        linkLib( "dios_divm" );
        linkLibs( rt::DiosCC::defaultDIVINELibs );
    }
}

DiosCC::DiosCC( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    Driver( opts, ctx )
{
    using brick::fs::joinPath;
    setupFS( rt::each );

    commonFlags.insert( commonFlags.end(),
                 { "-isystem", joinPath( includeDir, "libcxx/include" )
                 , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                 , "-isystem", joinPath( includeDir, "libunwind/include" )
                 , "-isystem", includeDir
                 , "-D_LITTLE_ENDIAN=1234"
                 , "-D_BYTE_ORDER=1234"
                 } );
}

void DiosCC::build( ParsedOpts po )
{
    Driver::build( po );

    if ( linker->hasModule() )
        linkLibs( defaultDIVINELibs );
}

const std::vector< std::string > DiosCC::defaultDIVINELibs = { "c++", "c++abi", "pthread", "c" };

} // cc
} // divine
